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
#include <sys/stat.h>
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
 *   SMOLKVM_WANT_SIMPLE          -- Machine with the simple polled irqchip + timer (default).
 *   SMOLKVM_WANT_APIC            -- Machine with the in-kernel APIC + PIT (boots stock OSes).
 */
#ifdef SMOLKVM_FOLD

#ifndef SMOLKVM_MEMREGIONS_NUM
#define SMOLKVM_MEMREGIONS_NUM		8
#endif

#ifndef SMOLKVM_MMIOREGIONS_NUM
#define SMOLKVM_MMIOREGIONS_NUM		8
#endif

#ifndef SMOLKVM_MAILBOX_HANDLERS_NUM
#define SMOLKVM_MAILBOX_HANDLERS_NUM	16
#endif

/*
 * Machine type. Both machines have the console and mailbox; they differ only in
 * the interrupt + timer hardware:
 *
 *   SMOLKVM_WANT_SIMPLE -- the simple polled MMIO irqchip + timer, for
 *                          bare-metal guests (the default).
 *   SMOLKVM_WANT_APIC   -- the in-kernel APIC/IOAPIC/PIC + 8254 PIT that a
 *                          stock OS such as Linux expects.
 *
 * Define one; SIMPLE is assumed if neither is given.
 */
#if defined(SMOLKVM_WANT_SIMPLE) && defined(SMOLKVM_WANT_APIC)
#error "smolkvm: define only one of SMOLKVM_WANT_SIMPLE or SMOLKVM_WANT_APIC"
#endif
#if !defined(SMOLKVM_WANT_SIMPLE) && !defined(SMOLKVM_WANT_APIC)
#define SMOLKVM_WANT_SIMPLE
#endif
/* -- */

/*
 * Fixed locations of KVM's in-kernel controllers, so the IPL can build a MADT
 * that matches what KVM_CREATE_IRQCHIP actually emulates. These are the x86
 * architectural defaults.
 */
#ifdef SMOLKVM_WANT_APIC
#define SMOLKVM_APIC_LAPIC_BASE		0xFEE00000ULL
#define SMOLKVM_APIC_IOAPIC_BASE	0xFEC00000ULL
#define SMOLKVM_APIC_IOAPIC_GSI_BASE	0
#endif
/* -- */

/* Utility macros */
#define SMOLKVM_ARRAYSIZE(_a)	(sizeof(_a)/sizeof(_a[0]))
#define SMOLKVM_BIT(_bit)	(1ULL << _bit)
#define SMOLKVM_SZ_1K		1024ULL
#define SMOLKVM_SZ_4K		(SMOLKVM_SZ_1K * 4)
#define SMOLKVM_SZ_1MB		(SMOLKVM_SZ_1K * 1024)

/*
 * Where the MMIO devices live. This must be a 2MB aligned hole with no RAM in
 * it: an access only reaches us (as a KVM_EXIT_MMIO) if no memslot covers the
 * address. It must NOT be in low memory -- a stock OS assumes low memory is
 * RAM and pokes around in it (Linux reads the EBDA pointer at 0x40e, scans
 * 0xf0000+ for DMI/SMBIOS, ...), so low memory has to be backed by a memslot
 * and devices have to live elsewhere. The initial page tables identity map
 * one 2MB page here for the IPL.
 */
#define SMOLKVM_MMIO_HOLE_PHYS	0xD0000000ULL

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
#define SMOLKVM_ERR_MMIO_NO_DEVICE  112
#define SMOLKVM_ERR_UNHANDLED_EXIT  113
#define SMOLKVM_ERR_GET_CPUID       114
#define SMOLKVM_ERR_SET_CPUID       115
#define SMOLKVM_ERR_CREATE_IRQCHIP  116
#define SMOLKVM_ERR_CREATE_PIT      117

#endif
/* -- */

/* Debug printing, register dumping */
#ifdef SMOLKVM_FOLD

#ifdef SMOLKVM_DEBUG
#define __smolkvm_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define __smolkvm_debug(fmt, ...) do { } while (0)
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
#define __smolkvm_debug_dump_regs(_regs) do { (void)(_regs); } while (0)
#define __smolkvm_debug_dump_sregs(_sregs) do { (void)(_sregs); } while (0)
#endif

#endif
/* -- */

/* x86 arch defines */
#ifdef SMOLKVM_FOLD

#define SMOLKVM_X86_PTE_NUM	512
/* Special register bits */
#define SMOLKVM_X86_CR0_PE	SMOLKVM_BIT(0)
#define SMOLKVM_X86_CR0_NW	SMOLKVM_BIT(29)
#define SMOLKVM_X86_CR0_CD	SMOLKVM_BIT(30)
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

struct smolkvm_mmio_reg {
	const char *name;
	uint64_t offset;
	uint64_t size;
};

struct smolkvm_mmio {
	const char *name;
	uint64_t phys;
	uint64_t len;

	const struct smolkvm_mmio_reg *regs;
	size_t num_regs;

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

/* So the SIGIO handler can reach the (one) vCPU's shared run structure */
static struct kvm_run *volatile __smolkvm_sigio_kvm_run;

static inline void __smolkvm_signal_sigio(int signum)
{
	// Just exit the KVM run call.

	/*
	 * ...and make sure the *next* KVM_RUN returns right away too: if this
	 * signal lands after the loop has drained the sockets but before the
	 * ioctl enters the kernel, the EINTR alone is lost and an idle
	 * (HLTed) guest would sit in KVM_RUN with input already waiting.
	 * The loop clears the flag before polling the devices.
	 */
	if (__smolkvm_sigio_kvm_run)
		__smolkvm_sigio_kvm_run->immediate_exit = 1;
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

/* GDB stuff 1 */
#ifdef SMOLKVM_FOLD
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

static const unsigned char __smolkvm_gdb_pkt_query_attached[] = "Attached";
static const unsigned char __smolkvm_gdb_pkt_query_support[] = "Supported:";
static const unsigned char __smolkvm_gdb_pkt_query_xfer[] = "Xfer:features:read:";
static const unsigned char __smolkvm_gdb_pkt_v_cont[] = "Cont?";
static const unsigned char __smolkvm_gdb_pkt_v_mustreplyempty[] = "MustReplyEmpty";

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

static inline void __smolkvm_gdb_split_value_comma_value(const char* str, unsigned int len,
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
	(len >= (sizeof(_token) - 1) && \
	 memcmp(raw, _token, sizeof(_token) - 1) == 0)

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
#endif /* fold */

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

static inline int __smolkvm_plugin_mmio(struct smolkvm_vm *vm, struct smolkvm_mmio *mmio)
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
		printf("unhandled MMIO access to 0x%lx, stopping the VM\n", mmio_addr);

		return -SMOLKVM_ERR_MMIO_NO_DEVICE;
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

#ifdef SMOLKVM_WANT_APIC
/* Defined with the console below; returns true if it claimed the access */
static inline bool __smolkvm_uart_handle_io(struct smolkvm_vm *vm);
#endif

/*
 * Port IO. Only the legacy COM1 UART is emulated (SMOLKVM_WANT_APIC); every
 * other port reads as 0xFF / swallows writes, which is what a PC with nothing
 * on the bus looks like, so a stock OS probing for hardware (PCI config,
 * i8042, RTC, ...) concludes it is absent and moves on.
 */
static inline int __smolkvm_handle_io(struct smolkvm_vm *vm)
{
	struct kvm_run *run = vm->vcpu_run;
	bool is_in = run->io.direction == KVM_EXIT_IO_IN;
	uint8_t *data = (uint8_t *) run + run->io.data_offset;
	uint32_t total = (uint32_t) run->io.size * run->io.count;

	__smolkvm_debug("io %s port 0x%x, size %u, count %u\n",
			is_in ? "in" : "out", run->io.port, run->io.size, run->io.count);

#ifdef SMOLKVM_WANT_APIC
	if (__smolkvm_uart_handle_io(vm))
		return 0;
#endif

	if (is_in)
		memset(data, 0xFF, total);

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

static void __smolkvm_console_disconnect(void)
{
	if (__smolkvm_console_priv.connected_socket >= 0) {
		__smolkvm_debug("console client went away\n");
		close(__smolkvm_console_priv.connected_socket);
		__smolkvm_console_priv.connected_socket = -1;
	}
}

static void __smolkvm_console_check_for_data_from_socket(struct smolkvm_vm *vm,
							 struct smolkvm_mmio *mmio)
{
	int connected_socket = __smolkvm_console_priv.connected_socket;
	struct __smolkvm_console_fifo *fifo_rx = &__smolkvm_console_priv.fifo_rx;
	int ret;

	if (connected_socket < 0)
		return;

	while (!__smolkvm_fifo_is_full(fifo_rx)) {
		uint8_t *dst = &fifo_rx->fifo[fifo_rx->head];

		ret = read(connected_socket, dst, 1);
		if (ret == 0) {
			/* Orderly shutdown from the other end */
			__smolkvm_console_disconnect();
			return;
		}
		if (ret < 0)
			return;

		__smolkvm_debug("read %d from console socket\n", ret);
		fifo_rx->head = (fifo_rx->head + 1) % SMOLKVM_ARRAYSIZE(fifo_rx->fifo);
	}
}

/*
 * Push one guest transmitted byte out: to the connected console client if
 * there is one, and always mirrored to our own stdout so boot output is
 * visible without attaching to the socket.
 */
static void __smolkvm_console_tx_byte(uint8_t ch)
{
	int connected_socket = __smolkvm_console_priv.connected_socket;

	if (connected_socket >= 0) {
		ssize_t wret;

		/*
		 * Our own SIGIO (client data arriving) can interrupt the send,
		 * so retry EINTR. EAGAIN means the client isn't draining; drop
		 * the byte rather than block or drop the client (stdout still
		 * gets it). MSG_NOSIGNAL: a disappearing client must not
		 * SIGPIPE us; it shows up as an error and disconnects instead.
		 */
		do {
			wret = send(connected_socket, &ch, 1, MSG_NOSIGNAL);
		} while (wret < 0 && errno == EINTR);

		if (wret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
			__smolkvm_console_disconnect();
	}

	putchar(ch);
	fflush(stdout);
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
	/* A new client replaces any existing one; don't leak the old fd */
	__smolkvm_console_disconnect();
	/* Data arriving must raise SIGIO too, to kick us out of KVM_RUN */
	__smolkvm_make_socket_trigger_sigio(conn_socket);
	__smolkvm_console_priv.connected_socket = conn_socket;

	return 1;
}

#ifdef SMOLKVM_WANT_APIC
static void __smolkvm_uart_update_irq(struct smolkvm_vm *vm);
#endif

static void __smolkvm_console_pre_run(struct smolkvm_vm *vm,
				      struct smolkvm_mmio *mmio)
{
	__smolkvm_console_check_for_connection(vm, mmio);
	__smolkvm_console_check_for_data_from_socket(vm, mmio);
#ifdef SMOLKVM_WANT_APIC
	/* New rx data might need to interrupt the guest */
	__smolkvm_uart_update_irq(vm);
#endif
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
	if (offset == __SMOLKVM_CONSOLE_REG_TXRX && len == 1)
		__smolkvm_console_tx_byte((uint8_t) value);
}

static void __smolkvm_console_post_run(struct smolkvm_vm *vm,
				      struct smolkvm_mmio *mmio)
{
	__smolkvm_console_check_for_connection(vm, mmio);
}

#define SMOLKVM_CONSOLE_PHYS (SMOLKVM_MMIO_HOLE_PHYS + 0x0000)

static const struct smolkvm_mmio_reg __smolkvm_console_regs[] = {
	{ .name = "TXRX",   .offset = __SMOLKVM_CONSOLE_REG_TXRX,   .size = 1 },
	{ .name = "STATUS", .offset = __SMOLKVM_CONSOLE_REG_STATUS, .size = 1 },
};

static struct smolkvm_mmio __smolkvm_console = {
	.name = "console",
	.phys = SMOLKVM_CONSOLE_PHYS,
	.len = 8,
	.regs = __smolkvm_console_regs,
	.num_regs = SMOLKVM_ARRAYSIZE(__smolkvm_console_regs),
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

/* COM1: just enough 16550 for Linux's ttyS0 (SMOLKVM_WANT_APIC machines) */
#ifdef SMOLKVM_FOLD
#ifdef SMOLKVM_WANT_APIC

/*
 * Every x86 Linux kernel registers ttyS0 at 0x3f8/irq4 without being told
 * about it (SERIAL_PORT_DFNS), so emulating the legacy COM1 is the smallest
 * possible path to a guest console: no ACPI/DT description, no driver.
 * It shares the rx fifo and socket with the MMIO console above.
 *
 * Emulation notes:
 *  - The transmitter has no fifo and is always empty: a THR write goes
 *    straight out to the socket.
 *  - IER/LCR/MCR/SCR/DLL/DLM are plain latches; enough for the 8250 driver's
 *    existence test (IER readback + scratch register).
 *  - FCR is ignored and IIR never reports fifos, so the driver detects a
 *    plain 8250 and does single byte tx per THRE interrupt.
 *  - The interrupt is driven through KVM_IRQ_LINE as a level that follows
 *    "rx data available" / "THRE pending", which gives the in-kernel
 *    PIC/IOAPIC the edges it wants. THRE is a one-shot: raised by a THR
 *    write (or enabling IER.THRI), cleared when the guest sees it in IIR.
 */
#define SMOLKVM_UART_PORT	0x3f8
#define SMOLKVM_UART_IRQ	4

#define __SMOLKVM_UART_REG_RBR	0	/* DLAB=0: read rx / write tx */
#define __SMOLKVM_UART_REG_IER	1	/* DLAB=0 */
#define __SMOLKVM_UART_REG_IIR	2	/* read (write is FCR, ignored) */
#define __SMOLKVM_UART_REG_LCR	3
#define __SMOLKVM_UART_REG_MCR	4
#define __SMOLKVM_UART_REG_LSR	5
#define __SMOLKVM_UART_REG_MSR	6
#define __SMOLKVM_UART_REG_SCR	7

#define __SMOLKVM_UART_IER_RDI	0x01	/* rx data interrupt enable */
#define __SMOLKVM_UART_IER_THRI	0x02	/* tx holding empty interrupt enable */
#define __SMOLKVM_UART_IIR_NONE	0x01
#define __SMOLKVM_UART_IIR_THRE	0x02
#define __SMOLKVM_UART_IIR_RDI	0x04
#define __SMOLKVM_UART_LCR_DLAB	0x80
#define __SMOLKVM_UART_LSR_DR	0x01
#define __SMOLKVM_UART_LSR_THRE	0x20
#define __SMOLKVM_UART_LSR_TEMT	0x40
/* CTS + DSR + DCD: we are always "connected" */
#define __SMOLKVM_UART_MSR_LINES 0xB0

struct __smolkvm_uart_priv {
	uint8_t ier, lcr, mcr, scr, dll, dlm;
	bool thre_pending;	/* one-shot THRE interrupt source */
	bool irq_level;		/* what we last told KVM_IRQ_LINE */
};

static struct __smolkvm_uart_priv __smolkvm_uart_priv;

static void __smolkvm_irq_line(struct smolkvm_vm *vm, unsigned int irq, int level)
{
	struct kvm_irq_level irq_level = {
		.irq = irq,
		.level = level,
	};

	if (ioctl(vm->vm_fd, KVM_IRQ_LINE, &irq_level))
		__smolkvm_debug("KVM_IRQ_LINE failed: %d\n", errno);
}

static void __smolkvm_uart_update_irq(struct smolkvm_vm *vm)
{
	struct __smolkvm_uart_priv *u = &__smolkvm_uart_priv;
	bool level = false;

	if ((u->ier & __SMOLKVM_UART_IER_RDI) &&
	    !__smolkvm_fifo_is_empty(&__smolkvm_console_priv.fifo_rx))
		level = true;

	if ((u->ier & __SMOLKVM_UART_IER_THRI) && u->thre_pending)
		level = true;

	if (level != u->irq_level) {
		__smolkvm_irq_line(vm, SMOLKVM_UART_IRQ, level);
		u->irq_level = level;
	}
}

static uint8_t __smolkvm_uart_read8(struct smolkvm_vm *vm, uint16_t offset)
{
	struct __smolkvm_uart_priv *u = &__smolkvm_uart_priv;
	struct __smolkvm_console_fifo *fifo_rx = &__smolkvm_console_priv.fifo_rx;
	bool dlab = u->lcr & __SMOLKVM_UART_LCR_DLAB;
	uint8_t val = 0;

	switch (offset) {
	case __SMOLKVM_UART_REG_RBR:
		if (dlab)
			return u->dll;
		if (!__smolkvm_fifo_is_empty(fifo_rx))
			val = __smolkvm_fifo_consume(fifo_rx);
		__smolkvm_uart_update_irq(vm);
		return val;
	case __SMOLKVM_UART_REG_IER:
		return dlab ? u->dlm : u->ier;
	case __SMOLKVM_UART_REG_IIR:
		if ((u->ier & __SMOLKVM_UART_IER_RDI) &&
		    !__smolkvm_fifo_is_empty(fifo_rx))
			return __SMOLKVM_UART_IIR_RDI;
		if ((u->ier & __SMOLKVM_UART_IER_THRI) && u->thre_pending) {
			/* Reading IIR acknowledges the THRE interrupt */
			u->thre_pending = false;
			__smolkvm_uart_update_irq(vm);
			return __SMOLKVM_UART_IIR_THRE;
		}
		return __SMOLKVM_UART_IIR_NONE;
	case __SMOLKVM_UART_REG_LCR:
		return u->lcr;
	case __SMOLKVM_UART_REG_MCR:
		return u->mcr;
	case __SMOLKVM_UART_REG_LSR:
		val = __SMOLKVM_UART_LSR_THRE | __SMOLKVM_UART_LSR_TEMT;
		if (!__smolkvm_fifo_is_empty(fifo_rx))
			val |= __SMOLKVM_UART_LSR_DR;
		return val;
	case __SMOLKVM_UART_REG_MSR:
		return __SMOLKVM_UART_MSR_LINES;
	case __SMOLKVM_UART_REG_SCR:
		return u->scr;
	}

	return 0;
}

static void __smolkvm_uart_write8(struct smolkvm_vm *vm, uint16_t offset, uint8_t val)
{
	struct __smolkvm_uart_priv *u = &__smolkvm_uart_priv;
	bool dlab = u->lcr & __SMOLKVM_UART_LCR_DLAB;

	switch (offset) {
	case __SMOLKVM_UART_REG_RBR:
		if (dlab) {
			u->dll = val;
			return;
		}
		__smolkvm_console_tx_byte(val);
		/* The byte is "sent" instantly, so THR is empty again */
		u->thre_pending = true;
		__smolkvm_uart_update_irq(vm);
		return;
	case __SMOLKVM_UART_REG_IER:
		if (dlab) {
			u->dlm = val;
			return;
		}
		if ((val & __SMOLKVM_UART_IER_THRI) &&
		    !(u->ier & __SMOLKVM_UART_IER_THRI))
			u->thre_pending = true;
		u->ier = val & 0x0F;
		__smolkvm_uart_update_irq(vm);
		return;
	case __SMOLKVM_UART_REG_IIR:	/* FCR: no fifos here */
		return;
	case __SMOLKVM_UART_REG_LCR:
		u->lcr = val;
		return;
	case __SMOLKVM_UART_REG_MCR:
		u->mcr = val;
		return;
	case __SMOLKVM_UART_REG_SCR:
		u->scr = val;
		return;
	}
}

static inline bool __smolkvm_uart_handle_io(struct smolkvm_vm *vm)
{
	struct kvm_run *run = vm->vcpu_run;
	bool is_in = run->io.direction == KVM_EXIT_IO_IN;
	uint8_t *data = (uint8_t *) run + run->io.data_offset;
	uint16_t offset;
	uint32_t i;

	if (run->io.port < SMOLKVM_UART_PORT ||
	    run->io.port >= SMOLKVM_UART_PORT + 8 ||
	    run->io.size != 1)
		return false;

	offset = run->io.port - SMOLKVM_UART_PORT;

	for (i = 0; i < run->io.count; i++) {
		if (is_in)
			data[i] = __smolkvm_uart_read8(vm, offset);
		else
			__smolkvm_uart_write8(vm, offset, data[i]);
	}

	return true;
}

#endif /* SMOLKVM_WANT_APIC */
#endif
/* -- */

/* Read/Write into guest memory */
#ifdef SMOLKVM_FOLD

#define SMOLKVM_OFFSET_IN_MEMREGION(__memregion, __physaddr) \
	(__physaddr - __memregion->guest_phys_addr)

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
			printf("0x%llx is region %d\n", (unsigned long long) addr, i);
#endif
			return i;
		}
	}

	return -1;
}

static inline int __smolkvm_memory_check_bounds(uint64_t addr, uint64_t len, const struct kvm_userspace_memory_region *memory_region)
{
	if (len > (memory_region->guest_phys_addr + memory_region->memory_size) - addr)
		return -1;

	return 0;
}

static inline int __smolkvm_memory_read(const struct smolkvm_vm *vm, uint64_t addr, uint64_t len, void *dst)
{
	const struct kvm_userspace_memory_region *memory_region;
	int region;
	int ret;

	region = __smolkvm_find_memregion(vm, addr);
	if (region < 0)
		return -1;

	memory_region = &vm->memregions[region];

	ret = __smolkvm_memory_check_bounds(addr, len, memory_region);
	if (ret)
		return ret;

	memcpy(dst, SMOLKVM_MEMREGION_PTR(memory_region, addr), len);

	return 0;
}

static inline int __smolkvm_memory_write(const struct smolkvm_vm *vm, uint64_t addr, uint64_t len, const void *src)
{
	const struct kvm_userspace_memory_region *memory_region;
	int region;
	int ret;

	region = __smolkvm_find_memregion(vm, addr);
	if (region < 0)
		return -1;

	memory_region = &vm->memregions[region];

	ret = __smolkvm_memory_check_bounds(addr, len, memory_region);
	if (ret)
		return ret;

	memcpy(SMOLKVM_MEMREGION_PTR(memory_region, addr), src, len);

	return 0;
}

static inline int __smolkvm_memory_set(const struct smolkvm_vm *vm, uint64_t addr, uint64_t len, int byte)
{
	const struct kvm_userspace_memory_region *memory_region;
	int region;
	int ret;

	region = __smolkvm_find_memregion(vm, addr);
	if (region < 0)
		return -1;

	memory_region = &vm->memregions[region];

	ret = __smolkvm_memory_check_bounds(addr, len, memory_region);
	if (ret)
		return ret;

	memset(SMOLKVM_MEMREGION_PTR(memory_region, addr), byte, len);

	return 0;
}

int smolkvm_guest_read(struct smolkvm_vm *vm, uint64_t gpa, uint64_t len, void *dst)
{
	return __smolkvm_memory_read(vm, gpa, len, dst);
}

int smolkvm_guest_write(struct smolkvm_vm *vm, uint64_t gpa, uint64_t len, const void *src)
{
	return __smolkvm_memory_write(vm, gpa, len, src);
}

#endif
/* -- */

/* GDB stuff 2 */
#ifdef SMOLKVM_FOLD
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
		if (ret == 0)
			/* The debugger closed the connection */
			return -1;
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
			unsigned int got = 0;

			/*
			 * The trailer is exactly two hex digits; reading any
			 * more would steal bytes from a pipelined packet.
			 * They might not have arrived yet (non-blocking
			 * socket), so keep trying.
			 */
			while (got < 2) {
				ret = read(conn_socket, chk + got, 2 - got);
				if (ret == 0)
					return -1;
				if (ret > 0)
					got += ret;
				else if (errno != EAGAIN && errno != EINTR)
					break;
			}
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
	unsigned int i;

#ifdef SMOLKVM_WANT_GDB_STUB_DEBUG
	printf("read memory: addr=0x%llx len=%llu\n", (unsigned long long) addr, (unsigned long long) len);
#endif

	/*
	 * The requested length is whatever the debugger asked for; a short
	 * reply is fine (GDB re-requests the rest), overflowing the buffers
	 * with a remote-controlled length is not.
	 */
	if (len > sizeof(buff))
		len = sizeof(buff);

	if (__smolkvm_memory_read(vm, addr, len, buff)) {
		__smolkvm_gdb_stub_send_packet(vm, "E01", 3);
		return 1;
	}

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

	/*
	 * No reply here: the target is now *running*. The stop reply goes out
	 * when it actually stops (see __smolkvm_gdb_stub_stop()); answering
	 * S05 straight away makes GDB believe the target halted again.
	 */
	return 1;
}

static inline int __smolkvm_gdb_stub_process_packet_step(struct smolkvm_vm *vm)
{
	struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;

	gdb_stub->stopped = false;
	gdb_stub->single_stepping = true;

	/* As with continue: the stop reply is sent after the step happens */
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

/* The target just stopped (e.g. a single step completed): tell the debugger */
static inline void __smolkvm_gdb_stub_stop(struct smolkvm_vm *vm)
{
	__smolkvm_gdb_stub_send_packet(vm, "S05", 3);
}
#endif /* SMOLKVM_WANT_GDB_STUB */
#endif /* fold */

/* Early CPU init stuff, switch to longmode, initial guest page tables etc */
#ifdef SMOLKVM_FOLD

/*
 * Low memory: identity mapped RAM covering 0..2MB, present so a stock OS can
 * poke around in "the BIOS area" (EBDA pointer, DMI scan, sub 1MB trampoline
 * allocation, ...) without hitting unhandled MMIO exits. Reads as zeros.
 */
#define SMOLKVM_LOWMEMORY_PHYS_START  0ULL
#define SMOLKVM_LOWMEMORY_SZ          (SMOLKVM_SZ_1MB * 2)

/* Base (IPL) memory is one huge page */
#define SMOLKVM_BASEMEMORY_PHYS_START (SMOLKVM_SZ_1MB * 2)
#define SMOLKVM_BASEMEMORY_SZ         (SMOLKVM_SZ_1MB * 2)
#define SMOLKVM_BASEMEMORY_PHYS_END   (SMOLKVM_BASEMEMORY_PHYS_START + SMOLKVM_BASEMEMORY_SZ)
#define SMOLKVM_PAGETABLE_OFF         (SMOLKVM_BASEMEMORY_SZ - (SMOLKVM_SZ_4K * 4))
#define SMOLKVM_PAGETABLE_PHYS        (SMOLKVM_BASEMEMORY_PHYS_END - (SMOLKVM_SZ_4K * 4))
#define SMOLKVM_PAGETABLE_PDPT_PHYS   (SMOLKVM_PAGETABLE_PHYS + SMOLKVM_SZ_4K)
#define SMOLKVM_PAGETABLE_PD_PHYS     (SMOLKVM_PAGETABLE_PHYS + (SMOLKVM_SZ_4K * 2))
#define SMOLKVM_PAGETABLE_PD_MMIO_PHYS (SMOLKVM_PAGETABLE_PHYS + (SMOLKVM_SZ_4K * 3))
/* A real GDT lives in the page below the page tables */
#define SMOLKVM_GDT_OFF               (SMOLKVM_PAGETABLE_OFF - SMOLKVM_SZ_4K)
#define SMOLKVM_GDT_PHYS              (SMOLKVM_PAGETABLE_PHYS - SMOLKVM_SZ_4K)

static inline void __smolkvm_create_initial_pagetables(struct smolkvm_vm *vm)
{
	void *memory = (void *) (vm->memregions[0].userspace_addr);
	struct smolkvm_pgtable *pgtables =
		(struct smolkvm_pgtable *)(memory +  SMOLKVM_PAGETABLE_OFF);
	struct smolkvm_pgtable *pml4 = &pgtables[0];
	struct smolkvm_pgtable *pdpt = &pgtables[1];
	struct smolkvm_pgtable *pd = &pgtables[2];
	struct smolkvm_pgtable *pd_mmio = &pgtables[3];

	uint64_t *root = &pml4->entries[0];
	uint64_t *onegb = &pdpt->entries[0];
	uint64_t *mmiogb = &pdpt->entries[SMOLKVM_MMIO_HOLE_PHYS >> 30];
	uint64_t *lowmem = &pd->entries[0];
	uint64_t *mem = &pd->entries[1];
	uint64_t *mmio = &pd_mmio->entries[(SMOLKVM_MMIO_HOLE_PHYS >> 21) & 0x1FF];

	__smolkvm_debug("Page tables phy 0x%016llx, offset 0x%016llx\n",
	       SMOLKVM_PAGETABLE_PHYS, SMOLKVM_PAGETABLE_OFF);

	*root   = SMOLKVM_PTE_ADDR(SMOLKVM_PAGETABLE_PDPT_PHYS)
	        | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;
	*onegb  = SMOLKVM_PTE_ADDR(SMOLKVM_PAGETABLE_PD_PHYS)
	        | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;
	*mmiogb = SMOLKVM_PTE_ADDR(SMOLKVM_PAGETABLE_PD_MMIO_PHYS)
	        | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;
	*lowmem = SMOLKVM_PTE_ADDR(SMOLKVM_LOWMEMORY_PHYS_START)
	        | SMOLKVM_PTE_PS | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;
	*mem    = SMOLKVM_PTE_ADDR(SMOLKVM_BASEMEMORY_PHYS_START)
	        | SMOLKVM_PTE_PS | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;
	*mmio   = SMOLKVM_PTE_ADDR(SMOLKVM_MMIO_HOLE_PHYS)
	        | SMOLKVM_PTE_PS | SMOLKVM_PTE_RW | SMOLKVM_PTE_PRESENT;

	vm->pgtables = pgtables;
}

#define SMOLKVM_SEG_TYPE_CODE_EXEC_READ 11
#define SMOLKVM_SEG_TYPE_DATA_READ_WRITE 3

static inline int __smolkvm_switch_cpu_into_longmode(struct smolkvm_vm *vm)
{
	/*
	 * Selectors follow the 64-bit boot protocol's GDT layout: null,
	 * reserved, __BOOT_CS (0x10), __BOOT_DS (0x18).
	 */
	const struct kvm_segment cs = {
		.base = 0,
		.limit = 0xFFFFFFFF,
		.selector = 0x10,
		.type = SMOLKVM_SEG_TYPE_CODE_EXEC_READ,
		.present = 1,
		.s = 1,
		.l = 1,
		.g = 1,
	};
	const struct kvm_segment ds = {
		.base = 0,
		.limit = 0xFFFFFFFF,
		.selector = 0x18,
		.type = SMOLKVM_SEG_TYPE_DATA_READ_WRITE,
		.present = 1,
		.s = 1,
		.l = 0,
		.g = 1,
	};
	void *memory = (void *) (vm->memregions[0].userspace_addr);
	uint64_t *gdt = (uint64_t *) (memory + SMOLKVM_GDT_OFF);
	struct kvm_sregs sregs = { 0 };
	int ret;

	/*
	 * A real GDT backing the selectors above. KVM_SET_SREGS loads the
	 * descriptor caches directly so nothing strictly reads this, but a
	 * GDTR pointing at nothing is out of spec and anything reloading a
	 * segment before installing its own GDT would triple fault.
	 */
	gdt[0] = 0;			/* null */
	gdt[1] = 0;			/* reserved */
	gdt[2] = 0x00af9b000000ffffULL;	/* 0x10: code, exec/read, L, G */
	gdt[3] = 0x00cf93000000ffffULL;	/* 0x18: data, read/write, G */

	ret = __smolkvm_get_sregs(vm, &sregs);
	if (ret)
		return -SMOLKVM_ERR_CREATE_GETSREGS;

	sregs.gdt.base = SMOLKVM_GDT_PHYS;
	sregs.gdt.limit = 4 * 8 - 1;

	printf("Special registers before longmode setup\n");
	__smolkvm_debug_dump_sregs(&sregs);

	/* Use the pagetables we put at the end of memory */
	sregs.cr3 = SMOLKVM_PAGETABLE_PHYS;

	/* Protected mode, long mode, all of that fun stuff */
	sregs.cr0 |= SMOLKVM_X86_CR0_PE | SMOLKVM_X86_CR0_PG;
	/* The reset state has caches disabled; a stock OS never expects that */
	sregs.cr0 &= ~(SMOLKVM_X86_CR0_CD | SMOLKVM_X86_CR0_NW);
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

	/* KVM requires page alignment, and the guest can pass anything here */
	if (gpa & (SMOLKVM_SZ_4K - 1)) {
		printf("gpa 0x%llx is not page aligned\n",
			   (unsigned long long) gpa);
		return -1;
	}

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
		/*
		 * Clear the slot again: leaving it populated would make the
		 * guest-memory helpers copy through the mapping we are about
		 * to tear down.
		 */
		memset(memory_region, 0, sizeof(*memory_region));
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
 * submits a command as a single tagged-pointer write, then reads STATUS to see
 * the result. Submission runs synchronously (we are inside the MMIO write
 * VMEXIT).
 *
 * Register map (all registers are 64-bit). SUBMIT and STATUS share one packed
 * layout: [63:60] flags, [59:48] command, [47:0] guest pointer.
 *   0x00  SUBMIT  (write)  (flags << 60) | (command << 48) | guest_ptr -- runs
 *                          `command` against the buffer at guest_ptr. Flag bit
 *                          ASYNC marks that the guest accepts an async reply.
 *   0x08  STATUS  (read)   the command + pointer currently/last processed, with
 *                          flag bits DONE and SUCCESS in the top 4 bits.
 */
#define SMOLKVM_MAILBOX_PHYS	(SMOLKVM_MMIO_HOLE_PHYS + 0x1000)
#define SMOLKVM_MAILBOX_LEN	0x10

#define __SMOLKVM_MAILBOX_REG_SUBMIT	0x00
#define __SMOLKVM_MAILBOX_REG_STATUS	0x08

/*
 * SUBMIT and STATUS share one 64-bit layout:
 *   [63:60] flags     (4 bits)
 *   [59:48] command   (12 bits)
 *   [47:0]  guest ptr (48 bits -- the x86-64 canonical boundary, so a tagged
 *                      value is non-canonical and can't collide with a real VA)
 */
#define SMOLKVM_MAILBOX_CMD_SHIFT	48
#define SMOLKVM_MAILBOX_FLAGS_BITS	4
#define SMOLKVM_MAILBOX_FLAGS_SHIFT	(64 - SMOLKVM_MAILBOX_FLAGS_BITS)
#define SMOLKVM_MAILBOX_FLAGS_MASK	((1ULL << SMOLKVM_MAILBOX_FLAGS_BITS) - 1)
#define SMOLKVM_MAILBOX_CMD_MASK	((1ULL << (SMOLKVM_MAILBOX_FLAGS_SHIFT - SMOLKVM_MAILBOX_CMD_SHIFT)) - 1)
#define SMOLKVM_MAILBOX_PTR_MASK	((1ULL << SMOLKVM_MAILBOX_CMD_SHIFT) - 1)

/* Decode any SUBMIT/STATUS value into its fields */
#define SMOLKVM_MAILBOX_GET_FLAGS(v)	(((v) >> SMOLKVM_MAILBOX_FLAGS_SHIFT) & SMOLKVM_MAILBOX_FLAGS_MASK)
#define SMOLKVM_MAILBOX_GET_CMD(v)	(((v) >> SMOLKVM_MAILBOX_CMD_SHIFT) & SMOLKVM_MAILBOX_CMD_MASK)
#define SMOLKVM_MAILBOX_GET_PTR(v)	((v) & SMOLKVM_MAILBOX_PTR_MASK)

/* SUBMIT flags (top 4 bits of a submission) */
#define SMOLKVM_MAILBOX_FLAG_ASYNC	(1ULL << 0)	/* guest accepts an async reply */

/* STATUS flags (top 4 bits of the status register) */
#define SMOLKVM_MAILBOX_DONE		(1ULL << 0)	/* the shown command has finished */
#define SMOLKVM_MAILBOX_SUCCESS		(1ULL << 1)	/* ...and finished successfully */

/* Built-in commands */
enum smolkvm_mailbox_command {
	SMOLKVM_MAILBOX_CMD_NOP		= 0,
	SMOLKVM_MAILBOX_CMD_DIE		= 1,
	SMOLKVM_MAILBOX_CMD_RESET	= 2,
	/* buffer: struct smolkvm_mailbox_map_memory */
	SMOLKVM_MAILBOX_CMD_MAP_MEMORY	= 3,

	SMOLKVM_MAILBOX_CMD_MINUSER	= 16,
};

/* Command buffer for SMOLKVM_MAILBOX_CMD_MAP_MEMORY */
struct smolkvm_mailbox_map_memory {
	uint64_t gpa;
	uint64_t size;
};

/*
 * Cap on a single guest MAP_MEMORY request. The guest picks the size and the
 * host mmaps it, so without a cap the guest can ask for silly amounts and OOM
 * the host. 1GB is also everything the initial page tables can identity map.
 */
#ifndef SMOLKVM_MAILBOX_MAP_MEMORY_MAX
#define SMOLKVM_MAILBOX_MAP_MEMORY_MAX	(SMOLKVM_SZ_1MB * 1024)
#endif

/*
 * Handler return codes (distinct from the packed STATUS register). PENDING
 * means the handler took ownership of the request and will finish it later via
 * smolkvm_mailbox_complete().
 */
#define SMOLKVM_MAILBOX_STATUS_OK	0
#define SMOLKVM_MAILBOX_STATUS_ERR	1
#define SMOLKVM_MAILBOX_STATUS_BADCMD	2
#define SMOLKVM_MAILBOX_STATUS_PENDING	3

/*
 * Host-registered command handlers. A program embedding smolkvm registers a
 * callback for a command number with smolkvm_mailbox_register(); when the guest
 * submits that command, the callback runs synchronously with the decoded guest
 * pointer, and whatever it returns becomes STATUS.
 */
typedef uint64_t (*smolkvm_mailbox_handler_fn)(struct smolkvm_vm *vm,
		uint64_t command, void *buffer, void *priv);

struct smolkvm_mailbox_handler {
	bool used;
	uint64_t command;
	void *buffer;		/* host buffer the message is copied into */
	size_t buffer_size;	/* bytes to copy from the guest pointer */
	smolkvm_mailbox_handler_fn fn;
	void *priv;
};

struct __smolkvm_mailbox_priv {
	uint64_t status;		/* packed (flags, command, ptr) the guest reads */
	uint64_t inflight_command;	/* command currently being processed */
	uint64_t inflight_ptr;		/* its guest pointer */
	struct smolkvm_mailbox_handler handlers[SMOLKVM_MAILBOX_HANDLERS_NUM];
};

static struct __smolkvm_mailbox_priv __smolkvm_mailbox_priv;

static struct smolkvm_mailbox_handler *
__smolkvm_mailbox_find_handler(struct __smolkvm_mailbox_priv *mb, uint64_t command)
{
	unsigned int i;

	for (i = 0; i < SMOLKVM_ARRAYSIZE(mb->handlers); i++)
		if (mb->handlers[i].used && mb->handlers[i].command == command)
			return &mb->handlers[i];

	return NULL;
}

static uint64_t __smolkvm_mailbox_pack(uint64_t flags, uint64_t command, uint64_t ptr)
{
	return ((flags & SMOLKVM_MAILBOX_FLAGS_MASK) << SMOLKVM_MAILBOX_FLAGS_SHIFT)
	     | ((command & SMOLKVM_MAILBOX_CMD_MASK) << SMOLKVM_MAILBOX_CMD_SHIFT)
	     | (ptr & SMOLKVM_MAILBOX_PTR_MASK);
}

static uint64_t __smolkvm_mailbox_dispatch(struct smolkvm_vm *vm,
		struct __smolkvm_mailbox_priv *mb, uint64_t command, uint64_t guest_ptr)
{
	struct smolkvm_mailbox_handler *h;

	switch (command) {
	case SMOLKVM_MAILBOX_CMD_NOP:
		return SMOLKVM_MAILBOX_STATUS_OK;
	case SMOLKVM_MAILBOX_CMD_DIE:
		printf("mailbox: guest asked us to die\n");
		exit(1);
	case SMOLKVM_MAILBOX_CMD_MAP_MEMORY: {
		struct smolkvm_mailbox_map_memory req;

		if (__smolkvm_memory_read(vm, guest_ptr, sizeof(req), &req))
			return SMOLKVM_MAILBOX_STATUS_ERR;

		__smolkvm_debug("mailbox: MAP_MEMORY gpa 0x%016llx size 0x%llx\n",
				(unsigned long long) req.gpa, (unsigned long long) req.size);

		if (req.size > SMOLKVM_MAILBOX_MAP_MEMORY_MAX) {
			printf("mailbox: MAP_MEMORY size 0x%llx over the limit\n",
			       (unsigned long long) req.size);
			return SMOLKVM_MAILBOX_STATUS_ERR;
		}

		return smolkvm_map_memory(vm, req.gpa, req.size)
				? SMOLKVM_MAILBOX_STATUS_ERR : SMOLKVM_MAILBOX_STATUS_OK;
	}
	default:
		h = __smolkvm_mailbox_find_handler(mb, command);
		if (h) {
			/* Copy the guest's command buffer in before calling the handler */
			if (h->buffer_size &&
			    __smolkvm_memory_read(vm, guest_ptr, h->buffer_size, h->buffer))
				return SMOLKVM_MAILBOX_STATUS_ERR;

			return h->fn(vm, command, h->buffer, h->priv);
		}

		printf("mailbox: unknown command %llu\n", (unsigned long long) command);
		return SMOLKVM_MAILBOX_STATUS_BADCMD;
	}
}

static void __smolkvm_mailbox_write(struct smolkvm_vm *vm,
		struct smolkvm_mmio *mmio, uint64_t offset, uint8_t len, uint64_t value)
{
	struct __smolkvm_mailbox_priv *mb = mmio->priv;

	if (offset == __SMOLKVM_MAILBOX_REG_SUBMIT) {
		uint64_t command   = SMOLKVM_MAILBOX_GET_CMD(value);
		uint64_t guest_ptr = SMOLKVM_MAILBOX_GET_PTR(value);
		uint64_t flags     = SMOLKVM_MAILBOX_GET_FLAGS(value);
		uint64_t ret;

		/* ASYNC and the other submit flags are decoded but not yet acted on */
		(void) flags;

		mb->inflight_command = command;
		mb->inflight_ptr = guest_ptr;

		ret = __smolkvm_mailbox_dispatch(vm, mb, command, guest_ptr);

		if (ret == SMOLKVM_MAILBOX_STATUS_PENDING) {
			/* Handler owns it now: show in-flight (not DONE) until
			 * smolkvm_mailbox_complete() is called. */
			mb->status = __smolkvm_mailbox_pack(0, command, guest_ptr);
		} else {
			uint64_t sf = SMOLKVM_MAILBOX_DONE;

			if (ret == SMOLKVM_MAILBOX_STATUS_OK)
				sf |= SMOLKVM_MAILBOX_SUCCESS;

			mb->status = __smolkvm_mailbox_pack(sf, command, guest_ptr);
		}
		return;
	}

	__smolkvm_debug("mailbox: write to unknown offset 0x%llx\n",
			(unsigned long long) offset);
}

static uint64_t __smolkvm_mailbox_read(struct smolkvm_vm *vm,
		struct smolkvm_mmio *mmio, uint64_t offset, uint8_t len)
{
	struct __smolkvm_mailbox_priv *mb = mmio->priv;

	if (offset == __SMOLKVM_MAILBOX_REG_STATUS)
		return mb->status;

	return 0;
}

static const struct smolkvm_mmio_reg __smolkvm_mailbox_regs[] = {
	{ .name = "SUBMIT", .offset = __SMOLKVM_MAILBOX_REG_SUBMIT, .size = 8 },
	{ .name = "STATUS", .offset = __SMOLKVM_MAILBOX_REG_STATUS, .size = 8 },
};

static struct smolkvm_mmio __smolkvm_mailbox = {
	.name = "mailbox",
	.phys = SMOLKVM_MAILBOX_PHYS,
	.len = SMOLKVM_MAILBOX_LEN,
	.regs = __smolkvm_mailbox_regs,
	.num_regs = SMOLKVM_ARRAYSIZE(__smolkvm_mailbox_regs),
	.write = __smolkvm_mailbox_write,
	.read = __smolkvm_mailbox_read,
	.priv = &__smolkvm_mailbox_priv,
};

/*
 * Register a host callback for a mailbox command. Call after smolkvm_create_vm.
 * Everything below SMOLKVM_MAILBOX_CMD_MINUSER is reserved for built-ins and
 * cannot be overridden (dispatch checks the built-ins first anyway).
 */
int smolkvm_mailbox_register(struct smolkvm_vm *vm, uint64_t command,
		void *buffer, size_t buffer_size,
		smolkvm_mailbox_handler_fn fn, void *priv)
{
	struct __smolkvm_mailbox_priv *mb = &__smolkvm_mailbox_priv;
	unsigned int i;

	(void) vm;

	if (buffer_size && !buffer) {
		printf("mailbox: command %llu has a buffer size but no buffer\n",
		       (unsigned long long) command);
		return -1;
	}

	if (command < SMOLKVM_MAILBOX_CMD_MINUSER) {
		printf("mailbox: command %llu is reserved for built-ins\n",
			   (unsigned long long) command);
		return -1;
	}

	if (__smolkvm_mailbox_find_handler(mb, command)) {
		printf("mailbox: command %llu already has a handler\n",
			   (unsigned long long) command);
		return -1;
	}

	for (i = 0; i < SMOLKVM_ARRAYSIZE(mb->handlers); i++) {
		if (!mb->handlers[i].used) {
			mb->handlers[i].used = true;
			mb->handlers[i].command = command;
			mb->handlers[i].buffer = buffer;
			mb->handlers[i].buffer_size = buffer_size;
			mb->handlers[i].fn = fn;
			mb->handlers[i].priv = priv;
			return 0;
		}
	}

	printf("mailbox: no free handler slots for command %llu\n",
		   (unsigned long long) command);
	return -1;
}

/*
 * Finish an asynchronous command. A handler that returned PENDING (or a
 * worker thread it spawned) calls this when the work is done; it marks the
 * in-flight command DONE (and SUCCESS if `success`) in the STATUS register
 * the guest polls.
 *
 * Single in-flight model: one outstanding command at a time, so the guest
 * must observe DONE before submitting again. The 64-bit STATUS store is
 * atomic on x86-64; completing from another thread still wants a proper
 * release/acquire pairing with the guest's poll for full correctness.
 */
void smolkvm_mailbox_complete(struct smolkvm_vm *vm, bool success)
{
	struct __smolkvm_mailbox_priv *mb = &__smolkvm_mailbox_priv;
	uint64_t sf = SMOLKVM_MAILBOX_DONE;

	(void) vm;

	if (success)
		sf |= SMOLKVM_MAILBOX_SUCCESS;

	mb->status = __smolkvm_mailbox_pack(sf, mb->inflight_command, mb->inflight_ptr);
}

static inline int __smolkvm_mailbox_create(struct smolkvm_vm *vm)
{
	__smolkvm_mailbox_priv.status = 0;
	__smolkvm_mailbox_priv.inflight_command = 0;
	__smolkvm_mailbox_priv.inflight_ptr = 0;
	memset(__smolkvm_mailbox_priv.handlers, 0,
		   sizeof(__smolkvm_mailbox_priv.handlers));

	__smolkvm_plugin_mmio(vm, &__smolkvm_mailbox);

	return 0;
}

#endif
/* -- */

/* Interrupt controller: a very basic 64-line IRQ status/mask/ack device */
#ifdef SMOLKVM_FOLD
#ifdef SMOLKVM_WANT_SIMPLE

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
#define SMOLKVM_IRQCHIP_PHYS	(SMOLKVM_MMIO_HOLE_PHYS + 0x2000)
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

static const struct smolkvm_mmio_reg __smolkvm_irqchip_regs[] = {
	{ .name = "STATUS", .offset = __SMOLKVM_IRQCHIP_REG_STATUS, .size = 8 },
	{ .name = "MASK",   .offset = __SMOLKVM_IRQCHIP_REG_MASK,   .size = 8 },
	{ .name = "ACK",    .offset = __SMOLKVM_IRQCHIP_REG_ACK,    .size = 8 },
};

static struct smolkvm_mmio __smolkvm_irqchip = {
	.name = "irqchip",
	.phys = SMOLKVM_IRQCHIP_PHYS,
	.len = SMOLKVM_IRQCHIP_LEN,
	.regs = __smolkvm_irqchip_regs,
	.num_regs = SMOLKVM_ARRAYSIZE(__smolkvm_irqchip_regs),
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

#endif /* SMOLKVM_WANT_SIMPLE */
#endif
/* -- */

/* Timer: free-running counter + programmable one-shot */
#ifdef SMOLKVM_FOLD
#ifdef SMOLKVM_WANT_SIMPLE

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
#define SMOLKVM_TIMER_PHYS		(SMOLKVM_MMIO_HOLE_PHYS + 0x3000)
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

static const struct smolkvm_mmio_reg __smolkvm_timer_regs[] = {
	{ .name = "FREQ",    .offset = __SMOLKVM_TIMER_REG_FREQ,    .size = 8 },
	{ .name = "COUNTER", .offset = __SMOLKVM_TIMER_REG_COUNTER, .size = 8 },
	{ .name = "ONESHOT", .offset = __SMOLKVM_TIMER_REG_ONESHOT, .size = 8 },
	{ .name = "STATUS",  .offset = __SMOLKVM_TIMER_REG_STATUS,  .size = 8 },
	{ .name = "IRQ",     .offset = __SMOLKVM_TIMER_REG_IRQ,     .size = 8 },
};

static struct smolkvm_mmio __smolkvm_timer = {
	.name = "timer",
	.phys = SMOLKVM_TIMER_PHYS,
	.len = SMOLKVM_TIMER_LEN,
	.regs = __smolkvm_timer_regs,
	.num_regs = SMOLKVM_ARRAYSIZE(__smolkvm_timer_regs),
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

#endif /* SMOLKVM_WANT_SIMPLE */
#endif
/* -- */

/* ELF loading */
#ifdef SMOLKVM_FOLD

int smolkvm_load_elf_with_displacement(struct smolkvm_vm *vm, const void *elf_image, int64_t displacement, uint64_t *entry)
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
		uint64_t dst = phdr->p_paddr + displacement;

		if (phdr->p_type != PT_LOAD)
			continue;

		if (__smolkvm_memory_write(vm, dst, phdr->p_filesz,
					   elf_image + phdr->p_offset)) {
			printf("failed to load segment %d "
			       "(0x%llx bytes at gpa 0x%llx) -- no RAM there?\n",
			       i, (unsigned long long) phdr->p_filesz,
			       (unsigned long long) dst);
			return -1;
		}

		/* Zero the tail of segments whose memory image is larger
		 * than their file image (e.g. .bss). */
		if (phdr->p_memsz > phdr->p_filesz &&
		    __smolkvm_memory_set(vm, dst + phdr->p_filesz,
					 phdr->p_memsz - phdr->p_filesz, 0)) {
			printf("failed to zero the tail of segment %d\n", i);
			return -1;
		}
	}

	if (entry)
		*entry = ehdr->e_entry + displacement;

	return 0;
}

int smolkvm_load_elf(struct smolkvm_vm *vm, const void *elf_image, uint64_t *entry)
{
	return smolkvm_load_elf_with_displacement(vm, elf_image, 0, entry);
}

int smolkvm_load_elf_file_with_displacement(struct smolkvm_vm *vm, const char *elf_path, int64_t displacement, uint64_t *entry)
{
	struct stat st;
	void *elf_image;
	ssize_t got;
	int ret;
	int elf;

	elf = open(elf_path, O_RDONLY);
	if (elf < 0) {
		printf("Failed to open ELF: %d\n", elf);
		return -1;
	}

	if (fstat(elf, &st) < 0 || st.st_size <= 0) {
		printf("Failed to stat ELF\n");
		close(elf);
		return -1;
	}

	elf_image = malloc((size_t) st.st_size);
	if (!elf_image) {
		printf("Failed to malloc() memory for IPL ELF image\n");
		close(elf);
		return -1;
	}

	got = read(elf, elf_image, (size_t) st.st_size);
	close(elf);

	if (got != st.st_size) {
		printf("Failed to read ELF image: %zd\n", got);
		free(elf_image);
		return -1;
	}

	printf("Read %zd bytes of ELF image\n", got);

	ret = smolkvm_load_elf_with_displacement(vm, elf_image, displacement, entry);
	free(elf_image);
	if (ret)
		return -1;

	return 0;
}

int smolkvm_load_elf_file(struct smolkvm_vm *vm, const char *elf_path, uint64_t *entry)
{
	return smolkvm_load_elf_file_with_displacement(vm, elf_path, 0, entry);
}
#endif
/* -- */

/* VM creation and teardown */
#ifdef SMOLKVM_FOLD

/*
 * Tell the guest what the CPU can do. KVM hands a vCPU a near-empty CPUID by
 * default; we pass through whatever the host KVM says it can support. This is
 * harmless for the bare-metal IPL (which never consults CPUID) but mandatory
 * for an OS like Linux that feature-probes heavily.
 */
#define SMOLKVM_CPUID_MAX_ENTRIES 256

static int __smolkvm_setup_cpuid(struct smolkvm_vm *vm)
{
	struct kvm_cpuid2 *cpuid;
	int ret;

	cpuid = calloc(1, sizeof(*cpuid) +
		       SMOLKVM_CPUID_MAX_ENTRIES * sizeof(struct kvm_cpuid_entry2));
	if (!cpuid)
		return -SMOLKVM_ERR_ALLOCMEMORY;

	cpuid->nent = SMOLKVM_CPUID_MAX_ENTRIES;

	ret = ioctl(vm->kvm_fd, KVM_GET_SUPPORTED_CPUID, cpuid);
	if (ret < 0) {
		__smolkvm_debug("KVM_GET_SUPPORTED_CPUID failed: %d\n", ret);
		free(cpuid);
		return -SMOLKVM_ERR_GET_CPUID;
	}

	ret = ioctl(vm->vcpu_fd, KVM_SET_CPUID2, cpuid);
	if (ret < 0) {
		__smolkvm_debug("KVM_SET_CPUID2 failed: %d\n", ret);
		free(cpuid);
		return -SMOLKVM_ERR_SET_CPUID;
	}

	free(cpuid);
	return 0;
}

#ifdef SMOLKVM_WANT_APIC
/*
 * Create the in-kernel interrupt controller (PIC + IOAPIC + per-vCPU LAPIC)
 * and the 8254 PIT that a stock OS expects. Must run after KVM_CREATE_VM but
 * before KVM_CREATE_VCPU, so the vCPU comes up with an in-kernel local APIC.
 * With this in place a guest HLT halts in-kernel and wakes on an interrupt
 * rather than exiting to us.
 */
static int __smolkvm_create_kernel_irqchip(int vm_fd)
{
	/*
	 * SPEAKER_DUMMY makes KVM emulate port 0x61 too; Linux's TSC
	 * calibration pokes the PIT channel 2 gate in there and gets confused
	 * by the 0xFF our unhandled-port fallback would return.
	 */
	struct kvm_pit_config pit = {
		.flags = KVM_PIT_SPEAKER_DUMMY,
	};
	int ret;

	ret = ioctl(vm_fd, KVM_CREATE_IRQCHIP, 0);
	if (ret < 0) {
		__smolkvm_debug("KVM_CREATE_IRQCHIP failed: %d\n", ret);
		return -SMOLKVM_ERR_CREATE_IRQCHIP;
	}

	ret = ioctl(vm_fd, KVM_CREATE_PIT2, &pit);
	if (ret < 0) {
		__smolkvm_debug("KVM_CREATE_PIT2 failed: %d\n", ret);
		return -SMOLKVM_ERR_CREATE_PIT;
	}

	return 0;
}
#endif

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

#ifdef SMOLKVM_WANT_APIC
	/* Must precede KVM_CREATE_VCPU so the vCPU gets an in-kernel LAPIC */
	ret = __smolkvm_create_kernel_irqchip(vm_fd);
	if (ret)
		return ret;
#endif

	vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
	if (vcpu_fd < 0)
		return -SMOLKVM_ERR_CREATEVCPU;

	kvm_run_sz= ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (kvm_run_sz <= 0)
		return -SMOLKVM_ERR_VCPUMMAPSIZE;

	vcpu_run = mmap(NULL, kvm_run_sz, PROT_READ | PROT_WRITE,
			MAP_SHARED, vcpu_fd, 0);
	if (vcpu_run == MAP_FAILED)
		return -SMOLKVM_ERR_VCPUMMAP;

	memory = mmap(NULL, SMOLKVM_BASEMEMORY_SZ, PROT_READ | PROT_WRITE,
		       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (memory == MAP_FAILED)
		return -SMOLKVM_ERR_ALLOCMEMORY;

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

	/* Low memory, so "BIOS area" pokes from a stock OS hit RAM */
	ret = smolkvm_map_memory(vm, SMOLKVM_LOWMEMORY_PHYS_START,
				 SMOLKVM_LOWMEMORY_SZ);
	if (ret)
		return -SMOLKVM_ERR_SETMEMORYREGION;

	/* Create the initial page tables */
	__smolkvm_create_initial_pagetables(vm);

	/* Advertise CPU features to the guest before the first run */
	ret = __smolkvm_setup_cpuid(vm);
	if (ret)
		return ret;

	/* Looks like KVM is ready to go. Setup the CPU */
	ret = __smolkvm_switch_cpu_into_longmode(vm);
	if (ret)
		return ret;

	ret = __smolkvm_set_rip(vm);
	if (ret)
		return ret;

//
	__smolkvm_sigio_kvm_run = vcpu_run;
	__smolkvm_setup_sighandler();
//

	/* Plug in the console .. boop beep boop */
	__smolkvm_console_create(vm);

	/* Plug in the mailbox so the guest can ask us to do things */
	__smolkvm_mailbox_create(vm);

#ifdef SMOLKVM_WANT_SIMPLE
	/* Plug in the interrupt controller */
	__smolkvm_irqchip_create(vm);

	/* Plug in the timer */
	__smolkvm_timer_create(vm);
#endif

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

/* KVM grows exit reasons; the table above is sparse and ends early */
static inline const char *__smolkvm_exit_reason_str(uint32_t exit_reason)
{
	if (exit_reason >= SMOLKVM_ARRAYSIZE(smolkvm_exit_reasons) ||
	    !smolkvm_exit_reasons[exit_reason])
		return "??";

	return smolkvm_exit_reasons[exit_reason];
}

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

	__smolkvm_debug("exit: %s\n", __smolkvm_exit_reason_str(exit_reason));
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
		break;
	case KVM_EXIT_IO:
		ret = __smolkvm_handle_io(vm);
		if (ret)
			return ret;
		break;
	case KVM_EXIT_DEBUG:
		/* GDB single-step / breakpoint: let the default loop handle it */
		break;
	default:
		/*
		 * Anything we don't explicitly recognise (internal errors, a
		 * fault that couldn't be delivered, stray IO, ...) stops the VM
		 * rather than silently re-entering KVM_RUN and livelocking.
		 */
		printf("unhandled exit reason %u, stopping the VM\n", exit_reason);
		return -SMOLKVM_ERR_UNHANDLED_EXIT;
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

static void __smolkvm_stop(struct smolkvm_vm *vm)
{
#ifdef SMOLKVM_WANT_GDB_STUB
	struct smolkvm_gdb_stub *gdb_stub = &vm->gdb_stub;

	gdb_stub->stopped = true;
	__smolkvm_gdb_stub_stop(vm);
#endif
}

static void __smolkvm_default_loop_pre_run(struct smolkvm_vm *vm)
{
	struct smolkvm_mmio **mmio;

	/*
	 * Clear before draining the devices: a SIGIO between here and KVM_RUN
	 * sets it again and the run returns immediately instead of losing the
	 * wakeup (see __smolkvm_signal_sigio()).
	 */
	vm->vcpu_run->immediate_exit = 0;

	__smolkvm_foreach_mmio(vm, mmio) {
		if ((*mmio)->pre_run)
			(*mmio)->pre_run(vm, *mmio);
	}

#ifdef SMOLKVM_WANT_GDB_STUB
	struct smolkvm_gdb_stub_pkt pkt;

	int pktret = __smolkvm_gdb_stub_read_packet(vm, &pkt);
	if (pktret == 1)
		__smolkvm_gdb_stub_process_packet(vm, &pkt);
	else if (pktret < 0) {
		/* The debugger went away: let the guest run free */
		printf("GDB disconnected, resuming\n");
		close(vm->gdb_stub.conn_socket);
		vm->gdb_stub.conn_socket = -1;
		vm->gdb_stub.stopped = false;
		vm->gdb_stub.single_stepping = false;
	}
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

static void __smolkvm_default_loop_post_run(struct smolkvm_vm *vm)
{
	struct smolkvm_mmio **mmio;

	__smolkvm_foreach_mmio(vm, mmio) {
		if ((*mmio)->post_run)
			(*mmio)->post_run(vm, *mmio);
	}

#ifdef SMOLKVM_WANT_GDB_STUB
	/*
	 * Only block for the debugger while the target is stopped; while it
	 * is running, packets are picked up opportunistically in pre_run on
	 * the next exit (and SIGIO forces one).
	 */
	if (vm->gdb_stub.stopped)
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
