// SPDX-License-Identifier: GPL-3.0-or-later
// kate: tab-width 8;

/*
 * Drives smolkvm's virtio-gpu device model directly, with no KVM involved:
 * builds a split virtqueue in a fake memslot, issues the command sequence
 * Linux's drm/virtio driver issues, and checks what came back -- then connects
 * a from-scratch RFB client and checks the pixels that reach a viewer are the
 * ones the "guest" drew.
 *
 * Booting a real kernel is the real proof, but that needs /dev/kvm, which CI
 * does not have. Everything here is a plain function call into the header, so
 * it runs anywhere it compiles: the ring walk, the scatter-gather reader, the
 * clipping, and what happens when the guest lies.
 *
 *	make smolkvm_gpu_selftest SMOLRFBDIR=/path/to/smolrfb && ./smolkvm_gpu_selftest
 */
#include "smolkvm.h"

#define RAM_BASE	0x400000ULL
#define RAM_SZ		(16 * 1024 * 1024)

#define QSZ		16

#define DESC_GPA	(RAM_BASE + 0x0000)
#define AVAIL_GPA	(RAM_BASE + 0x1000)
#define USED_GPA	(RAM_BASE + 0x2000)
#define REQ_GPA		(RAM_BASE + 0x3000)
#define RESP_GPA	(RAM_BASE + 0x4000)
#define BACKING_GPA	(RAM_BASE + 0x10000)

#define RES_W		64
#define RES_H		32

static struct smolkvm_vm vm;
static int failures;

#define CHECK(_cond, _what) do {					\
	if (!(_cond)) {							\
		printf("FAIL: %s\n", _what);				\
		failures++;						\
	} else {							\
		printf("ok:   %s\n", _what);				\
	}								\
} while (0)

static void *gpa(uint64_t addr)
{
	return __smolkvm_memory_ptr(&vm, addr, 1);
}

static void mmio_write(uint64_t off, uint32_t val)
{
	__smolkvm_virtio_gpu.write(&vm, &__smolkvm_virtio_gpu, off, 4, val);
}

static uint32_t mmio_read(uint64_t off)
{
	return (uint32_t) __smolkvm_virtio_gpu.read(&vm, &__smolkvm_virtio_gpu, off, 4);
}

/* Put a two descriptor chain (request out, response in) in slot 0 and notify */
static void submit(const void *req, size_t req_len, size_t resp_len)
{
	struct __smolkvm_vring_desc *desc = gpa(DESC_GPA);
	uint16_t *avail = gpa(AVAIL_GPA);

	memcpy(gpa(REQ_GPA), req, req_len);
	memset(gpa(RESP_GPA), 0, resp_len);

	desc[0].addr = REQ_GPA;
	desc[0].len = req_len;
	desc[0].flags = __SMOLKVM_VRING_DESC_F_NEXT;
	desc[0].next = 1;

	desc[1].addr = RESP_GPA;
	desc[1].len = resp_len;
	desc[1].flags = __SMOLKVM_VRING_DESC_F_WRITE;
	desc[1].next = 0;

	avail[2 + (avail[1] % QSZ)] = 0;
	avail[1]++;

	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_NOTIFY, 0);
}

static uint32_t resp_type(void)
{
	const struct __smolkvm_virtio_gpu_ctrl_hdr *hdr = gpa(RESP_GPA);

	return hdr->type;
}

static void hdr_init(struct __smolkvm_virtio_gpu_ctrl_hdr *hdr, uint32_t type)
{
	memset(hdr, 0, sizeof(*hdr));
	hdr->type = type;
}

/* Let the device service its RFB clients, the way pre_run does in the loop */
static void pump(int times)
{
	while (times--)
		__smolkvm_virtio_gpu.pre_run(&vm, &__smolkvm_virtio_gpu);
}

/* Read exactly `len` bytes, pumping the server in between so it can answer */
static int cread(int fd, void *buf, size_t len)
{
	uint8_t *p = buf;
	size_t got = 0;
	int spins = 0;

	while (got < len) {
		ssize_t r;

		pump(1);

		r = recv(fd, p + got, len - got, MSG_DONTWAIT);
		if (r > 0) {
			got += (size_t) r;
			spins = 0;
			continue;
		}
		if (r == 0)
			return -1;
		if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
			return -1;
		if (++spins > 2000)
			return -1;
		usleep(1000);
	}

	return 0;
}

/*
 * Connect a from-scratch RFB client, ask for the whole screen and check the
 * pixels that come back are the ones in the framebuffer.
 */
static void rfb_client_check(struct __smolkvm_virtio_gpu_priv *g)
{
	struct sockaddr_in addr = { 0 };
	uint8_t hdr[24];
	uint8_t req[10];
	uint32_t *pix;
	uint32_t namelen;
	int fd;
	int w, h;
	int i;
	int bad;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(15900);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	/*
	 * The listen socket is armed for SIGIO, so our own connect() gets
	 * interrupted by the signal it causes. On loopback the connection is
	 * already made by then, so EINTR here is success.
	 */
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) && errno != EINTR) {
		CHECK(0, "a viewer can connect");
		close(fd);
		return;
	}
	pump(4);
	CHECK(smolrfb_clients(&g->rfb) == 1, "the server sees the viewer");

	/* version */
	if (cread(fd, hdr, 12)) {
		CHECK(0, "the server offers a version");
		close(fd);
		return;
	}
	CHECK(memcmp(hdr, "RFB 003.008\n", 12) == 0, "the server offers RFB 3.8");
	send(fd, "RFB 003.008\n", 12, 0);

	/* security */
	if (cread(fd, hdr, 1) || cread(fd, hdr + 1, hdr[0])) {
		CHECK(0, "the server offers a security type");
		close(fd);
		return;
	}
	CHECK(hdr[1] == 1, "the only security type offered is None");
	hdr[0] = 1;
	send(fd, hdr, 1, 0);
	cread(fd, hdr, 4);		/* SecurityResult */

	/* ClientInit, shared */
	hdr[0] = 1;
	send(fd, hdr, 1, 0);

	if (cread(fd, hdr, 24)) {
		CHECK(0, "the server sends a ServerInit");
		close(fd);
		return;
	}
	w = (hdr[0] << 8) | hdr[1];
	h = (hdr[2] << 8) | hdr[3];
	CHECK(w == RES_W && h == RES_H, "ServerInit reports the display size");
	CHECK(hdr[4] == 32 && hdr[7] == 1, "ServerInit offers 32bpp true colour");

	namelen = ((uint32_t) hdr[20] << 24) | ((uint32_t) hdr[21] << 16)
		| ((uint32_t) hdr[22] << 8) | hdr[23];
	{
		uint8_t name[SMOLRFB_NAME_SZ];

		cread(fd, name, namelen < sizeof(name) ? namelen : sizeof(name));
	}

	/* FramebufferUpdateRequest, non-incremental, the whole screen */
	req[0] = 3;
	req[1] = 0;
	req[2] = 0; req[3] = 0;
	req[4] = 0; req[5] = 0;
	req[6] = (uint8_t) (RES_W >> 8); req[7] = (uint8_t) RES_W;
	req[8] = (uint8_t) (RES_H >> 8); req[9] = (uint8_t) RES_H;
	send(fd, req, sizeof(req), 0);

	if (cread(fd, hdr, 4)) {
		CHECK(0, "a framebuffer update arrives");
		close(fd);
		return;
	}
	CHECK(hdr[0] == 0 && ((hdr[2] << 8) | hdr[3]) == 1,
	      "the update holds one rectangle");

	if (cread(fd, hdr, 12)) {
		CHECK(0, "the rectangle has a header");
		close(fd);
		return;
	}
	w = (hdr[4] << 8) | hdr[5];
	h = (hdr[6] << 8) | hdr[7];
	CHECK(w == RES_W && h == RES_H, "the rectangle covers the whole screen");

	pix = malloc((size_t) w * h * 4);
	if (cread(fd, pix, (size_t) w * h * 4)) {
		CHECK(0, "the pixels arrive");
		free(pix);
		close(fd);
		return;
	}

	/* Raw, 32bpp, exactly what the framebuffer holds, bar the padding byte */
	bad = 0;
	for (i = 0; i < w * h; i++)
		if ((pix[i] & 0x00FFFFFF) != (g->fb[i] & 0x00FFFFFF))
			bad++;
	CHECK(bad == 0, "the viewer got the framebuffer the guest drew");

	free(pix);
	close(fd);
	pump(4);
	CHECK(smolrfb_clients(&g->rfb) == 0, "the server notices the viewer leave");
}

int main(void)
{
	struct __smolkvm_virtio_gpu_priv *g = &__smolkvm_virtio_gpu_priv;
	uint32_t *backing;
	void *ram;
	int x, y;
	int bad;

	/*
	 * The device arms its RFB client sockets for SIGIO, and the default
	 * disposition for that is to kill us. smolkvm_create_vm() installs the
	 * handler; this harness does not call it, so do it here.
	 */
	__smolkvm_setup_sighandler();

	/* One memslot of "guest RAM", which is all the device ever looks at */
	ram = mmap(NULL, RAM_SZ, PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (ram == MAP_FAILED)
		return 1;
	memset(ram, 0, RAM_SZ);

	vm.memregions[0].slot = 0;
	vm.memregions[0].guest_phys_addr = RAM_BASE;
	vm.memregions[0].memory_size = RAM_SZ;
	vm.memregions[0].userspace_addr = (uint64_t) ram;

	smolkvm_virtio_gpu_configure(RES_W, RES_H, NULL, 15900);
	if (__smolkvm_virtio_gpu_create(&vm)) {
		printf("FAIL: could not create the device\n");
		return 1;
	}

	/* --- the transport --- */

	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_MAGIC) == 0x74726976, "magic is \"virt\"");
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_VERSION) == 2, "transport version 2");
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_DEVICE_ID) == 16, "device id is gpu");

	mmio_write(__SMOLKVM_VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_DEVICE_FEATURES) & 1,
	      "VERSION_1 is offered in the high feature word");

	/* Config space: one scanout */
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_CONFIG + 8) == 1, "config says one scanout");

	/* A driver that will not take VERSION_1 must be refused */
	mmio_write(__SMOLKVM_VIRTIO_MMIO_STATUS, __SMOLKVM_VIRTIO_STATUS_FEATURES_OK);
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_STATUS) & __SMOLKVM_VIRTIO_STATUS_FAILED,
	      "FEATURES_OK without VERSION_1 is refused");

	mmio_write(__SMOLKVM_VIRTIO_MMIO_STATUS, 0);
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_STATUS) == 0, "a zero status write resets");

	/* Negotiate properly this time */
	mmio_write(__SMOLKVM_VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
	mmio_write(__SMOLKVM_VIRTIO_MMIO_DRIVER_FEATURES, 1);
	mmio_write(__SMOLKVM_VIRTIO_MMIO_STATUS, __SMOLKVM_VIRTIO_STATUS_FEATURES_OK);
	CHECK(!(mmio_read(__SMOLKVM_VIRTIO_MMIO_STATUS) & __SMOLKVM_VIRTIO_STATUS_FAILED),
	      "FEATURES_OK with VERSION_1 is accepted");

	/* Both queues must exist, and a third must not */
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_SEL, 1);
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_QUEUE_NUM_MAX) != 0, "the cursor queue exists");
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_SEL, 2);
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_QUEUE_NUM_MAX) == 0, "there is no third queue");

	/* Set the control queue up */
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_SEL, 0);
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_NUM, QSZ);
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t) DESC_GPA);
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint32_t) (DESC_GPA >> 32));
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t) AVAIL_GPA);
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32_t) (AVAIL_GPA >> 32));
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t) USED_GPA);
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_USED_HIGH, (uint32_t) (USED_GPA >> 32));
	mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_READY, 1);
	CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_QUEUE_READY) == 1, "the control queue is ready");

	mmio_write(__SMOLKVM_VIRTIO_MMIO_STATUS,
		   __SMOLKVM_VIRTIO_STATUS_FEATURES_OK | __SMOLKVM_VIRTIO_STATUS_DRIVER_OK);

	/* --- the commands --- */

	{
		struct __smolkvm_virtio_gpu_ctrl_hdr req;
		const struct __smolkvm_virtio_gpu_resp_display_info *info = gpa(RESP_GPA);
		uint16_t *used = gpa(USED_GPA);

		hdr_init(&req, __SMOLKVM_VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
		req.flags = __SMOLKVM_VIRTIO_GPU_FLAG_FENCE;
		req.fence_id = 0x1234;
		submit(&req, sizeof(req), sizeof(*info));

		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
		      "GET_DISPLAY_INFO answered");
		CHECK(info->pmodes[0].r.width == RES_W && info->pmodes[0].r.height == RES_H,
		      "the display is the size we configured");
		CHECK(info->pmodes[0].enabled == 1, "scanout 0 is enabled");
		CHECK(info->hdr.fence_id == 0x1234 &&
		      (info->hdr.flags & __SMOLKVM_VIRTIO_GPU_FLAG_FENCE),
		      "the fence id is echoed back");
		CHECK(used[1] == 1, "the chain came back in the used ring");
		CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_INTERRUPT_STATUS)
		      & __SMOLKVM_VIRTIO_MMIO_INT_VRING, "the vring interrupt is asserted");

		mmio_write(__SMOLKVM_VIRTIO_MMIO_INTERRUPT_ACK,
			   __SMOLKVM_VIRTIO_MMIO_INT_VRING);
		CHECK(mmio_read(__SMOLKVM_VIRTIO_MMIO_INTERRUPT_STATUS) == 0,
		      "acking clears it");
	}

	{
		struct {
			struct __smolkvm_virtio_gpu_ctrl_hdr hdr;
			struct __smolkvm_virtio_gpu_resource_create_2d body;
		} req;

		/* A format we do not do must be refused, not drawn as noise */
		hdr_init(&req.hdr, __SMOLKVM_VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
		req.body.resource_id = 9;
		req.body.format = 67;		/* R8G8B8A8 */
		req.body.width = RES_W;
		req.body.height = RES_H;
		submit(&req, sizeof(req), sizeof(struct __smolkvm_virtio_gpu_ctrl_hdr));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
		      "an unsupported pixel format is refused");

		hdr_init(&req.hdr, __SMOLKVM_VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
		req.body.resource_id = 1;
		req.body.format = __SMOLKVM_VIRTIO_GPU_FORMAT_B8G8R8X8;
		req.body.width = RES_W;
		req.body.height = RES_H;
		submit(&req, sizeof(req), sizeof(struct __smolkvm_virtio_gpu_ctrl_hdr));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_OK_NODATA,
		      "RESOURCE_CREATE_2D accepted");
	}

	/*
	 * Back the resource with two entries, so the scatter list is actually
	 * scattered and the reader has to cross an entry boundary mid-row.
	 */
	backing = gpa(BACKING_GPA);
	for (y = 0; y < RES_H; y++)
		for (x = 0; x < RES_W; x++)
			backing[y * RES_W + x] = (uint32_t) ((y << 8) | x);

	{
		struct {
			struct __smolkvm_virtio_gpu_ctrl_hdr hdr;
			struct __smolkvm_virtio_gpu_attach_backing body;
			struct __smolkvm_virtio_gpu_mem_entry entries[2];
		} req;
		size_t half = (size_t) RES_W * RES_H * 4 / 2;

		hdr_init(&req.hdr, __SMOLKVM_VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
		req.body.resource_id = 1;
		req.body.nr_entries = 2;
		req.entries[0].addr = BACKING_GPA;
		req.entries[0].length = half;
		req.entries[0].padding = 0;
		req.entries[1].addr = BACKING_GPA + half;
		req.entries[1].length = half;
		req.entries[1].padding = 0;
		submit(&req, sizeof(req), sizeof(struct __smolkvm_virtio_gpu_ctrl_hdr));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_OK_NODATA,
		      "RESOURCE_ATTACH_BACKING accepted");
	}

	{
		struct {
			struct __smolkvm_virtio_gpu_ctrl_hdr hdr;
			struct __smolkvm_virtio_gpu_set_scanout body;
		} req;

		hdr_init(&req.hdr, __SMOLKVM_VIRTIO_GPU_CMD_SET_SCANOUT);
		memset(&req.body, 0, sizeof(req.body));
		req.body.r.width = RES_W;
		req.body.r.height = RES_H;
		req.body.scanout_id = 0;
		req.body.resource_id = 1;
		submit(&req, sizeof(req), sizeof(struct __smolkvm_virtio_gpu_ctrl_hdr));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_OK_NODATA, "SET_SCANOUT accepted");
	}

	/* Binding pulls the whole resource in, across both backing entries */
	bad = 0;
	for (y = 0; y < RES_H; y++)
		for (x = 0; x < RES_W; x++)
			if (g->fb[y * RES_W + x] != (uint32_t) ((y << 8) | x))
				bad++;
	CHECK(bad == 0, "SET_SCANOUT copied the whole scattered resource in");

	/* Change part of it and transfer just that rectangle back */
	for (y = 4; y < 12; y++)
		for (x = 8; x < 24; x++)
			backing[y * RES_W + x] = 0x00FF00FF;

	{
		struct {
			struct __smolkvm_virtio_gpu_ctrl_hdr hdr;
			struct __smolkvm_virtio_gpu_transfer_to_host_2d body;
		} req;

		hdr_init(&req.hdr, __SMOLKVM_VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
		req.body.r.x = 8;
		req.body.r.y = 4;
		req.body.r.width = 16;
		req.body.r.height = 8;
		req.body.offset = (4ULL * RES_W + 8) * 4;
		req.body.resource_id = 1;
		req.body.padding = 0;
		submit(&req, sizeof(req), sizeof(struct __smolkvm_virtio_gpu_ctrl_hdr));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_OK_NODATA,
		      "TRANSFER_TO_HOST_2D accepted");
	}

	bad = 0;
	for (y = 0; y < RES_H; y++) {
		for (x = 0; x < RES_W; x++) {
			bool inside = (y >= 4 && y < 12 && x >= 8 && x < 24);
			uint32_t want = inside ? 0x00FF00FF : (uint32_t) ((y << 8) | x);

			if (g->fb[y * RES_W + x] != want)
				bad++;
		}
	}
	CHECK(bad == 0, "the transfer moved exactly the rectangle asked for");

	/* A rectangle running off the edge is clipped, not refused or overrun */
	{
		struct {
			struct __smolkvm_virtio_gpu_ctrl_hdr hdr;
			struct __smolkvm_virtio_gpu_transfer_to_host_2d body;
		} req;

		hdr_init(&req.hdr, __SMOLKVM_VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
		req.body.r.x = RES_W - 4;
		req.body.r.y = RES_H - 2;
		req.body.r.width = 999;
		req.body.r.height = 999;
		req.body.offset = 0;
		req.body.resource_id = 1;
		req.body.padding = 0;
		submit(&req, sizeof(req), sizeof(struct __smolkvm_virtio_gpu_ctrl_hdr));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_OK_NODATA,
		      "an oversized transfer rectangle is clipped");
	}

	/* An unknown resource and an unimplemented command both come back as errors */
	{
		struct {
			struct __smolkvm_virtio_gpu_ctrl_hdr hdr;
			struct __smolkvm_virtio_gpu_resource_flush body;
		} req;

		hdr_init(&req.hdr, __SMOLKVM_VIRTIO_GPU_CMD_RESOURCE_FLUSH);
		req.body.r.x = 0;
		req.body.r.y = 0;
		req.body.r.width = RES_W;
		req.body.r.height = RES_H;
		req.body.resource_id = 77;
		req.body.padding = 0;
		submit(&req, sizeof(req), sizeof(struct __smolkvm_virtio_gpu_ctrl_hdr));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
		      "a flush of an unknown resource is an error");

		req.body.resource_id = 1;
		hdr_init(&req.hdr, __SMOLKVM_VIRTIO_GPU_CMD_RESOURCE_FLUSH);
		submit(&req, sizeof(req), sizeof(struct __smolkvm_virtio_gpu_ctrl_hdr));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_OK_NODATA, "RESOURCE_FLUSH accepted");
	}

	{
		struct __smolkvm_virtio_gpu_ctrl_hdr req;

		hdr_init(&req, 0x0205);		/* a 3D command */
		submit(&req, sizeof(req), sizeof(req));
		CHECK(resp_type() == __SMOLKVM_VIRTIO_GPU_RESP_ERR_UNSPEC,
		      "a 3D command is refused");
	}

	/* A descriptor chain that points at itself must not wedge us */
	{
		struct __smolkvm_vring_desc *desc = gpa(DESC_GPA);
		uint16_t *avail = gpa(AVAIL_GPA);
		uint16_t *used = gpa(USED_GPA);
		uint16_t before = used[1];

		desc[0].addr = REQ_GPA;
		desc[0].len = 8;
		desc[0].flags = __SMOLKVM_VRING_DESC_F_NEXT;
		desc[0].next = 0;		/* itself */

		avail[2 + (avail[1] % QSZ)] = 0;
		avail[1]++;
		mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_NOTIFY, 0);

		CHECK(used[1] == before + 1, "a looping descriptor chain is survived");
	}

	/* ...and neither must an avail entry pointing past the table */
	{
		uint16_t *avail = gpa(AVAIL_GPA);

		avail[2 + (avail[1] % QSZ)] = QSZ + 5;
		avail[1]++;
		mmio_write(__SMOLKVM_VIRTIO_MMIO_QUEUE_NOTIFY, 0);
		CHECK(1, "an out of range avail entry is survived");
	}

	/* --- and out the other end, to an actual viewer --- */
	rfb_client_check(g);

	/* Reset must drop the resources it is holding */
	mmio_write(__SMOLKVM_VIRTIO_MMIO_STATUS, 0);
	CHECK(g->scanout_res == 0 && g->res[0].id == 0 && g->res[0].backing == NULL,
	      "reset released the resources");

	smolrfb_close(&g->rfb);

	printf("\n%s\n", failures ? "THERE WERE FAILURES" : "all checks passed");

	return failures ? 1 : 0;
}
