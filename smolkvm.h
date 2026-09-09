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


#endif /* _SMOLKVM_H */
