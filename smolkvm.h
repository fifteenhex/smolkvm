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
 */
#ifdef SMOLKVM_FOLD

#ifndef SMOLKVM_MEMREGIONS_NUM
#define SMOLKVM_MEMREGIONS_NUM		8
#endif

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

/* State structures */
#ifdef SMOLKVM_FOLD


struct smolkvm_vm {
	int vcpu_fd;
	int kvm_fd;
	int vm_fd;
	struct kvm_run *vcpu_run;
	size_t vcpu_run_sz;

	struct kvm_userspace_memory_region memregions[SMOLKVM_MEMREGIONS_NUM];
	/* Running count only; per-slot occupancy (memory_size != 0) is authoritative */
	unsigned int memory_region_plugged_in;


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


#endif /* _SMOLKVM_H */
