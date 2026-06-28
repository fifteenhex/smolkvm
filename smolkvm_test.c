#include "smolkvm.h"
#include "ipl/include/params.h"
#include "ipl/include/loadkernel.h"

#include <unistd.h>

#define MAILBOX_CMD_GETPARAMS (SMOLKVM_MAILBOX_CMD_MINUSER + 0)
#define MAILBOX_CMD_LOADKERNEL (SMOLKVM_MAILBOX_CMD_MINUSER + 1)

static uint64_t get_params_fn(struct smolkvm_vm *vm, uint64_t command, void *buffer, void *priv)
{
	printf("Guest asked for parameters\n");
	return 0;
}

static uint64_t load_kernel_fn(struct smolkvm_vm *vm, uint64_t command, void *buffer, void *priv)
{
	struct cmd_buf_loadkernel *loadkernel = buffer;
	const char *kernel_path = priv;
	uint64_t entry = 0;

	printf("Guest asked for kernel to be loaded\n");

	if (!kernel_path) {
		printf("No kernel path set!\n");
		return 0;
	}

	smolkvm_load_elf_file(vm, kernel_path, &entry);
	printf("Kernel entry point: 0x%llx\n", (unsigned long long) entry);

	smolkvm_guest_write(vm, loadkernel->entry_ptr, sizeof(loadkernel->entry_ptr), &entry);

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	struct cmd_buf_loadkernel loadkernelparams = { 0 };
	struct cmd_buffer_getparams getparams = { 0 };
	const char *ipl_path = "ipl/build/ipl";
	const char *kernel_path = NULL;
	const char *header_path = NULL;
	struct smolkvm_vm vm = { 0 };
	uint64_t ipl_entry = 0;
	int ret;
	int opt;

	while ((opt = getopt(argc, argv, "h:i:k:")) != -1) {
		switch (opt) {
		case 'h':
			header_path = optarg;
			break;
		case 'i':
			ipl_path = optarg;
			break;
		case 'k':
			kernel_path = optarg;
			break;
		default:
			fprintf(stderr, "usage: %s [-h header_out]\n", argv[0]);
			return 1;
		}
	}

	printf("Built %s @ %s\n", __DATE__, __TIME__);

	ret = smolkvm_create_vm(&vm);
	if (ret) {
		printf("smolkvm_create_vm() failed: %d\n", ret);
		return 1;
	}

	if (header_path) {
		FILE *header = fopen(header_path, "w");
		if (!header) {
			fprintf(stderr, "could not open %s for writing\n", header_path);
			smolkvm_destroy_vm(&vm);
			return 1;
		}

		smolkvm_dump_register_header(&vm, header);
		fclose(header);
		smolkvm_destroy_vm(&vm);
		return 0;
	}

	ret = smolkvm_mailbox_register(&vm, MAILBOX_CMD_GETPARAMS, &getparams,
				       sizeof(getparams), get_params_fn, NULL);
	if (ret)
		return 1;

	ret = smolkvm_mailbox_register(&vm, MAILBOX_CMD_LOADKERNEL, &loadkernelparams,
				       sizeof(loadkernelparams), load_kernel_fn, kernel_path);
	if (ret)
		return 1;

	smolkvm_dump_memory_map(&vm);

	ret = smolkvm_load_elf_file(&vm, ipl_path, &ipl_entry);
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
