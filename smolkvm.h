// SPDX-License-Identifier: GPL-3.0-or-later
// kate: tab-width 8;

#ifndef _SMOLKVM_H
#define _SMOLKVM_H

#ifndef NOLIBC
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#endif

#include <linux/kvm.h>

/*
 * SMoLKVM - Your favourite single header crappy implementation of KVM userspace.
 *
 * How do (WIP):
 *
 * - Allocate a "struct smolkvm". This can be on the stack or whatever.
 * - Call smolkvm_create_vm(*vm) with the pointer to the above. This will create
 *   the most basic setup that has xyz.
 *
 * ~~Code notes~~
 *
 * Function prefixes:
 *
 * "", no prefix, these are for you, the user, to use.
 * "_", single underscore, internal functions, not intended for external use but maybe ok
 * "__", double underscore, private functions, don't use.
 */

/* This is just to allow your editor to fold blocks of code so this is easier to read */
#define SMOLKVM_FOLD

/*
 * Configuration defines:
 *   SMOLKVM_DEBUG                -- Be very noisy about what is going on to help with working out what is broken.
 *   SMOLKVM_MEMREGIONS_NUM       -- How many memory regions are possible, see default below.
 *   SMOLKVM_MMIOREGIONS_NUM      -- How many mmio, device, regions are possible, see default below.
 *   SMOLKVM_WANT_GDB_STUB        -- Include a GDB remote stub for debugging.
 *   SMOLKVM_WANT_GDB_STUB_DEBUG  -- Add noisy debug messages for the stub.
 */
#ifdef SMOLKVM_FOLD

#ifndef SMOLKVM_MEMREGIONS_NUM
#define SMOLKVM_MEMREGIONS_NUM	8
#endif

#ifndef SMOLKVM_MMIOREGIONS_NUM
#define SMOLKVM_MMIOREGIONS_NUM	8
#endif
/* -- */

/* Utility macros */
#define SMOLKVM_ARRAYSIZE(_a)	(sizeof(_a)/sizeof(_a[0]))
#define SMOLKVM_BIT(_bit)	(1ULL << _bit)
#define SMOLKVM_SZ_1K		1024ULL
#define SMOLKVM_SZ_4K		(SMOLKVM_SZ_1K * 4)
#define SMOLKVM_SZ_1MB		(SMOLKVM_SZ_1K * 1024)

#endif
/* -- */

/* Error numbers */
#ifdef SMOLKVM_FOLD

#define SMOLKVM_ERR_OPENKVM         100
#define SMOLKVM_ERR_CREATEVM        101
#define SMOLKVM_ERR_CREATEVCPU      102
#define SMOLKVM_ERR_VCPUMMAPSIZE    103
#define SMOLKVM_ERR_VCPUMMAP        104
#define SMOLKVM_ERR_ALLOCMEMORY     106
#define SMOLKVM_ERR_SETMEMORYREGION 107
#define SMOLKVM_ERR_CREATE_GETSREGS 108
#define SMOLKVM_ERR_CREATE_SETSREGS 109
#define SMOLKVM_ERR_CREATE_GETREGS  110
#define SMOLKVM_ERR_CREATE_SETREGS  111

#endif
/* -- */

/* Debug printing, register dumping */
#ifdef SMOLKVM_FOLD

#ifdef SMOLKVM_DEBUG
#define __smolkvm_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define __smolkvm_debug
#endif

static inline void __smolkvm_dump_regs(struct kvm_regs *regs)
{
	printf("RAX   0x%016llx  RBX   0x%016llx\n", regs->rax, regs->rbx);
	printf("RCX   0x%016llx  RDX   0x%016llx\n", regs->rcx, regs->rdx);
	printf("RSI   0x%016llx  RDI   0x%016llx\n", regs->rsi, regs->rdi);
	printf("RBP   0x%016llx  RSP   0x%016llx\n", regs->rbp, regs->rsp);
	printf("R8    0x%016llx  R9    0x%016llx\n", regs->r8, regs->r9);
	printf("R10   0x%016llx  R11   0x%016llx\n", regs->r10, regs->r11);
	printf("R12   0x%016llx  R13   0x%016llx\n", regs->r12, regs->r13);
	printf("R14   0x%016llx  R15   0x%016llx\n", regs->r14, regs->r15);
	printf("RIP   0x%016llx  RFLAGS 0x%016llx\n", regs->rip, regs->rflags);
}

static inline void __smolkvm_dump_sregs(struct kvm_sregs *sregs)
{
	printf("CR0   0x%016llx\n", sregs->cr0);
	printf("CR3   0x%016llx\n", sregs->cr3);
	printf("CR4   0x%016llx\n", sregs->cr4);
	printf("EFER  0x%016llx\n", sregs->efer);
}

#ifdef SMOLKVM_DEBUG
static inline void __smolkvm_debug_dump_regs(struct kvm_regs *regs)
{
	__smolkvm_dump_regs(regs);
}

static inline void __smolkvm_debug_dump_sregs(struct kvm_sregs *sregs)
{
	__smolkvm_dump_sregs(sregs);
}
#else
#define __smolkvm_debug_dump_regs
#define __smolkvm_debug_dump_sregs
#endif

#endif
/* -- */

/* x86 arch defines */
#ifdef SMOLKVM_FOLD

#define SMOLKVM_X86_PTE_NUM	512
/* Special register bits */
#define SMOLKVM_X86_CR0_PE	SMOLKVM_BIT(0)
#define SMOLKVM_X86_CR0_PG	SMOLKVM_BIT(31)
#define SMOLKVM_X86_CR4_PSE	SMOLKVM_BIT(4)
#define SMOLKVM_X86_CR4_PAE	SMOLKVM_BIT(5)
#define SMOLKVM_X86_EFER_LME	SMOLKVM_BIT(8)
#define SMOLKVM_X86_EFER_LMA	SMOLKVM_BIT(10)
/* Page table bits */
#define SMOLKVM_PTE_PRESENT	SMOLKVM_BIT(0)
#define SMOLKVM_PTE_RW		SMOLKVM_BIT(1)
#define SMOLKVM_PTE_PS		SMOLKVM_BIT(7)
#define SMOLKVM_PTE_ADDR(_addr)	(((uint64_t)(_addr) >> 12) << 12)

#endif
/* -- */

/* State structures */
#ifdef SMOLKVM_FOLD

struct smolkvm_pgtable {
	uint64_t entries[SMOLKVM_X86_PTE_NUM];
};

struct smolkvm_vm;

struct smolkvm_mmio {
	const char *name;
	uint64_t phys;
	uint64_t len;

	void (*pre_run)(struct smolkvm_vm *vm,
			struct smolkvm_mmio *mmio);

	void (*write)(struct smolkvm_vm *vm,
		      struct smolkvm_mmio *mmio,
		      uint64_t offset,
		      uint8_t len,
		      uint64_t value);
	uint64_t (*read)(struct smolkvm_vm *vm,
			 struct smolkvm_mmio *mmio,
		         uint64_t offset,
			 uint8_t len);

	void (*post_run)(struct smolkvm_vm *vm,
			 struct smolkvm_mmio *mmio);

	void *priv;
};

#ifdef SMOLKVM_WANT_GDB_STUB
struct smolkvm_gdb_stub {
	int listen_socket;
	int conn_socket;

	/* Are we waiting for GDB to tell us to run ? */
	bool stopped;
	bool single_stepping;
};
#endif

struct smolkvm_vm {
	int vcpu_fd;
	int kvm_fd;
	int vm_fd;
	struct kvm_run *vcpu_run;
	size_t vcpu_run_sz;
	struct smolkvm_pgtable *pgtables;

	struct kvm_userspace_memory_region memregions[SMOLKVM_MEMREGIONS_NUM];
	/* Running count only; per-slot occupancy (memory_size != 0) is authoritative */
	unsigned int memory_region_plugged_in;

	struct smolkvm_mmio *mmioregions[SMOLKVM_MMIOREGIONS_NUM];
	/* Running count only; per-slot occupancy (pointer != NULL) is authoritative */
	unsigned int mmio_plugged_in;

#ifdef SMOLKVM_WANT_GDB_STUB
	struct smolkvm_gdb_stub gdb_stub;
#endif

	bool was_interrupted;
};

#endif
/* -- */

/* Slot management: per-slot occupancy is the source of truth */
#ifdef SMOLKVM_FOLD

static inline bool __smolkvm_memregion_slot_used(const struct smolkvm_vm *vm, unsigned int slot)
{
	return vm->memregions[slot].memory_size != 0;
}

/* First free RAM slot, or -1 if the table is full */
static inline int __smolkvm_find_free_memregion_slot(const struct smolkvm_vm *vm)
{
	unsigned int i;

	for (i = 0; i < SMOLKVM_ARRAYSIZE(vm->memregions); i++)
		if (!__smolkvm_memregion_slot_used(vm, i))
			return (int) i;

	return -1;
}

#endif
/* -- */

/* KVM ioctl wrappers */
#ifdef SMOLKVM_FOLD

/*  -1 - Error
 *   0 - ioctl returned
 *   1 - interrupted;
 */
static inline int __smolkvm_run(const struct smolkvm_vm *vm)
{
	int ret;

	ret = ioctl(vm->vcpu_fd, KVM_RUN, 0);
	if (ret) {
		if (errno == EINTR) {
			__smolkvm_debug("KVM_RUN was interrupted\n");
			return 1;
		}
		__smolkvm_debug("KVM_RUN failed: %d, errno: %d\n", ret, errno);
		return -1;
	}

	return ret;
}

static inline int __smolkvm_get_regs(const struct smolkvm_vm *vm, struct kvm_regs *regs)
{
	return ioctl(vm->vcpu_fd, KVM_GET_REGS, regs);
}

static inline int __smolkvm_set_regs(struct smolkvm_vm *vm, struct kvm_regs *regs)
{
	return ioctl(vm->vcpu_fd, KVM_SET_REGS, regs);
}

static inline int __smolkvm_get_sregs(const struct smolkvm_vm *vm, struct kvm_sregs *sregs)
{
	return ioctl(vm->vcpu_fd, KVM_GET_SREGS, sregs);
}

static inline int __smolkvm_set_sregs(const struct smolkvm_vm *vm, struct kvm_sregs *sregs)
{
	return ioctl(vm->vcpu_fd, KVM_SET_SREGS, sregs);
}

static inline int __smolkvm_set_guest_debug(struct smolkvm_vm *vm, const struct kvm_guest_debug *guest_debug)
{
	int ret = ioctl(vm->vcpu_fd, KVM_SET_GUEST_DEBUG, guest_debug);
	if (ret)
		__smolkvm_debug("KVM_SET_GUEST_DEBUG failed: %d\n", errno);

	return ret;
}

#endif
/* -- */

/* signal stuff */
#ifdef SMOLKVM_FOLD

static inline void __smolkvm_signal_sigio(int signum)
{
	// Just exit the KVM run call.
}

static inline int __smolkvm_setup_sighandler(void)
{
	struct sigaction sa = { 0 };
	sa.sa_handler = __smolkvm_signal_sigio;

	sigemptyset(&sa.sa_mask);

	sigaction(SIGIO, &sa, NULL);

	return 0;
}

#endif
/* -- */

/* Network functions */
#ifdef SMOLKVM_FOLD

static int __smolkvm_make_socket_trigger_sigio(int sock)
{
	int flags;

	fcntl(sock, F_SETOWN, getpid());
	flags = fcntl(sock, F_GETFL);
	fcntl(sock, F_SETFL, flags | O_ASYNC);

	return 0;
}

static int __smolkvm_create_server_socket(int port)
{
	int opt = 1;
	int sock;
	int ret;
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = htonl(INADDR_ANY),
	};

	sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock < 0) {
		printf("failed to create socket\n");
		return -1;
	}

	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret) {
		printf("failed to set socket options\n");
		goto err_close_sock;
	}

	ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		printf("failed to bind socket\n");
		goto err_close_sock;
	}

	ret = listen(sock, 1);
	if (ret < 0) {
		printf("failed to listen on socket\n");
		goto err_close_sock;
	}

	return sock;

err_close_sock:
	close(sock);
	return -1;
}

static int __smolkvm_create_unix_domain_socket(const char *path)
{
	struct sockaddr_un addr = { 0 };
	int sock;
	int ret;

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path);

	sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sock < 0)
		return -1;

	unlink(path);

	ret = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
	if (ret) {
		printf("failed to bind socket\n");
		goto err_close_sock;
	}

	ret = listen(sock, 1);
	if (ret) {
		printf("failed to listen on socket\n");
		goto err_close_sock;
	}

	return sock;

err_close_sock:
	close(sock);
	return -1;
}

#endif
/* -- */

#ifdef SMOLKVM_WANT_GDB_STUB
/* The raw "type" characters that are at the start of the GDB packet */
#define SMOLKVM_GDB_STUB_PKTTYPE_EXTENDED	'!'
#define SMOLKVM_GDB_STUB_PKTTYPE_STOP_REASON	'?'
#define SMOLKVM_GDB_STUB_PKTTYPE_CONTINUE	'c'
#define SMOLKVM_GDB_STUB_PKTTYPE_READ_REGS	'g'
#define SMOLKVM_GDB_STUB_PKTTYPE_H		'H'
#define SMOLKVM_GDB_STUB_PKTTYPE_READ_MEM	'm'
#define SMOLKVM_GDB_STUB_PKTTYPE_QUERY		'q'
#define SMOLKVM_GDB_STUB_PKTTYPE_STEP		's'
#define SMOLKVM_GDB_STUB_PKTTYPE_V		'v'

#define SMOLKVM_GDB_TARGET_XML "<target version=\"1.0\"><architecture>i386:x86-64</architecture></target>"

static const unsigned char __smolkvm_gdb_stub_ack = '+';
static const unsigned char __smolkvm_gdb_stub_nak = '-';
static const unsigned char __smolkvm_gdb_stub_pktstart = '$';
static const unsigned char __smolkvm_gdb_stub_pktend = '#';

static const unsigned char *__smolkvm_gdb_pkt_query_attached = "Attached";
static const unsigned char *__smolkvm_gdb_pkt_query_support = "Supported:";
static const unsigned char *__smolkvm_gdb_pkt_query_xfer = "Xfer:features:read:";
static const unsigned char *__smolkvm_gdb_pkt_v_cont = "Cont?";
static const unsigned char *__smolkvm_gdb_pkt_v_mustreplyempty = "MustReplyEmpty";

enum smolkvm_gdb_stub_type {
	SMOLKVM_GDB_STUB_UNKNOWN,
	SMOLKVM_GDB_STUB_STOP_REASON,
	SMOLKVM_GDB_STUB_READ_REGS,
	SMOLKVM_GDB_STUB_CONTINUE,
	SMOLKVM_GDB_STUB_STEP,
	SMOLKVM_GDB_STUB_QUERY,
	SMOLKVM_GDB_STUB_READ_MEM,
	SMOLKVM_GDB_STUB_H,
	SMOLKVM_GDB_STUB_V,
};

enum smolkvm_gdb_stub_query_subtype {
	SMOLKVM_GDB_STUB_QUERY_UNKNOWN,
	SMOLKVM_GDB_STUB_QUERY_ATTACHED,
	SMOLKVM_GDB_STUB_QUERY_SUPPORTED,
	SMOLKVM_GDB_STUB_QUERY_XFER_FEATURES,
};

enum smolkvm_gdb_stub_v_subtype {
	SMOLKVM_GDB_STUB_V_CONT,
	SMOLKVM_GDB_STUB_V_MUSTREPLYEMPTY,
	SMOLKVM_GDB_STUB_V_UNKNOWN,
};

struct smolkvm_gdb_stub_pkt_query {
	enum smolkvm_gdb_stub_query_subtype subtype;
};

struct smolkvm_gdb_stub_pkt_h {
	int subtype;
};

struct smolkvm_gdb_stub_pkt_v {
	enum smolkvm_gdb_stub_v_subtype subtype;
};

struct smolkvm_gdb_stub_pkt_read_mem {
	uint64_t addr;
	uint64_t len;
};

struct smolkvm_gdb_stub_pkt {
	enum smolkvm_gdb_stub_type type;
	union {
		struct smolkvm_gdb_stub_pkt_query query;
		struct smolkvm_gdb_stub_pkt_h h;
		struct smolkvm_gdb_stub_pkt_v v;
		struct smolkvm_gdb_stub_pkt_read_mem read_mem;
	};
};

static inline uint8_t __smolkvm_gdb_stub_checksum(const char *data, unsigned int len)
{
	unsigned long long chksum = 0;
	int i;

	for (i = 0; i < len; i++)
		chksum += data[i];
	chksum %= 256;

	return chksum;
}

static inline bool __smolkvm_gdb_stub_pkt_checksum(const char* pkt, unsigned int pktlen, const char* chksum)
{
	unsigned long long from_packet;

	from_packet = strtoull(chksum, NULL, 16);

	return __smolkvm_gdb_stub_checksum(pkt, pktlen) == from_packet;
}

static inline int __smolkvm_gdb_split_value_comma_value(const char* str, unsigned int len,
							uint64_t *left, uint64_t *right)
{
	unsigned long long l, r;

	// TODO make this safe
	l = strtoull(str, NULL, 16);
	while (*str && *str != ',')
		str++;
	if (*str == ',')
		str++;
	r = strtoull(str, NULL, 16);

	*left = l;
	*right = r;
}

#define __smolkvm_gdb_stub_start_match(_token) \
	(memcmp(raw, _token, sizeof(_token) -1) == 0)

static inline int __smolkvm_gdb_stub_pkt_unpack(const unsigned char *raw,
	unsigned int len, struct smolkvm_gdb_stub_pkt *pkt)
{
	unsigned char type = *raw++;
	len--;

	switch(type) {
	case SMOLKVM_GDB_STUB_PKTTYPE_CONTINUE:
		pkt->type = SMOLKVM_GDB_STUB_CONTINUE;
	break;
	case SMOLKVM_GDB_STUB_PKTTYPE_STEP:
		pkt->type = SMOLKVM_GDB_STUB_STEP;
	break;
	case SMOLKVM_GDB_STUB_PKTTYPE_STOP_REASON:
		pkt->type = SMOLKVM_GDB_STUB_STOP_REASON;
		break;
	case SMOLKVM_GDB_STUB_PKTTYPE_READ_REGS:
		pkt->type = SMOLKVM_GDB_STUB_READ_REGS;
	break;
	case SMOLKVM_GDB_STUB_PKTTYPE_READ_MEM:  // Add this
		pkt->type = SMOLKVM_GDB_STUB_READ_MEM;
		__smolkvm_gdb_split_value_comma_value(raw, len, &pkt->read_mem.addr, &pkt->read_mem.len);
		break;
	case SMOLKVM_GDB_STUB_PKTTYPE_QUERY:
		pkt->type = SMOLKVM_GDB_STUB_QUERY;
		if (__smolkvm_gdb_stub_start_match(__smolkvm_gdb_pkt_query_attached))
			pkt->query.subtype = SMOLKVM_GDB_STUB_QUERY_ATTACHED;
		else if (__smolkvm_gdb_stub_start_match(__smolkvm_gdb_pkt_query_support))
			pkt->query.subtype = SMOLKVM_GDB_STUB_QUERY_SUPPORTED;
		else if (__smolkvm_gdb_stub_start_match(__smolkvm_gdb_pkt_query_xfer))
			pkt->query.subtype = SMOLKVM_GDB_STUB_QUERY_XFER_FEATURES;
		else
			pkt->query.subtype = SMOLKVM_GDB_STUB_QUERY_UNKNOWN;
		break;
	case SMOLKVM_GDB_STUB_PKTTYPE_H:
		pkt->type = SMOLKVM_GDB_STUB_H;
		break;
	case SMOLKVM_GDB_STUB_PKTTYPE_V:
		pkt->type = SMOLKVM_GDB_STUB_V;
		if (__smolkvm_gdb_stub_start_match(__smolkvm_gdb_pkt_v_cont))
			pkt->query.subtype = SMOLKVM_GDB_STUB_V_CONT;
		else if (__smolkvm_gdb_stub_start_match(__smolkvm_gdb_pkt_v_mustreplyempty))
			pkt->query.subtype = SMOLKVM_GDB_STUB_V_MUSTREPLYEMPTY;
		else
			pkt->query.subtype = SMOLKVM_GDB_STUB_V_UNKNOWN;
		break;
	default:
		pkt->type = SMOLKVM_GDB_STUB_UNKNOWN;
		return -1;
	}

	return 0;
}
#endif /* SMOLKVM_WANT_GDB_STUB */

/* mmio handling */
#ifdef SMOLKVM_FOLD

static inline bool __smolkvm_mmio_slot_used(const struct smolkvm_vm *vm, unsigned int slot)
{
	return vm->mmioregions[slot] != NULL;
}

static inline int __smolkvm_find_free_mmio_slot(const struct smolkvm_vm *vm)
{
	unsigned int i;

	for (i = 0; i < SMOLKVM_ARRAYSIZE(vm->mmioregions); i++)
		if (!__smolkvm_mmio_slot_used(vm, i))
			return (int) i;

	return -1;
}

#define __smolkvm_foreach_mmio(_vm, __mmio) \
	for (__mmio = &(_vm)->mmioregions[0]; \
	     __mmio != &(_vm)->mmioregions[SMOLKVM_ARRAYSIZE((_vm)->mmioregions)]; \
	     __mmio++) \
		if (*__mmio)

static inline int __smolkvm_plugin_mmio(struct smolkvm_vm *vm, const struct smolkvm_mmio *mmio)
{
	int slot = __smolkvm_find_free_mmio_slot(vm);

	if (slot < 0) {
		printf("no free mmio slots, cannot plug in \'%s\'\n", mmio->name);
		return -1;
	}

	__smolkvm_debug("Plugging in MMIO device \'%s\' to slot %d\n",
			mmio->name, slot);

	vm->mmioregions[slot] = mmio;
	vm->mmio_plugged_in++;

	return 0;
}

static inline int __smolkvm_find_mmio_device(const struct smolkvm_vm *vm, uint64_t addr)
{
	unsigned int i;

	for (i = 0; i < SMOLKVM_ARRAYSIZE(vm->mmioregions); i++) {
		struct smolkvm_mmio *mmio = vm->mmioregions[i];

		if (!mmio)
			continue;

		if ((addr >= mmio->phys) && (addr < (mmio->phys + mmio->len)))
			return (int) i;
	}

	return -1;
}

static inline int __smolkvm_handle_mmio(struct smolkvm_vm *vm)
{
	struct kvm_run *run = vm->vcpu_run;
	uint64_t mmio_addr = run->mmio.phys_addr;
	int mmio_device = __smolkvm_find_mmio_device(vm, mmio_addr);
	bool is_write = run->mmio.is_write;
	uint64_t len = run->mmio.len;
	struct smolkvm_mmio *mmio;
	uint64_t offset;

	__smolkvm_debug("mmio %s to 0x%lx, len %lu\n",
			is_write ? "write" : "read", mmio_addr, len);
	if (is_write)
	    __smolkvm_debug("0x%02x, 0x%02x, 0x%02x, 0x%02x\n"
			    "0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			    run->mmio.data[0], run->mmio.data[1], run->mmio.data[2], run->mmio.data[3],
			    run->mmio.data[4], run->mmio.data[5], run->mmio.data[6], run->mmio.data[7]);

	if (mmio_device < 0) {
		printf("didn't find mmio device for 0x%lx\n", mmio_addr);

		/* TODO should be an error roight? */
		return 0;
	}

	mmio = vm->mmioregions[mmio_device];
	__smolkvm_debug("mmio access to device \'%s\'\n", mmio->name);

	offset = mmio_addr - mmio->phys;

	// value is in run->mmio.data[8]

	if (is_write) {
		uint64_t val = 0;
		int i;

		/* Pack the data into a u64, I might regret this later :/ */
		for (i = 0; i < (int) len && i < sizeof(run->mmio.data); i++)
		    val |= ((uint64_t) run->mmio.data[i]) << (i * 8);

		mmio->write(vm, mmio, offset, len, val);
	}
	else {
		uint64_t val = 0;
		int i;

		val = mmio->read(vm, mmio, offset, len);
		for (i = 0; i < (int) len && i < sizeof(run->mmio.data); i++)
			run->mmio.data[i] = (val >> (i * 8)) & 0xff;
	}

	return 0;
}

#endif
/* -- */

/* Everyone loves a console */
#ifdef SMOLKVM_FOLD

/* todo, fix these offsets once I decide the register layout */
#define __SMOLKVM_CONSOLE_REG_STATUS	1
#define __SMOLKVM_CONSOLE_REG_TXRX	0

struct __smolkvm_console_fifo {
	uint8_t fifo[64];
	unsigned head;
	unsigned tail;
};

bool __smolkvm_fifo_is_empty(struct __smolkvm_console_fifo *fifo)
{
	return fifo->head == fifo->tail;
}

bool __smolkvm_fifo_is_full(struct __smolkvm_console_fifo *fifo)
{
	return ((fifo->head + 1) % SMOLKVM_ARRAYSIZE(fifo->fifo)) == fifo->tail;
}

uint8_t __smolkvm_fifo_consume(struct __smolkvm_console_fifo *fifo)
{
	uint8_t item = fifo->fifo[fifo->tail];
	fifo->tail = (fifo->tail + 1) % SMOLKVM_ARRAYSIZE(fifo->fifo);

	return item;
}

struct {
	int listen_socket;
	int connected_socket;

	struct __smolkvm_console_fifo fifo_rx;
} __smolkvm_console_priv;

static void __smolkvm_console_check_for_data_from_socket(struct smolkvm_vm *vm,
							 struct smolkvm_mmio *mmio)
{
	int ret;
	int connected_socket = __smolkvm_console_priv.connected_socket;
	struct __smolkvm_console_fifo *fifo_rx = &__smolkvm_console_priv.fifo_rx;

	if (__smolkvm_fifo_is_full(fifo_rx))
		return;

	if (connected_socket >= 0) {
		uint8_t *dst = &fifo_rx->fifo[fifo_rx->head];

		ret = read(connected_socket, dst, 1);
		if (ret > 0) {
			__smolkvm_debug("read %d from console socket\n", ret);
			fifo_rx->head = (fifo_rx->head + ret) % SMOLKVM_ARRAYSIZE(fifo_rx->fifo);
		}
	}
}

static int __smolkvm_console_check_for_connection(struct smolkvm_vm *vm,
						   struct smolkvm_mmio *mmio)
{
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int conn_socket;

	conn_socket = accept4(__smolkvm_console_priv.listen_socket,
		      (struct sockaddr *)&client_addr, &addr_len, O_NONBLOCK);
	if (conn_socket < 0) {
		/* No one connected, that's sad but ok */
		if (errno == EAGAIN)
			return 0;

		printf("accept failed for console: %d, errno %d\n", conn_socket, errno);
		return -1;
	}

	__smolkvm_debug("someone connected to the console socket\n");
	__smolkvm_console_priv.connected_socket = conn_socket;

	return 1;
}

static void __smolkvm_console_pre_run(struct smolkvm_vm *vm,
				      struct smolkvm_mmio *mmio)
{
	printf("%s:%d\n", __func__, __LINE__);
	__smolkvm_console_check_for_connection(vm, mmio);
	__smolkvm_console_check_for_data_from_socket(vm, mmio);
}

static uint64_t __smolkvm_console_read(struct smolkvm_vm *vm,
				       struct smolkvm_mmio *mmio,
				       uint64_t offset,
				       uint8_t len)
{
	struct __smolkvm_console_fifo *fifo_rx = &__smolkvm_console_priv.fifo_rx;

	if (len != 1)
		return 0;

	switch (offset) {
	case __SMOLKVM_CONSOLE_REG_STATUS:
		if (!__smolkvm_fifo_is_empty(fifo_rx))
			return 1;
		break;
	case __SMOLKVM_CONSOLE_REG_TXRX:
		if (!__smolkvm_fifo_is_empty(fifo_rx))
			return __smolkvm_fifo_consume(fifo_rx);
		break;
	}

	return 0;
}

static void __smolkvm_console_write(struct smolkvm_vm *vm,
				    struct smolkvm_mmio *mmio,
				    uint64_t offset,
				    uint8_t len,
				    uint64_t value)
{
	int connected_socket = __smolkvm_console_priv.connected_socket;

	if (offset == __SMOLKVM_CONSOLE_REG_TXRX && len == 1) {
		char ch = (char) value;
		if (connected_socket >= 0)
			write(connected_socket, &ch, sizeof(ch));

		printf("** %c **\n", (char) value);
	}

}

static void __smolkvm_console_post_run(struct smolkvm_vm *vm,
				      struct smolkvm_mmio *mmio)
{
	printf("%s:%d\n", __func__, __LINE__);
	__smolkvm_console_check_for_connection(vm, mmio);
}

#define SMOLKVM_CONSOLE_PHYS 0x1000

static const struct smolkvm_mmio __smolkvm_console = {
	.name = "console",
	.phys = SMOLKVM_CONSOLE_PHYS,
	.len = 8,
	.pre_run = __smolkvm_console_pre_run,
	.write = __smolkvm_console_write,
	.read = __smolkvm_console_read,
	.post_run = __smolkvm_console_post_run,
	.priv = &__smolkvm_console_priv,
};

static inline int __smolkvm_console_create(struct smolkvm_vm *vm)
{
	int listen_sock;

	__smolkvm_console_priv.connected_socket = -1;
	__smolkvm_console_priv.fifo_rx.head = 0;
	__smolkvm_console_priv.fifo_rx.tail = 0;

	listen_sock = __smolkvm_create_unix_domain_socket("/tmp/smolkvmconsole");
	if (listen_sock < 0)
		return 1;

	__smolkvm_make_socket_trigger_sigio(listen_sock);

	__smolkvm_console_priv.listen_socket = listen_sock;

	__smolkvm_plugin_mmio(vm, &__smolkvm_console);

	return 0;
}

#endif
/* -- */

/* Read/Write into guest memory */
#ifdef SMOLKVM_FOLD

#define SMOLKVM_OFFSET_IN_MEMREGION(__memregion, __physaddr) \
	(__memregion->guest_phys_addr - __physaddr)

#define SMOLKVM_MEMREGION_PTR(_memregion, _physaddr) \
	((void *) _memregion->userspace_addr + SMOLKVM_OFFSET_IN_MEMREGION(_memregion, _physaddr))

static inline int __smolkvm_find_memregion(const struct smolkvm_vm *vm, uint64_t addr)
{
	int i;

	for (i = 0; i < SMOLKVM_ARRAYSIZE(vm->memregions); i++)
	{
		const struct kvm_userspace_memory_region *memory_region = &vm->memregions[i];
		uint64_t phys_start = memory_region->guest_phys_addr;
		uint64_t phys_end = phys_start + memory_region->memory_size;

		/* Skip empty / unplugged slots */
		if (memory_region->memory_size == 0)
			continue;

		if ((addr >= phys_start) && (addr < phys_end)) {
#ifdef SMOLKVM_DEBUG
			printf("0x%llx is region %d\n", addr, i);
#endif
			return i;
		}
	}

	return -1;
}

static inline int __smolkvm_memory_read(const struct smolkvm_vm *vm, uint64_t addr, uint64_t len, void *dst)
{
	const struct kvm_userspace_memory_region *memory_region;
	int region;

	region = __smolkvm_find_memregion(vm, addr);
	if (region < 0)
		return -1;

	memory_region = &vm->memregions[region];
	memcpy(dst, SMOLKVM_MEMREGION_PTR(memory_region, addr), len);

	return 0;
}

static inline int __smolkvm_memory_write(const struct smolkvm_vm *vm, uint64_t addr, uint64_t len, const void *src)
{
	const struct kvm_userspace_memory_region *memory_region;
	int region;

	region = __smolkvm_find_memregion(vm, addr);
	if (region < 0)
		return -1;

	memory_region = &vm->memregions[region];
	memcpy(SMOLKVM_MEMREGION_PTR(memory_region, addr), src, len);

	return 0;
}

#endif
/* -- */

#ifdef SMOLKVM_WANT_GDB_STUB
static inline int __smolkvm_gdb_stub_start(struct smolkvm_vm *vm)
{
	struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;
	int port = 1234;
	int ret;

	ret = __smolkvm_create_server_socket(port);
	if (ret < 0)
		return ret;

	gdb_stub->listen_socket = ret;
	gdb_stub->stopped = true;

	return 0;
}

static inline int __smolkvm_gdb_stub_accept(struct smolkvm_vm *vm)
{
	int listen_socket = vm->gdb_stub.listen_socket;
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int conn_socket;
	int ret;

	while (true) {
		ret = accept4(listen_socket, (struct sockaddr *)&client_addr, &addr_len, O_NONBLOCK);
		if (ret < 0) {
			if (errno == EINTR)
				continue;

			printf("accept failed: %d, errno %d\n", ret, errno);
			return -errno;
		}
		break;
	}

	conn_socket = ret;
	__smolkvm_make_socket_trigger_sigio(conn_socket);

#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("accepted connection\n");
#endif

	vm->gdb_stub.conn_socket = conn_socket;

	return 0;
}

static inline int __smolkvm_gdb_wait_for_data(const struct smolkvm_vm *vm)
{
	struct pollfd pfd = {
		.fd = vm->gdb_stub.conn_socket,
		.events = POLLIN,
		.revents = 0,
	};
	int ret;

	ret = poll(&pfd, 1, -1);
	if (ret < 0)
		return -1;
	if (ret == 0)
		return 0;

	if (pfd.revents & POLLIN)
		return 1;

	return -1;
}

static inline int __smolkvm_gdb_stub_read_packet(struct smolkvm_vm *vm,
						  struct smolkvm_gdb_stub_pkt *pkt)
{
	int conn_socket = vm->gdb_stub.conn_socket;
	unsigned char buff[2049] = {0};
	unsigned char chk[3] = { 0 };
	/* Did we see the $ yet? */
	bool packet_started = false;
	unsigned int len = 0;
	int ret;

	while (len < (sizeof(buff) - 1)) {
		unsigned char sym;

		ret = read(conn_socket, &sym, 1);
		if (ret != 1)
			break;

		if (!packet_started) {
			if (sym == __smolkvm_gdb_stub_pktstart)
				packet_started = true;
#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
			else
				printf("packet not started, received something that wasn't start character: %c\n", sym);
#endif

			continue;
		}

		if (sym == __smolkvm_gdb_stub_pktend) {
			ret = read(conn_socket, &chk, sizeof(chk));
			break;
		}

		buff[len++] = sym;
	}

	if (!len)
		return 0;

#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("have %d bytes of packet data, raw data %s\n", len, buff);
#endif

	if (__smolkvm_gdb_stub_pkt_checksum(buff, len, chk)) {
		__smolkvm_gdb_stub_pkt_unpack(buff, len, pkt);
		ret = write(conn_socket, &__smolkvm_gdb_stub_ack, 1);
		return 1;
	}
	else
		ret = write(conn_socket, &__smolkvm_gdb_stub_nak, 1);

	return 0;
}

#define __smolkvm_gdb_stub_write_const(_sock, _const) \
	write(_sock, &_const, sizeof(_const))

/*
 * This is a workaround to the problem with nolibc's sprintf() not being able to
 * do left padding yet. I guess it might be a bit quicker too?
 */
static inline unsigned char __smolkvm_gdb_stub_nibbletohex(uint8_t nibble)
{
	const unsigned char table[] = {'0', '1', '2', '3', '4', '5', '6', '7',
				       '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	return table[nibble];
}

static inline void __smolkvm_gdb_u8tohex(char *output, uint8_t value)
{
	*output++ = __smolkvm_gdb_stub_nibbletohex((value >> 4) & 0xf);
	*output = __smolkvm_gdb_stub_nibbletohex(value & 0xf);
}

/* FIXME: change argument order? use the u8 function in the loop ? */
static inline void __smolkvm_gdb_u64tohex(uint64_t value, char *output)
{
	int i;

	for (i = 0; i < 64; i += 8) {
		uint8_t byte = (value >> i) & 0xff;
		__smolkvm_gdb_u8tohex(output, byte);
		output += 2;
	}
}

static inline void __smolkvm_gdb_u32tohex(uint32_t value, char *output)
{
	int i;

	for (i = 0; i < 32; i += 8) {
		uint8_t byte = (value >> i) & 0xff;
		__smolkvm_gdb_u8tohex(output, byte);
		output += 2;
	}
}

static inline void __smolkvm_gdb_stub_send_packet(struct smolkvm_vm *vm,
						  const char *data,
						  unsigned int len)
{
	int conn_socket = vm->gdb_stub.conn_socket;
	unsigned char chk[3] = { 0 };
	unsigned char resp;
	int ret;

	__smolkvm_gdb_u8tohex(chk, __smolkvm_gdb_stub_checksum(data, len));

	ret = __smolkvm_gdb_stub_write_const(conn_socket, __smolkvm_gdb_stub_pktstart);
	if (ret != 1)
		return;

	ret = write(conn_socket, data, len);
	if (ret != len){
		printf("whelp\n");
	}

	ret = __smolkvm_gdb_stub_write_const(conn_socket, __smolkvm_gdb_stub_pktend);

	ret = write(conn_socket, chk, sizeof(chk) - 1);


	__smolkvm_gdb_wait_for_data(vm);
	ret = read(conn_socket, &resp, 1);
	if (ret != 1)
		return;

#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("response to sent packet: \'%c\'\n", resp);
#endif
}

static inline int __smolkvm_gdb_stub_process_packet_stop_reason(struct smolkvm_vm *vm)
{
	__smolkvm_gdb_stub_send_packet(vm, "S05", 3);
	return 1;
}

#define __smolkvm_gdb_stub_encode_reg64(_head, _regval)	\
	do {						\
		__smolkvm_gdb_u64tohex(_regval, _head);	\
		_head += 16;				\
	} while(0)

#define __smolkvm_gdb_stub_encode_reg32(_head, _regval)	\
	do {						\
		__smolkvm_gdb_u32tohex(_regval, _head);	\
		_head += 8;				\
	} while(0)

static inline int __smolkvm_gdb_stub_process_packet_read_regs(struct smolkvm_vm *vm)
{
	struct kvm_regs regs = { 0 };
	struct kvm_sregs sregs = { 0 };
	unsigned char buf[(17 * 16) + (7 * 8)];
	unsigned char *head = buf;
	int ret, pos = 0;

	ret = __smolkvm_get_regs(vm, &regs);
	if (ret) {
		printf("failed to read regs %d\n", errno);
	}

	ret = __smolkvm_get_sregs(vm, &sregs);
	if (ret) {
		printf("failed to read special regs %d\n", errno);
		//return -1;
	}

	memset(buf, '0', sizeof(buf));
	__smolkvm_gdb_stub_encode_reg64(head, regs.rax);
	__smolkvm_gdb_stub_encode_reg64(head, regs.rbx);
	__smolkvm_gdb_stub_encode_reg64(head, regs.rcx);
	__smolkvm_gdb_stub_encode_reg64(head, regs.rdx);
	__smolkvm_gdb_stub_encode_reg64(head, regs.rsi);
	__smolkvm_gdb_stub_encode_reg64(head, regs.rdi);
	__smolkvm_gdb_stub_encode_reg64(head, regs.rbp);
	__smolkvm_gdb_stub_encode_reg64(head, regs.rsp);

	__smolkvm_gdb_stub_encode_reg64(head, regs.r8);
	__smolkvm_gdb_stub_encode_reg64(head, regs.r9);
	__smolkvm_gdb_stub_encode_reg64(head, regs.r10);
	__smolkvm_gdb_stub_encode_reg64(head, regs.r11);
	__smolkvm_gdb_stub_encode_reg64(head, regs.r12);
	__smolkvm_gdb_stub_encode_reg64(head, regs.r13);
	__smolkvm_gdb_stub_encode_reg64(head, regs.r14);
	__smolkvm_gdb_stub_encode_reg64(head, regs.r15);

	__smolkvm_gdb_stub_encode_reg64(head, regs.rip);

	__smolkvm_gdb_stub_encode_reg32(head, regs.rflags);

	__smolkvm_gdb_stub_encode_reg32(head, sregs.cs.selector);
	__smolkvm_gdb_stub_encode_reg32(head, sregs.ss.selector);
	__smolkvm_gdb_stub_encode_reg32(head, sregs.ds.selector);
	__smolkvm_gdb_stub_encode_reg32(head, sregs.es.selector);
	__smolkvm_gdb_stub_encode_reg32(head, sregs.fs.selector);
	__smolkvm_gdb_stub_encode_reg32(head, sregs.gs.selector);

	__smolkvm_gdb_stub_send_packet(vm, buf, sizeof(buf));
	return 1;
}

static inline int __smolkvm_gdb_stub_process_packet_read_mem(struct smolkvm_vm *vm,
							      struct smolkvm_gdb_stub_pkt *pkt)
{
	uint64_t addr = pkt->read_mem.addr;
	uint64_t len = pkt->read_mem.len;
	char hexbuff[256] = { 0 };
	uint8_t buff[128];
	int i;

#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("read memory: addr=0x%llx len=%lld\n", addr, len);
#endif

	__smolkvm_memory_read(vm, addr, len, buff);
	for (i = 0; i < len; i++)
		__smolkvm_gdb_u8tohex(&hexbuff[i * 2], buff[i]);

	__smolkvm_gdb_stub_send_packet(vm, hexbuff, len * 2);

	return 1;
}

static inline int __smolkvm_gdb_stub_process_packet_query(struct smolkvm_vm *vm,
						  struct smolkvm_gdb_stub_pkt *pkt)
{
	struct smolkvm_gdb_stub_pkt_query *query = &pkt->query;

	#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("processing query packet, subtype: %d\n", query->subtype);
	#endif

	switch (query->subtype) {
	case SMOLKVM_GDB_STUB_QUERY_ATTACHED:
		__smolkvm_gdb_stub_send_packet(vm, "1", 1);
		return 1;
	case SMOLKVM_GDB_STUB_QUERY_SUPPORTED:
		const char pkt_string[] = "PacketSize=2048;qXfer:features:read+;arch=i386:x86-64";
		__smolkvm_gdb_stub_send_packet(vm, pkt_string, strlen(pkt_string));
		return 1;
	case SMOLKVM_GDB_STUB_QUERY_XFER_FEATURES:
		const char target_xml[] = "l" SMOLKVM_GDB_TARGET_XML;
		__smolkvm_gdb_stub_send_packet(vm, target_xml, strlen(target_xml));
		return 1;
	}

	return 0;
}

static inline int __smolkvm_gdb_stub_process_packet_h(struct smolkvm_vm *vm,
						  struct smolkvm_gdb_stub_pkt *pkt)
{
	struct smolkvm_gdb_stub_pkt_h *h = &pkt->h;

	#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("processing H packet, subtype: %d\n", h->subtype);
	#endif

	__smolkvm_gdb_stub_send_packet(vm, "OK", 2);
	return 1;
}

static inline int __smolkvm_gdb_stub_process_packet_v(struct smolkvm_vm *vm,
						  struct smolkvm_gdb_stub_pkt *pkt)
{
	struct smolkvm_gdb_stub_pkt_v *v = &pkt->v;

	#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("processing v packet, subtype: %d\n", v->subtype);
	#endif

	switch (v->subtype) {
	//case SMOLKVM_GDB_STUB_QUERY_SUPPORTED:
	//	break;
	case SMOLKVM_GDB_STUB_V_MUSTREPLYEMPTY:
	default:
		/* Don't do anything, let an empty packet get sent. */
		break;
	}
	return 0;
}

static inline int __smolkvm_gdb_stub_process_packet_continue(struct smolkvm_vm *vm)
{
	struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;

	gdb_stub->stopped = false;
	gdb_stub->single_stepping = false;

	__smolkvm_gdb_stub_send_packet(vm, "S05", 3);

	return 1;
}

static inline int __smolkvm_gdb_stub_process_packet_step(struct smolkvm_vm *vm)
{
	struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;

	gdb_stub->stopped = false;
	gdb_stub->single_stepping = true;

	__smolkvm_gdb_stub_send_packet(vm, "S05", 3);

	return 1;
}

static inline void __smolkvm_gdb_stub_process_packet(struct smolkvm_vm *vm,
						  struct smolkvm_gdb_stub_pkt *pkt)
{
	/*
	 * <0 means there was an error,
	 *  0 means the packet wasn't handled
	 *  1 means the packet was handled
	 */
	int ret;

#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("processing packet, type: %d\n", pkt->type);
#endif

	switch(pkt->type) {
	case SMOLKVM_GDB_STUB_STOP_REASON:
		ret = __smolkvm_gdb_stub_process_packet_stop_reason(vm);
		break;
	case SMOLKVM_GDB_STUB_READ_REGS:
		ret = __smolkvm_gdb_stub_process_packet_read_regs(vm);
		break;
	case SMOLKVM_GDB_STUB_READ_MEM:
		ret = __smolkvm_gdb_stub_process_packet_read_mem(vm, pkt);
		break;
	case SMOLKVM_GDB_STUB_CONTINUE:
		ret = __smolkvm_gdb_stub_process_packet_continue(vm);
		break;
	case SMOLKVM_GDB_STUB_STEP:
		ret = __smolkvm_gdb_stub_process_packet_step(vm);
		break;
	case SMOLKVM_GDB_STUB_QUERY:
		ret = __smolkvm_gdb_stub_process_packet_query(vm, pkt);
		break;
	case SMOLKVM_GDB_STUB_H:
		ret = __smolkvm_gdb_stub_process_packet_h(vm, pkt);
		break;
	case SMOLKVM_GDB_STUB_V:
		ret = __smolkvm_gdb_stub_process_packet_v(vm, pkt);
		break;
	default:
		ret = 0;
#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
		printf("unhandled packet type\n");
#endif
		break;
	}

	if (ret < 0) {
		printf("error handling packet: %d\n", ret);
		return;
	}

	if (ret == 0)
		__smolkvm_gdb_stub_send_packet(vm, NULL, 0);
}

static inline void __smolkvm_gdb_stub_stop(struct smolkvm_vm *vm)
{

}
#endif /* SMOLKVM_WANT_GDB_STUB */

/* Early CPU init stuff, switch to longmode, initial guest page tables etc */
#ifdef SMOLKVM_FOLD

/* Starting memory is one huge page */
#define SMOLKVM_BASEMEMORY_PHYS_START (SMOLKVM_SZ_1MB * 2)
#define SMOLKVM_BASEMEMORY_SZ         (SMOLKVM_SZ_1MB * 2)
#define SMOLKVM_BASEMEMORY_PHYS_END   (SMOLKVM_BASEMEMORY_PHYS_START + SMOLKVM_BASEMEMORY_SZ)
#define SMOLKVM_PAGETABLE_OFF         (SMOLKVM_BASEMEMORY_SZ - (SMOLKVM_SZ_4K * 4))
#define SMOLKVM_PAGETABLE_PHYS        (SMOLKVM_BASEMEMORY_PHYS_END - (SMOLKVM_SZ_4K * 4))

static inline void __smolkvm_create_initial_pagetables(struct smolkvm_vm *vm)
{
	void *memory = (void *) (vm->memregions[0].userspace_addr);
	struct smolkvm_pgtable *pgtables =
		(struct smolkvm_pgtable *)(memory +  SMOLKVM_PAGETABLE_OFF);
	struct smolkvm_pgtable *pml4 = &pgtables[0];
	struct smolkvm_pgtable *pdpt = &pgtables[1];
	struct smolkvm_pgtable *pd = &pgtables[2];

	uint64_t *root = &pml4->entries[0];
	uint64_t *onegb = &pdpt->entries[0];
	uint64_t *mmio = &pd->entries[0];
	uint64_t *mem = &pd->entries[1];

	__smolkvm_debug("Page tables phy 0x%016llx, offset 0x%016llx\n",
	       SMOLKVM_PAGETABLE_PHYS, SMOLKVM_PAGETABLE_OFF);

	*root  = SMOLKVM_PTE_ADDR(SMOLKVM_PAGETABLE_PHYS + SMOLKVM_SZ_4K)
	       | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;
	*onegb = SMOLKVM_PTE_ADDR(SMOLKVM_PAGETABLE_PHYS + (SMOLKVM_SZ_4K * 2))
	       | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;
	*mmio  = SMOLKVM_PTE_ADDR(0)
	       | SMOLKVM_PTE_PS | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;
	*mem   = SMOLKVM_PTE_ADDR(SMOLKVM_BASEMEMORY_PHYS_START)
	       | SMOLKVM_PTE_PS | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;

	vm->pgtables = pgtables;
}

#define SMOLKVM_SEG_TYPE_CODE_EXEC_READ 11
#define SMOLKVM_SEG_TYPE_DATA_READ_WRITE 3

static inline int __smolkvm_switch_cpu_into_longmode(struct smolkvm_vm *vm)
{
	const struct kvm_segment cs = {
		.base = 0,
		.limit = 0xFFFFFFFF,
		.selector = 0x8,
		.type = SMOLKVM_SEG_TYPE_CODE_EXEC_READ,
		.present = 1,
		.s = 1,
		.l = 1,
		.g = 1,
	};
	const struct kvm_segment ds = {
		.base = 0,
		.limit = 0xFFFFFFFF,
		.selector = 0x10,
		.type = SMOLKVM_SEG_TYPE_DATA_READ_WRITE,
		.present = 1,
		.s = 1,
		.l = 0,
		.g = 1,
	};
	struct kvm_sregs sregs = { 0 };
	int ret;

	ret = __smolkvm_get_sregs(vm, &sregs);
	if (ret)
		return -SMOLKVM_ERR_CREATE_GETSREGS;

	printf("Special registers before longmode setup\n");
	__smolkvm_debug_dump_sregs(&sregs);

	/* Use the pagetables we put at the end of memory */
	sregs.cr3 = SMOLKVM_PAGETABLE_PHYS;

	/* Protected mode, long mode, all of that fun stuff */
	sregs.cr0 |= SMOLKVM_X86_CR0_PE | SMOLKVM_X86_CR0_PG;
	sregs.cr4 |= SMOLKVM_X86_CR4_PSE | SMOLKVM_X86_CR4_PAE;
	sregs.efer |= SMOLKVM_X86_EFER_LMA | SMOLKVM_X86_EFER_LME;

	/* Setup the code and data segments */
	sregs.cs = cs;
	sregs.ds = sregs.es = sregs.fs = sregs.gs = sregs.ss = ds;

	__smolkvm_debug("Special registers after longmode setup\n");
	__smolkvm_debug_dump_sregs(&sregs);

	ret = ioctl(vm->vcpu_fd, KVM_SET_SREGS, &sregs);
	if (ret) {
		printf("%d\n", errno);
		return -SMOLKVM_ERR_CREATE_SETSREGS;
	}

	return 0;
}

int __smolkvm_set_rip(struct smolkvm_vm *vm)
{
	struct kvm_regs regs = { 0 };
	int ret;

	ret = __smolkvm_get_regs(vm, &regs);
	if (ret)
		return -SMOLKVM_ERR_CREATE_GETREGS;

	regs.rip = SMOLKVM_BASEMEMORY_PHYS_START;
	regs.rflags = 0x2;

	ret = __smolkvm_set_regs(vm, &regs);
	if (ret)
		return -SMOLKVM_ERR_CREATE_SETREGS;

	return 0;
}
#endif
/* -- */

/* Plugging memory into the guest, TODO removing */
#ifdef SMOLKVM_FOLD

static inline int __smolkvm_setmemory(struct smolkvm_vm *vm, int which)
{
	struct kvm_userspace_memory_region *memory_region;
	int ret;

	memory_region = &vm->memregions[which];

	ret = ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, memory_region);
	if (ret)
		printf("%d\n", errno);

	return ret;
}

#endif
/* -- */

/* Memory map dump */
#ifdef SMOLKVM_FOLD

enum __smolkvm_mapentry_type {
	__SMOLKVM_MAPENTRY_RAM,
	__SMOLKVM_MAPENTRY_MMIO,
};

struct __smolkvm_mapentry {
	uint64_t start;
	uint64_t len;
	uint64_t backing;
	const char *name;
	enum __smolkvm_mapentry_type type;
};

static inline void __smolkvm_print_human_size(uint64_t bytes)
{
	static const char *units[] = { "B", "KiB", "MiB", "GiB", "TiB" };
	unsigned int u = 0;

	while (bytes >= SMOLKVM_SZ_1K && (bytes % SMOLKVM_SZ_1K) == 0 &&
	       u < SMOLKVM_ARRAYSIZE(units) - 1) {
		bytes /= SMOLKVM_SZ_1K;
		u++;
	}

	printf("%llu %s", (unsigned long long)bytes, units[u]);
}

void smolkvm_dump_memory_map(const struct smolkvm_vm *vm)
{
	struct __smolkvm_mapentry entries[SMOLKVM_MEMREGIONS_NUM + SMOLKVM_MMIOREGIONS_NUM];
	unsigned int n = 0;
	unsigned int i, j;

	/* Gather the RAM regions */
	for (i = 0; i < SMOLKVM_ARRAYSIZE(vm->memregions); i++) {
		const struct kvm_userspace_memory_region *r = &vm->memregions[i];

		if (r->memory_size == 0)
			continue;

		entries[n].start   = r->guest_phys_addr;
		entries[n].len     = r->memory_size;
		entries[n].backing = r->userspace_addr;
		entries[n].name    = NULL;
		entries[n].type    = __SMOLKVM_MAPENTRY_RAM;
		n++;
	}

	/* Gather the MMIO devices */
	for (i = 0; i < SMOLKVM_ARRAYSIZE(vm->mmioregions); i++) {
		const struct smolkvm_mmio *m = vm->mmioregions[i];

		if (!m)
			continue;

		entries[n].start   = m->phys;
		entries[n].len     = m->len;
		entries[n].backing = 0;
		entries[n].name    = m->name;
		entries[n].type    = __SMOLKVM_MAPENTRY_MMIO;
		n++;
	}

	/* Insertion sort by start address so the map reads top-down */
	for (i = 1; i < n; i++) {
		struct __smolkvm_mapentry tmp = entries[i];

		for (j = i; j > 0 && entries[j - 1].start > tmp.start; j--)
			entries[j] = entries[j - 1];

		entries[j] = tmp;
	}

	printf("Guest memory map (%u region%s):\n", n, n == 1 ? "" : "s");

	for (i = 0; i < n; i++) {
		const struct __smolkvm_mapentry *e = &entries[i];
		uint64_t end = e->start + e->len - 1;

		printf("  [%s] 0x%016llx - 0x%016llx (",
		       e->type == __SMOLKVM_MAPENTRY_RAM ? "ram " : "mmio",
		       (unsigned long long)e->start,
		       (unsigned long long)end);
		__smolkvm_print_human_size(e->len);
		printf(")");

		if (e->type == __SMOLKVM_MAPENTRY_RAM)
			printf(" -> host 0x%016llx", (unsigned long long)e->backing);
		else
			printf(" %s", e->name ? e->name : "(unnamed)");

		printf("\n");
	}
}

#endif
/* -- */

/* Adding RAM to a running guest */
#ifdef SMOLKVM_FOLD

/*
 * Allocate `size` bytes of host memory and plug it into the guest at guest
 * physical address `gpa` as a fresh KVM memslot. This is the low level "add
 * memory" primitive: it makes the GPA range *backable*, but the guest still
 * needs page tables covering it before it can actually touch the range.
 */
int smolkvm_map_memory(struct smolkvm_vm *vm, uint64_t gpa, uint64_t size)
{
	struct kvm_userspace_memory_region *memory_region;
	unsigned int slot;
	int free_slot;
	void *memory;
	int ret;

	free_slot = __smolkvm_find_free_memregion_slot(vm);
	if (free_slot < 0) {
		printf("no free memory slots, cannot map 0x%llx bytes at 0x%llx\n",
			   (unsigned long long) size, (unsigned long long) gpa);
		return -1;
	}
	slot = (unsigned int) free_slot;

	/* KVM requires the size to be a non-zero multiple of the page size */
	if (size == 0 || (size & (SMOLKVM_SZ_4K - 1))) {
		printf("size 0x%llx is not a non-zero multiple of page size\n",
			   (unsigned long long) size);
		return -1;
	}

	memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
				  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (memory == MAP_FAILED) {
		printf("failed to mmap 0x%llx bytes of guest memory\n",
			   (unsigned long long) size);
		return -1;
	}

	memset(memory, 0, size);

	memory_region = &vm->memregions[slot];

	memory_region->slot = slot;
	memory_region->guest_phys_addr = gpa;
	memory_region->memory_size = size;
	memory_region->userspace_addr = (uint64_t) memory;

	ret = __smolkvm_setmemory(vm, slot);
	if (ret) {
		printf("KVM_SET_USER_MEMORY_REGION failed for slot %u\n", slot);
		munmap(memory, size);
		return -1;
	}

	vm->memory_region_plugged_in++;

	__smolkvm_debug("mapped 0x%llx bytes at gpa 0x%016llx (slot %u, host %p)\n",
					(unsigned long long) size, (unsigned long long) gpa, slot, memory);

	return 0;
}

#endif
/* -- */

/* Mailbox device: guest -> host command channel */
#ifdef SMOLKVM_FOLD

/*
 * A tiny MMIO "mailbox" the guest uses to ask the host to do things. The guest
 * writes the arguments, then writes a command code to the COMMAND register,
 * which runs the command synchronously (we are inside the MMIO write VMEXIT).
 * The result is then readable from the STATUS register.
 *
 * Register map (all registers are 64-bit):
 *   0x00  COMMAND  (write)  writing here runs the command in the value
 *   0x08  STATUS   (read)   result of the last command, 0 == OK
 *   0x10  ARG0     (r/w)
 *   0x18  ARG1     (r/w)
 */
#define SMOLKVM_MAILBOX_PHYS	0x2000
#define SMOLKVM_MAILBOX_LEN	0x20

#define __SMOLKVM_MAILBOX_REG_COMMAND	0x00
#define __SMOLKVM_MAILBOX_REG_STATUS	0x08
#define __SMOLKVM_MAILBOX_REG_ARG0	0x10
#define __SMOLKVM_MAILBOX_REG_ARG1	0x18

/* Commands the guest can write to the COMMAND register */
enum smolkvm_mailbox_command {
	SMOLKVM_MAILBOX_CMD_NOP		= 0,
	SMOLKVM_MAILBOX_CMD_MAP_MEMORY	= 1,	/* map ARG1 bytes of RAM at gpa ARG0 */
};

/* STATUS values */
#define SMOLKVM_MAILBOX_STATUS_OK	0
#define SMOLKVM_MAILBOX_STATUS_ERR	1
#define SMOLKVM_MAILBOX_STATUS_BADCMD	2

struct __smolkvm_mailbox_priv {
	uint64_t arg0;
	uint64_t arg1;
	uint64_t status;
};

static struct __smolkvm_mailbox_priv __smolkvm_mailbox_priv;

static uint64_t __smolkvm_mailbox_dispatch(struct smolkvm_vm *vm,
					   struct __smolkvm_mailbox_priv *mb,
					   uint64_t command)
{
	int ret;

	switch (command) {
	case SMOLKVM_MAILBOX_CMD_NOP:
		return SMOLKVM_MAILBOX_STATUS_OK;
	case SMOLKVM_MAILBOX_CMD_MAP_MEMORY:
		__smolkvm_debug("mailbox: MAP_MEMORY gpa 0x%016llx size 0x%llx\n",
						(unsigned long long) mb->arg0,
						(unsigned long long) mb->arg1);
		ret = smolkvm_map_memory(vm, mb->arg0, mb->arg1);
		return ret ? SMOLKVM_MAILBOX_STATUS_ERR : SMOLKVM_MAILBOX_STATUS_OK;
	default:
		printf("mailbox: unknown command %llu\n", (unsigned long long) command);
		return SMOLKVM_MAILBOX_STATUS_BADCMD;
	}
}

static void __smolkvm_mailbox_write(struct smolkvm_vm *vm,
									struct smolkvm_mmio *mmio,
									uint64_t offset,
									uint8_t len,
									uint64_t value)
{
	struct __smolkvm_mailbox_priv *mb = mmio->priv;

	switch (offset) {
	case __SMOLKVM_MAILBOX_REG_ARG0:
		mb->arg0 = value;
		break;
	case __SMOLKVM_MAILBOX_REG_ARG1:
		mb->arg1 = value;
		break;
	case __SMOLKVM_MAILBOX_REG_COMMAND:
		mb->status = __smolkvm_mailbox_dispatch(vm, mb, value);
		break;
	default:
		__smolkvm_debug("mailbox: write to unknown offset 0x%llx\n",
						(unsigned long long) offset);
		break;
	}
}

static uint64_t __smolkvm_mailbox_read(struct smolkvm_vm *vm,
									   struct smolkvm_mmio *mmio,
									   uint64_t offset,
									   uint8_t len)
{
	struct __smolkvm_mailbox_priv *mb = mmio->priv;

	switch (offset) {
	case __SMOLKVM_MAILBOX_REG_STATUS:
		return mb->status;
	case __SMOLKVM_MAILBOX_REG_ARG0:
		return mb->arg0;
	case __SMOLKVM_MAILBOX_REG_ARG1:
		return mb->arg1;
	}

	return 0;
}

static struct smolkvm_mmio __smolkvm_mailbox = {
	.name = "mailbox",
	.phys = SMOLKVM_MAILBOX_PHYS,
	.len = SMOLKVM_MAILBOX_LEN,
	.write = __smolkvm_mailbox_write,
	.read = __smolkvm_mailbox_read,
	.priv = &__smolkvm_mailbox_priv,
};

static inline int __smolkvm_mailbox_create(struct smolkvm_vm *vm)
{
	__smolkvm_mailbox_priv.arg0 = 0;
	__smolkvm_mailbox_priv.arg1 = 0;
	__smolkvm_mailbox_priv.status = 0;

	__smolkvm_plugin_mmio(vm, &__smolkvm_mailbox);

	return 0;
}

#endif
/* -- */

/* Interrupt controller: a very basic 64-line IRQ status/mask/ack device */
#ifdef SMOLKVM_FOLD

/*
 * A minimal MMIO interrupt controller for up to 64 interrupt lines. It is a
 * *polled* controller: it latches pending lines and exposes them to the guest,
 * but does not (yet) inject anything into the vCPU -- the guest reads STATUS to
 * see what needs servicing. The host (or another device) asserts a line with
 * smolkvm_irq_raise().
 *
 * Each line is one bit, so all 64 fit in a single 64-bit register.
 *
 * Register map (all registers are 64-bit):
 *   0x00  STATUS  (read)   asserted lines == pending & ~mask
 *   0x08  MASK    (r/w)    a 1 bit masks (suppresses) that line
 *   0x10  ACK     (write)  write a 1 bit to clear that pending line
 */
#define SMOLKVM_IRQCHIP_PHYS	0x3000
#define SMOLKVM_IRQCHIP_LEN	0x20
#define SMOLKVM_IRQCHIP_MAX_IRQ	64

#define __SMOLKVM_IRQCHIP_REG_STATUS	0x00
#define __SMOLKVM_IRQCHIP_REG_MASK	0x08
#define __SMOLKVM_IRQCHIP_REG_ACK	0x10

struct __smolkvm_irqchip_priv {
	uint64_t pending;	/* latched lines, set by raise, cleared by ACK */
	uint64_t mask;	/* bit set => line is masked (suppressed) */
};

static struct __smolkvm_irqchip_priv __smolkvm_irqchip_priv;

static uint64_t __smolkvm_irqchip_read(struct smolkvm_vm *vm,
									   struct smolkvm_mmio *mmio,
									   uint64_t offset,
									   uint8_t len)
{
	struct __smolkvm_irqchip_priv *ic = mmio->priv;

	switch (offset) {
	case __SMOLKVM_IRQCHIP_REG_STATUS:
		/* Only report lines that are pending *and* not masked */
		return ic->pending & ~ic->mask;
	case __SMOLKVM_IRQCHIP_REG_MASK:
		return ic->mask;
	}

	return 0;
}

static void __smolkvm_irqchip_write(struct smolkvm_vm *vm,
									struct smolkvm_mmio *mmio,
									uint64_t offset,
									uint8_t len,
									uint64_t value)
{
	struct __smolkvm_irqchip_priv *ic = mmio->priv;

	switch (offset) {
	case __SMOLKVM_IRQCHIP_REG_MASK:
		ic->mask = value;
		break;
	case __SMOLKVM_IRQCHIP_REG_ACK:
		/* Clear every pending line the guest acknowledged */
		ic->pending &= ~value;
		break;
	default:
		__smolkvm_debug("irqchip: write to read-only/unknown offset 0x%llx\n",
						(unsigned long long) offset);
		break;
	}
}

static struct smolkvm_mmio __smolkvm_irqchip = {
	.name = "irqchip",
	.phys = SMOLKVM_IRQCHIP_PHYS,
	.len = SMOLKVM_IRQCHIP_LEN,
	.write = __smolkvm_irqchip_write,
	.read = __smolkvm_irqchip_read,
	.priv = &__smolkvm_irqchip_priv,
};

/* Assert interrupt line `irq` (latches even if currently masked) */
void smolkvm_irq_raise(struct smolkvm_vm *vm, unsigned int irq)
{
	(void) vm;

	if (irq >= SMOLKVM_IRQCHIP_MAX_IRQ) {
		printf("irq %u out of range (max %d)\n", irq, SMOLKVM_IRQCHIP_MAX_IRQ);
		return;
	}

	__smolkvm_irqchip_priv.pending |= SMOLKVM_BIT(irq);
}

/* Deassert interrupt line `irq` without the guest having to ACK it */
void smolkvm_irq_lower(struct smolkvm_vm *vm, unsigned int irq)
{
	(void) vm;

	if (irq >= SMOLKVM_IRQCHIP_MAX_IRQ)
		return;

	__smolkvm_irqchip_priv.pending &= ~SMOLKVM_BIT(irq);
}

/* True if any line is asserted (pending and unmasked) -- the hook a real
 * delivery path would use to decide whether to inject into the vCPU. */
static inline bool smolkvm_irq_asserted(struct smolkvm_vm *vm)
{
	(void) vm;

	return (__smolkvm_irqchip_priv.pending & ~__smolkvm_irqchip_priv.mask) != 0;
}

static inline int __smolkvm_irqchip_create(struct smolkvm_vm *vm)
{
	__smolkvm_irqchip_priv.pending = 0;
	__smolkvm_irqchip_priv.mask = 0;	/* nothing masked by default */

	return __smolkvm_plugin_mmio(vm, &__smolkvm_irqchip);
}

#endif
/* -- */

/* Timer: free-running counter + programmable one-shot */
#ifdef SMOLKVM_FOLD

/*
 * One MMIO device providing two things:
 *
 *   - a free-running counter that advances in real time at a programmable
 *     frequency (think TSC / monotonic clock the guest can read), and
 *   - a programmable one-shot that fires once after a set number of ticks,
 *     raising an irqchip line (and latching an EXPIRED status bit).
 *
 * Time is read from the host's CLOCK_MONOTONIC. The one-shot is evaluated
 * lazily on every register read and around each vCPU run (pre/post hooks), so
 * in this polled model the guest sees it fire as soon as it next exits -- e.g.
 * when it polls STATUS or the irqchip.
 *
 * Register map (all registers are 64-bit):
 *   0x00  FREQ     (r/w)  counter ticks per second; writing restarts COUNTER
 *   0x08  COUNTER  (r)    free-running tick count since boot / last FREQ write
 *   0x10  ONESHOT  (r/w)  write N>0 to fire N ticks from now, 0 to disarm;
 *                         read returns ticks remaining (0 if disarmed/fired)
 *   0x18  STATUS   (r/w)  read bit0 = EXPIRED latch; write bit0 to clear it
 *   0x20  IRQ      (r/w)  irqchip line raised when the one-shot fires
 */
#define SMOLKVM_TIMER_PHYS		0x4000
#define SMOLKVM_TIMER_LEN		0x40
#define SMOLKVM_TIMER_DEFAULT_FREQ	1000000ULL	/* 1 MHz -> 1 tick == 1us */
#define SMOLKVM_TIMER_DEFAULT_IRQ	0

#define __SMOLKVM_TIMER_REG_FREQ	0x00
#define __SMOLKVM_TIMER_REG_COUNTER	0x08
#define __SMOLKVM_TIMER_REG_ONESHOT	0x10
#define __SMOLKVM_TIMER_REG_STATUS	0x18
#define __SMOLKVM_TIMER_REG_IRQ		0x20

#define SMOLKVM_TIMER_STATUS_EXPIRED	SMOLKVM_BIT(0)

#define __SMOLKVM_NS_PER_SEC		1000000000ULL

struct __smolkvm_timer_priv {
	uint64_t freq;		/* ticks per second */
	uint64_t base_ns;		/* monotonic ns where COUNTER reads zero */
	uint64_t deadline_ns;	/* monotonic ns when the one-shot fires */
	bool     armed;		/* one-shot armed */
	bool     fired;		/* one-shot expiry latch */
	unsigned int irq;		/* irqchip line raised on expiry */
};

static struct __smolkvm_timer_priv __smolkvm_timer_priv;

static inline uint64_t __smolkvm_now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (uint64_t) ts.tv_sec * __SMOLKVM_NS_PER_SEC + (uint64_t) ts.tv_nsec;
}

/* ns -> ticks, split to avoid overflow in the sub-second part */
static inline uint64_t __smolkvm_ns_to_ticks(uint64_t ns, uint64_t freq)
{
	uint64_t secs = ns / __SMOLKVM_NS_PER_SEC;
	uint64_t rem  = ns % __SMOLKVM_NS_PER_SEC;

	return secs * freq + (rem * freq) / __SMOLKVM_NS_PER_SEC;
}

/* ticks -> ns (freq must be non-zero) */
static inline uint64_t __smolkvm_ticks_to_ns(uint64_t ticks, uint64_t freq)
{
	uint64_t secs = ticks / freq;
	uint64_t rem  = ticks % freq;

	return secs * __SMOLKVM_NS_PER_SEC + (rem * __SMOLKVM_NS_PER_SEC) / freq;
}

/* Fire the one-shot if its deadline has passed. Idempotent. */
static void __smolkvm_timer_update(struct smolkvm_vm *vm, struct __smolkvm_timer_priv *t)
{
	if (t->armed && __smolkvm_now_ns() >= t->deadline_ns) {
		t->armed = false;
		t->fired = true;
		smolkvm_irq_raise(vm, t->irq);
	}
}

static uint64_t __smolkvm_timer_read(struct smolkvm_vm *vm,
				     struct smolkvm_mmio *mmio,
				     uint64_t offset,
				     uint8_t len)
{
	struct __smolkvm_timer_priv *t = mmio->priv;
	uint64_t now;

	__smolkvm_timer_update(vm, t);

	switch (offset) {
	case __SMOLKVM_TIMER_REG_FREQ:
		return t->freq;
	case __SMOLKVM_TIMER_REG_COUNTER:
		return __smolkvm_ns_to_ticks(__smolkvm_now_ns() - t->base_ns, t->freq);
	case __SMOLKVM_TIMER_REG_ONESHOT:
		if (!t->armed)
			return 0;
		now = __smolkvm_now_ns();
		if (now >= t->deadline_ns)
			return 0;
		return __smolkvm_ns_to_ticks(t->deadline_ns - now, t->freq);
	case __SMOLKVM_TIMER_REG_STATUS:
		return t->fired ? SMOLKVM_TIMER_STATUS_EXPIRED : 0;
	case __SMOLKVM_TIMER_REG_IRQ:
		return t->irq;
	}

	return 0;
}

static void __smolkvm_timer_write(struct smolkvm_vm *vm,
				  struct smolkvm_mmio *mmio,
				  uint64_t offset,
				  uint8_t len,
				  uint64_t value)
{
	struct __smolkvm_timer_priv *t = mmio->priv;

	switch (offset) {
	case __SMOLKVM_TIMER_REG_FREQ:
		if (value == 0) {
			printf("timer: frequency 0 ignored\n");
			break;
		}
		t->freq = value;
		t->base_ns = __smolkvm_now_ns();	/* restart the counter at the new rate */
		break;
	case __SMOLKVM_TIMER_REG_ONESHOT:
		if (value == 0) {			/* disarm */
			t->armed = false;
			break;
		}
		if (t->freq == 0) {
			printf("timer: cannot arm one-shot, frequency is 0\n");
			break;
		}
		t->deadline_ns = __smolkvm_now_ns() + __smolkvm_ticks_to_ns(value, t->freq);
		t->armed = true;
		t->fired = false;
		break;
	case __SMOLKVM_TIMER_REG_STATUS:
		if (value & SMOLKVM_TIMER_STATUS_EXPIRED)	/* write-1-to-clear */
			t->fired = false;
		break;
	case __SMOLKVM_TIMER_REG_IRQ:
		if (value >= SMOLKVM_IRQCHIP_MAX_IRQ) {
			printf("timer: irq %llu out of range\n", (unsigned long long) value);
			break;
		}
		t->irq = (unsigned int) value;
		break;
	default:
		__smolkvm_debug("timer: write to unknown offset 0x%llx\n",
						(unsigned long long) offset);
		break;
	}
}

/* Evaluated around every vCPU run so the one-shot can fire between exits */
static void __smolkvm_timer_tick(struct smolkvm_vm *vm, struct smolkvm_mmio *mmio)
{
	__smolkvm_timer_update(vm, mmio->priv);
}

static struct smolkvm_mmio __smolkvm_timer = {
	.name = "timer",
	.phys = SMOLKVM_TIMER_PHYS,
	.len = SMOLKVM_TIMER_LEN,
	.pre_run = __smolkvm_timer_tick,
	.post_run = __smolkvm_timer_tick,
	.write = __smolkvm_timer_write,
	.read = __smolkvm_timer_read,
	.priv = &__smolkvm_timer_priv,
};

static inline int __smolkvm_timer_create(struct smolkvm_vm *vm)
{
	__smolkvm_timer_priv.freq = SMOLKVM_TIMER_DEFAULT_FREQ;
	__smolkvm_timer_priv.base_ns = __smolkvm_now_ns();
	__smolkvm_timer_priv.deadline_ns = 0;
	__smolkvm_timer_priv.armed = false;
	__smolkvm_timer_priv.fired = false;
	__smolkvm_timer_priv.irq = SMOLKVM_TIMER_DEFAULT_IRQ;

	return __smolkvm_plugin_mmio(vm, &__smolkvm_timer);
}

#endif
/* -- */

/* ELF loading */
#ifdef SMOLKVM_FOLD

int smolkvm_load_elf(struct smolkvm_vm *vm, const void *elf_image)
{
	Elf64_Ehdr *ehdr = (Elf64_Ehdr *) elf_image;
	Elf64_Phdr *phdrs;
	int i;

	if (!(ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
	      ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
	      ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
	      ehdr->e_ident[EI_MAG3] == ELFMAG3))
	{
		 printf("ELF ident looks wrong\n");
		 return -1;
	}

	if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
		printf("Bad ELF class\n");
		return -1;
	}

	if (ehdr->e_machine != EM_X86_64) {
		printf("Bad ELF machine\n");
		return -1;
	}

	/* Good enough?? */
	phdrs = (Elf64_Phdr *) (elf_image + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum; i++) {
		const Elf64_Phdr *phdr = &phdrs[i];
		if (phdr->p_type == PT_LOAD) {
			const void *src = elf_image + phdr->p_offset;
			__smolkvm_memory_write(vm, phdr->p_paddr, phdr->p_filesz, src);
		}
	}

	return 0;
}

int smolkvm_load_elf_file(struct smolkvm_vm *vm, const char *elf_path)
{
	void *elf_image;
	int ret;
	int elf;

	elf = open(elf_path, O_RDONLY);
	if (elf < 0) {
		printf("Failed to open IPL ELF: %d\n", elf);
		return -1;
	}

	elf_image = malloc(SMOLKVM_BASEMEMORY_SZ);
	if (!elf_image) {
		printf("Failed to malloc() memory for IPL ELF image\n");
		return -1;
	}

	ret = read(elf, elf_image, SMOLKVM_BASEMEMORY_SZ);
	if (ret <= 0) {
		printf("Failed to read ELF image: %d\n", ret);
		return -1;
	}

	printf("Read %d bytes of ELF image\n", ret);

	ret = smolkvm_load_elf(vm, elf_image);
	if (ret)
		return -1;

	return 0;
}

#endif
/* -- */

/* VM creation and teardown */
#ifdef SMOLKVM_FOLD

int smolkvm_create_vm(struct smolkvm_vm *vm)
{
	struct kvm_userspace_memory_region *memory_region = &vm->memregions[0];
	int kvm_fd, vm_fd, vcpu_fd;
	struct kvm_run *vcpu_run;
	int kvm_run_sz;
	void *memory;
	int ret;

	kvm_fd = open("/dev/kvm", O_RDWR);
	if (kvm_fd < 0)
		return -SMOLKVM_ERR_OPENKVM;

	vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
	if (vm_fd < 0)
		return -SMOLKVM_ERR_CREATEVM;

	vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
	if (vcpu_fd < 0)
		return -SMOLKVM_ERR_CREATEVCPU;

	kvm_run_sz= ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (kvm_run_sz <= 0)
		return -SMOLKVM_ERR_VCPUMMAPSIZE;

	vcpu_run = mmap(NULL, kvm_run_sz, PROT_READ | PROT_WRITE,
			MAP_SHARED, vcpu_fd, 0);
	if (!vcpu_run)
		return -SMOLKVM_ERR_VCPUMMAP;

	memory = mmap(NULL, SMOLKVM_BASEMEMORY_SZ, PROT_READ | PROT_WRITE,
		       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (!memory)
		return SMOLKVM_ERR_ALLOCMEMORY;

	memset(memory, 0, SMOLKVM_BASEMEMORY_SZ);

	__smolkvm_debug("Memory from phys 0x%016llx -> 0x%016llx\n",
	       SMOLKVM_BASEMEMORY_PHYS_START, SMOLKVM_BASEMEMORY_PHYS_END);

	vm->kvm_fd = kvm_fd;
	vm->vm_fd = vm_fd;
	vm->vcpu_fd = vcpu_fd;
	vm->vcpu_run  = vcpu_run;
	vm->vcpu_run_sz = kvm_run_sz;

	/* Sync the registers each run */
	vm->vcpu_run->kvm_valid_regs = KVM_SYNC_X86_REGS | KVM_SYNC_X86_SREGS;

	/* Fill in the initial memory region */
	memory_region->slot = 0;
	memory_region->guest_phys_addr = SMOLKVM_BASEMEMORY_PHYS_START;
	memory_region->memory_size = SMOLKVM_BASEMEMORY_SZ;
	memory_region->userspace_addr = (uint64_t)memory;

	/* Plug in the initial memory */
	ret = __smolkvm_setmemory(vm, 0);
	if (ret)
		return -SMOLKVM_ERR_SETMEMORYREGION;

	/* We now have one RAM region plugged in (slot 0) */
	vm->memory_region_plugged_in = 1;

	/* Create the initial page tables */
	__smolkvm_create_initial_pagetables(vm);

	/* Looks like KVM is ready to go. Setup the CPU */
	ret = __smolkvm_switch_cpu_into_longmode(vm);
	if (ret)
		return ret;

	ret = __smolkvm_set_rip(vm);
	if (ret)
		return ret;

//
	__smolkvm_setup_sighandler();
//

	/* Plug in the console .. boop beep boop */
	__smolkvm_console_create(vm);

	/* Plug in the mailbox so the guest can ask us to do things */
	__smolkvm_mailbox_create(vm);

	/* Plug in the interrupt controller */
	__smolkvm_irqchip_create(vm);

	/* Plug in the timer */
	__smolkvm_timer_create(vm);

	return 0;
}

void smolkvm_destroy_vm(struct smolkvm_vm *vm)
{
	munmap(vm->vcpu_run, vm->vcpu_run_sz);
	close(vm->vcpu_fd);
	close(vm->vm_fd);
	close(vm->kvm_fd);
}

#endif
/* -- */

static const char *smolkvm_exit_reasons[] = {
	[KVM_EXIT_UNKNOWN] = "UNKNOWN",
	[KVM_EXIT_EXCEPTION] = "EXCEPTION",
	[KVM_EXIT_IO] = "IO",
	[KVM_EXIT_HYPERCALL] = "HYPERCALL",
	[KVM_EXIT_DEBUG] = "DEBUG",
	[KVM_EXIT_HLT] = "HLT",
	[KVM_EXIT_MMIO] = "MMIO",
	[KVM_EXIT_IRQ_WINDOW_OPEN] = "IRQ_WINDOW_OPEN",
	[KVM_EXIT_SHUTDOWN] = "SHUTDOWN",
	[KVM_EXIT_FAIL_ENTRY] = "FAIL_ENTRY",
	[KVM_EXIT_INTR] = "INTR",
	[KVM_EXIT_SET_TPR] = "SET_TPR",
	[KVM_EXIT_TPR_ACCESS] = "TPR_ACCESS",
	[KVM_EXIT_NMI] = "NMI",
	[KVM_EXIT_INTERNAL_ERROR] = "INTERNAL_ERROR",
	[KVM_EXIT_WATCHDOG] = "WATCHDOG",
	[KVM_EXIT_SYSTEM_EVENT] = "SYSTEM_EVENT",
	[KVM_EXIT_IOAPIC_EOI] = "IOAPIC_EOI",
};

int smolkvm_run(struct smolkvm_vm *vm)
{
	int ret;
	uint32_t exit_reason;

	ret = __smolkvm_run(vm);
	if (ret) {
		if (ret == 1) {
			vm->was_interrupted = true;
			return 0;
		}

		return ret;
	}

	exit_reason = vm->vcpu_run->exit_reason;

	__smolkvm_debug("exit: %s\n", smolkvm_exit_reasons[exit_reason]);
	__smolkvm_debug_dump_regs(&vm->vcpu_run->s.regs.regs);
	__smolkvm_debug_dump_sregs(&vm->vcpu_run->s.regs.sregs);

	switch (vm->vcpu_run->exit_reason) {
	case KVM_EXIT_FAIL_ENTRY:
		printf("VM entry failed! Reason: 0x%llx\n",
			vm->vcpu_run->fail_entry.hardware_entry_failure_reason);
		/* This is bad, abort abort! */
		return -1;
	case KVM_EXIT_HLT:
		break;
	case KVM_EXIT_SHUTDOWN:
		return -1;
	case KVM_EXIT_MMIO:
		ret = __smolkvm_handle_mmio(vm);
		if (ret)
			return ret;
	default:
		break;
	}

	return 0;
}

int smolkvm_single_step(struct smolkvm_vm *vm, bool on)
{
	const struct kvm_guest_debug single_step_on = {
		.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP,
	};
	const struct kvm_guest_debug single_step_off = {
		.control = 0,
	};
	int ret;

	ret = __smolkvm_set_guest_debug(vm, (on ? &single_step_on : &single_step_off));
	if (ret)
		return -1;

	return 0;
}

static bool __smolkvm_stopped(const struct smolkvm_vm *vm)
{
#ifdef SMOLKVM_WANT_GDB_STUB
	const struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;

	return gdb_stub->stopped;
#else
	return false;
#endif
}

static bool __smolkvm_stop(struct smolkvm_vm *vm)
{
#ifdef SMOLKVM_WANT_GDB_STUB
	struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;

	gdb_stub->stopped = true;
#endif
}

static int __smolkvm_default_loop_pre_run(struct smolkvm_vm *vm)
{
	struct smolkvm_mmio **mmio;

	__smolkvm_foreach_mmio(vm, mmio) {
		if ((*mmio)->pre_run)
			(*mmio)->pre_run(vm, *mmio);
	}

#ifdef SMOLKVM_WANT_GDB_STUB
	struct smolkvm_gdb_stub_pkt pkt;

	int pktret = __smolkvm_gdb_stub_read_packet(vm, &pkt);
	if (pktret == 1)
		__smolkvm_gdb_stub_process_packet(vm, &pkt);
#else
	return 0;
#endif
}

static bool __smolkvm_is_single_stepping(struct smolkvm_vm *vm)
{
#ifdef SMOLKVM_WANT_GDB_STUB
	const struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;

	return gdb_stub->single_stepping;
#else
	return false;
#endif
}

static int __smolkvm_default_loop_configure_run(struct smolkvm_vm *vm)
{
#ifdef SMOLKVM_WANT_GDB_STUB
	const struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;

	return smolkvm_single_step(vm, __smolkvm_is_single_stepping(vm));
#else
	return 0;
#endif
}

static int __smolkvm_default_loop_post_run(struct smolkvm_vm *vm)
{
	struct smolkvm_mmio **mmio;

	__smolkvm_foreach_mmio(vm, mmio) {
		if ((*mmio)->post_run)
			(*mmio)->post_run(vm, *mmio);
	}

#ifdef SMOLKVM_WANT_GDB_STUB
	__smolkvm_gdb_wait_for_data(vm);
#endif
}

/* Default loop for running the VM */
#ifdef SMOLKVM_FOLD

int smolkvm_default_loop(struct smolkvm_vm *vm)
{
	int ret;

	while (true) {
		/* Any pre-run work, like handling for GDB packets */
		__smolkvm_default_loop_pre_run(vm);

		/* Check if we should actually do the run or not */
		if (!__smolkvm_stopped(vm)) {
			/* Setup things like single step */
			ret = __smolkvm_default_loop_configure_run(vm);
			if (ret)
				return ret;

			/* Do the actual kvm run bit */
			ret = smolkvm_run(vm);
			if (ret)
				return ret;

			/* CHECKME set the stopped flag, what we actually should do here depends on the exit reason */
			if (__smolkvm_is_single_stepping(vm))
				__smolkvm_stop(vm);
		}

		/* Any post-run work, like handling GDB packets */
		__smolkvm_default_loop_post_run(vm);

		/* Clear any flags from the last run */
		vm->was_interrupted = false;
	}

	return ret;
}

#endif
/* -- */

#endif /* _SMOLKVM_H */
