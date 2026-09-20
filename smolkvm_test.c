#include "smolkvm.h"
#include "ipl/include/params.h"
#include "ipl/include/loadkernel.h"

#ifndef NOLIBC
#include <unistd.h>
#endif

#define MAILBOX_CMD_GETPARAMS (SMOLKVM_MAILBOX_CMD_MINUSER + 0)
#define MAILBOX_CMD_LOADKERNEL (SMOLKVM_MAILBOX_CMD_MINUSER + 1)

#define DEFAULT_RAM_BASE	0x400000ULL
#define DEFAULT_RAM_MB		64
#define DEFAULT_CMDLINE		"console=ttyS0 earlycon=uart8250,io,0x3f8"

/* Everything the mailbox handlers need to know about the machine we build */
struct testvm_cfg {
	uint64_t ram_base;
	uint64_t ram_sz;
	const char *kernel_path;
	const char *initrd_path;
	const char *cmdline;
	uint64_t initrd_base;
	uint64_t initrd_sz;
};

static uint64_t get_params_fn(struct smolkvm_vm *vm, uint64_t command, void *buffer, void *priv)
{
	struct cmd_buffer_getparams *getparams = buffer;
	const struct testvm_cfg *cfg = priv;
	struct ipl_params params = { 0 };

	printf("Guest asked for parameters\n");
	params.ram_base = cfg->ram_base;
	params.ram_sz = cfg->ram_sz;
	params.initrd_base = cfg->initrd_base;
	params.initrd_sz = cfg->initrd_sz;
	strncpy(params.cmdline, cfg->cmdline, sizeof(params.cmdline) - 1);

	smolkvm_guest_write(vm, getparams->params_ptr, sizeof(params), &params);

	return 0;
}

/* Read `path` whole into guest memory at `gpa`; returns bytes loaded or -1 */
static int64_t load_file_into_guest(struct smolkvm_vm *vm, const char *path, uint64_t gpa)
{
	struct stat st;
	void *image;
	ssize_t got;
	int ret;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("could not open %s\n", path);
		return -1;
	}

	if (fstat(fd, &st) < 0 || st.st_size <= 0) {
		printf("could not stat %s\n", path);
		close(fd);
		return -1;
	}

	image = malloc((size_t) st.st_size);
	if (!image) {
		close(fd);
		return -1;
	}

	got = read(fd, image, (size_t) st.st_size);
	close(fd);
	if (got != st.st_size) {
		free(image);
		return -1;
	}

	ret = smolkvm_guest_write(vm, gpa, (uint64_t) st.st_size, image);
	free(image);
	if (ret) {
		printf("could not write %s into guest memory at 0x%llx\n",
		       path, (unsigned long long) gpa);
		return -1;
	}

	return st.st_size;
}

static uint64_t load_kernel_fn(struct smolkvm_vm *vm, uint64_t command, void *buffer, void *priv)
{
	struct cmd_buf_loadkernel *loadkernel = buffer;
	const struct testvm_cfg *cfg = priv;
	uint64_t entry = 0;

	printf("Guest asked for kernel to be loaded\n");

	if (!cfg->kernel_path) {
		printf("No kernel path set!\n");
		return SMOLKVM_MAILBOX_STATUS_ERR;
	}

	if (smolkvm_load_elf_file(vm, cfg->kernel_path, &entry)) {
		printf("Loading the kernel failed\n");
		return SMOLKVM_MAILBOX_STATUS_ERR;
	}
	printf("Kernel entry point: 0x%llx\n", (unsigned long long) entry);

	if (cfg->initrd_sz) {
		if (load_file_into_guest(vm, cfg->initrd_path, cfg->initrd_base) < 0)
			return SMOLKVM_MAILBOX_STATUS_ERR;
		printf("initrd: 0x%llx bytes at 0x%llx\n",
		       (unsigned long long) cfg->initrd_sz,
		       (unsigned long long) cfg->initrd_base);
	}

	smolkvm_guest_write(vm, loadkernel->entry_ptr, sizeof(entry), &entry);

	return 0;
}

#ifdef SMOLKVM_WANT_VIRTIO_GPU
/* "1024x768" -> 1024, 768. Returns 0 on success. */
static int parse_display_size(const char *str, int *width, int *height)
{
	const char *x = strchr(str, 'x');

	if (!x || x == str || !x[1])
		return -1;

	*width = atoi(str);
	*height = atoi(x + 1);

	if (*width <= 0 || *height <= 0)
		return -1;

	return 0;
}

/*
 * Length of the command line up to a standalone "--", or the whole thing if
 * there is not one. Linux stops reading its own parameters at "--" and hands
 * everything after it to init, so a fragment tacked on the end of a command
 * line that has one is passed to init as an argument and the kernel never
 * sees it. It has to go in front.
 */
static size_t cmdline_kernel_len(const char *cmdline)
{
	size_t i;

	for (i = 0; cmdline[i]; i++) {
		/* Only a "--" on its own is the separator, not one inside a word */
		if (cmdline[i] != '-' || cmdline[i + 1] != '-')
			continue;
		if (i && cmdline[i - 1] != ' ')
			continue;
		if (cmdline[i + 2] && cmdline[i + 2] != ' ')
			continue;

		return i;
	}

	return i;
}

/*
 * The guest only learns the virtio devices exist from the command line, so
 * glue their description of themselves onto whatever was asked for rather than
 * making every caller remember a pile of addresses.
 */
static const char *cmdline_with_virtio(const char *cmdline)
{
	static char full[512];
	size_t kernel_len = cmdline_kernel_len(cmdline);

	snprintf(full, sizeof(full), "%.*s%s%s %s",
		 (int) kernel_len, cmdline,
		 kernel_len ? " " : "",
		 smolkvm_virtio_cmdline(),
		 cmdline + kernel_len);

	return full;
}
#endif

static void usage(const char *argv0)
{
	fprintf(stderr,
		"usage: %s [-i ipl] [-k vmlinux] [-r initrd] [-c cmdline] [-m ram_mb] [-h header_out]\n"
		"  -k  ELF kernel (vmlinux, NOT bzImage) to boot\n"
		"  -r  initrd/initramfs image, loaded at the top of RAM\n"
		"  -c  kernel command line (default: \"%s\")\n"
		"  -m  system RAM size in MB (default: %d)\n"
		"  -h  dump the generated machine header to a file and exit\n"
#ifdef SMOLKVM_WANT_VIRTIO_GPU
		"  -g  display size as WIDTHxHEIGHT (default: %dx%d)\n"
		"  -p  port for the VNC server, on loopback (default: %d)\n"
#endif
		, argv0, DEFAULT_CMDLINE, DEFAULT_RAM_MB
#ifdef SMOLKVM_WANT_VIRTIO_GPU
		, SMOLKVM_VIRTIO_GPU_WIDTH, SMOLKVM_VIRTIO_GPU_HEIGHT,
		SMOLKVM_VIRTIO_GPU_PORT
#endif
		);
}

int main(int argc, char **argv, char **envp)
{
	struct cmd_buf_loadkernel loadkernelparams = { 0 };
	struct cmd_buffer_getparams getparams = { 0 };
	struct testvm_cfg cfg = {
		.ram_base = DEFAULT_RAM_BASE,
		.ram_sz = DEFAULT_RAM_MB * SMOLKVM_SZ_1MB,
		.cmdline = DEFAULT_CMDLINE,
	};
	const char *ipl_path = "ipl/build/ipl";
	const char *header_path = NULL;
	struct smolkvm_vm vm = { 0 };
	uint64_t ipl_entry = 0;
#ifdef SMOLKVM_WANT_VIRTIO_GPU
	int gpu_width = SMOLKVM_VIRTIO_GPU_WIDTH;
	int gpu_height = SMOLKVM_VIRTIO_GPU_HEIGHT;
	int gpu_port = SMOLKVM_VIRTIO_GPU_PORT;
#endif
	int ret;
	int opt;

	while ((opt = getopt(argc, argv, "h:i:k:r:c:m:g:p:")) != -1) {
		switch (opt) {
		case 'h':
			header_path = optarg;
			break;
#ifdef SMOLKVM_WANT_VIRTIO_GPU
		case 'g':
			if (parse_display_size(optarg, &gpu_width, &gpu_height)) {
				fprintf(stderr, "could not read \"%s\" as WIDTHxHEIGHT\n",
					optarg);
				return 1;
			}
			break;
		case 'p':
			gpu_port = atoi(optarg);
			break;
#endif
		case 'i':
			ipl_path = optarg;
			break;
		case 'k':
			cfg.kernel_path = optarg;
			break;
		case 'r':
			cfg.initrd_path = optarg;
			break;
		case 'c':
			cfg.cmdline = optarg;
			break;
		case 'm':
			cfg.ram_sz = (uint64_t) atoi(optarg) * SMOLKVM_SZ_1MB;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	/* The IPL's identity map is one page directory: 1GB is the hard limit */
	if (cfg.ram_base + cfg.ram_sz > SMOLKVM_SZ_1MB * 1024) {
		fprintf(stderr, "RAM is capped at 1GB (the IPL maps a single PD)\n");
		cfg.ram_sz = SMOLKVM_SZ_1MB * 1024 - cfg.ram_base;
	}

	if (cfg.initrd_path) {
		struct stat st;

		if (stat(cfg.initrd_path, &st) || st.st_size <= 0) {
			fprintf(stderr, "could not stat initrd %s\n", cfg.initrd_path);
			return 1;
		}

		/* Top of RAM, page aligned down */
		cfg.initrd_sz = (uint64_t) st.st_size;
		cfg.initrd_base = (cfg.ram_base + cfg.ram_sz - cfg.initrd_sz)
				  & ~(SMOLKVM_SZ_4K - 1);
	}

#ifdef SMOLKVM_WANT_VIRTIO_GPU
	/* Both of these have to be settled before the VM (and the display) exists */
	smolkvm_virtio_gpu_configure(gpu_width, gpu_height, NULL, gpu_port);
	cfg.cmdline = cmdline_with_virtio(cfg.cmdline);
#endif

	printf("Built %s @ %s\n", __DATE__, __TIME__);

	/*
	 * Dumping the machine header only reads compile-time constants, so do
	 * it before (and instead of) creating the VM: this path must work on a
	 * build host with no /dev/kvm, e.g. CI generating ipl/include/machine.h.
	 */
	if (header_path) {
		FILE *header = fopen(header_path, "w");
		if (!header) {
			fprintf(stderr, "could not open %s for writing\n", header_path);
			return 1;
		}

		smolkvm_dump_register_header_novm(header);
		fclose(header);
		return 0;
	}

	ret = smolkvm_create_vm(&vm);
	if (ret) {
		printf("smolkvm_create_vm() failed: %d\n", ret);
		return 1;
	}

	ret = smolkvm_mailbox_register(&vm, MAILBOX_CMD_GETPARAMS, &getparams,
				       sizeof(getparams), get_params_fn, &cfg);
	if (ret)
		return 1;

	ret = smolkvm_mailbox_register(&vm, MAILBOX_CMD_LOADKERNEL, &loadkernelparams,
				       sizeof(loadkernelparams), load_kernel_fn, &cfg);
	if (ret)
		return 1;

	smolkvm_dump_memory_map(&vm);

	ret = smolkvm_load_elf_file(&vm, ipl_path, &ipl_entry);
	if (ret) {
		printf("Loading the IPL from %s failed\n", ipl_path);
		return 1;
	}
	printf("IPL entry point: 0x%llx\n", (unsigned long long) ipl_entry);

#ifdef SMOLKVM_WANT_GDB_STUB
	__smolkvm_gdb_stub_start(&vm);
	ret = __smolkvm_gdb_stub_accept(&vm);
	if (ret) {
		printf("GDB stub accept failed: %d\n", ret);
		return 1;
	}
#endif

	smolkvm_default_loop(&vm);

	printf("Destroying vm\n");
	smolkvm_destroy_vm(&vm);

	return 0;
}
