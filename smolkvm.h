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


#endif /* _SMOLKVM_H */
