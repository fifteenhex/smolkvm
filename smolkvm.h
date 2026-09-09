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
#define SMOLKVM_ERR_GET_CPUID       114
#define SMOLKVM_ERR_SET_CPUID       115

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


#endif /* _SMOLKVM_H */
