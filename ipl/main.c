#include <stdio.h>
#include <asm/bootparam.h>

#include "params.h"
#include "machine.h"
#include "mailbox.h"
#include "loadkernel.h"

static struct ipl_params ipl_params = { 0 };

/* --- boot ---------------------------------------------------------------- */

static struct boot_params linux_boot_params = { 0 };

#define PTE_PRESENT	(1ull << 0)
#define PTE_RW		(1ull << 1)
#define PTE_PS		(1ull << 7)	/* 2MB huge page */
#define HUGE_PAGE_SZ	0x200000ull

static void map_system_ram(uint64_t base, uint64_t size)
{
	uint64_t *pd = (uint64_t *) SMOLKVM_PAGETABLE_PD;
	uint64_t gpa;

	/*
	 * One page directory of 2MB pages covers exactly 1GB, and that is all
	 * the PDPT points down here; going further would scribble over the
	 * neighbouring page table pages.
	 */
	if (base + size > 512 * HUGE_PAGE_SZ) {
		printf("RAM beyond 1GB is not mappable, capping\n");
		size = 512 * HUGE_PAGE_SZ - base;
	}

	for (gpa = base; gpa < base + size; gpa += HUGE_PAGE_SZ)
		pd[gpa / HUGE_PAGE_SZ] = gpa | PTE_PS | PTE_RW | PTE_PRESENT;

	__asm__ __volatile__(
		"mov %%cr3, %%rax\n\t"
		"mov %%rax, %%cr3"
		::: "rax", "memory");
}

#ifndef E820_TYPE_RAM
#define E820_TYPE_RAM		1
#endif
#ifndef E820_TYPE_RESERVED
#define E820_TYPE_RESERVED	2
#endif

static void e820_add(struct boot_params *bp, uint64_t addr, uint64_t size, uint32_t type)
{
	struct boot_e820_entry *e = &bp->e820_table[bp->e820_entries++];

	e->addr = addr;
	e->size = size;
	e->type = type;
}

static void fill_boot_params(void)
{
	struct boot_params *bp = &linux_boot_params;

	/*
	 * The classic usable low memory range (up to the traditional EBDA
	 * spot). The host backs all of low memory with RAM; without some
	 * sub-1MB usable memory Linux cannot place its real-mode trampoline.
	 */
	e820_add(bp, 0, 0x9FC00, E820_TYPE_RAM);

	/* Off-limits to the kernel: the MMIO devices and the IPL's own region
	 * (which also holds these boot_params and the cmdline, so it must
	 * survive until the kernel has consumed them) */
	e820_add(bp, SMOLKVM_CONSOLE_BASE,    SMOLKVM_CONSOLE_LEN,    E820_TYPE_RESERVED);
	e820_add(bp, SMOLKVM_MAILBOX_BASE,    SMOLKVM_MAILBOX_LEN,    E820_TYPE_RESERVED);
	e820_add(bp, SMOLKVM_BASEMEMORY_BASE, SMOLKVM_BASEMEMORY_LEN, E820_TYPE_RESERVED);

	/* The system RAM we mapped is usable */
	e820_add(bp, ipl_params.ram_base, ipl_params.ram_sz, E820_TYPE_RAM);

	bp->hdr.type_of_loader = 0xFF;
	bp->hdr.boot_flag      = 0xAA55;
	bp->hdr.header         = 0x53726448;	/* "HdrS" */
	bp->hdr.version        = 0x020d;
	bp->hdr.cmd_line_ptr   = (uint32_t) (uintptr_t) ipl_params.cmdline;

	if (ipl_params.initrd_sz) {
		bp->hdr.ramdisk_image = (uint32_t) ipl_params.initrd_base;
		bp->hdr.ramdisk_size  = (uint32_t) ipl_params.initrd_sz;
	}
}

__attribute__((noreturn))
static void jump_to_kernel(uint64_t entry, struct boot_params *params)
{
	__asm__ __volatile__(
		"cli\n\t"
		"jmp *%[entry]\n\t"
		:
		: [entry] "r"(entry), "S"(params)
		: "memory");

	__builtin_unreachable();
}

void _c_start(void)
{
	struct smolkvm_mailbox_map_memory sysramcmd = { 0 };
	uint64_t kernel_entry = 0;
	struct cmd_buffer_getparams getparams = {
		.params_ptr = (uint64_t) &ipl_params,
		.size = sizeof(ipl_params),
	};
	struct cmd_buf_loadkernel loadkernel = {
		.entry_ptr = (uint64_t) &kernel_entry,
	};

	printf("smolkvm test vm IPL\n");

	mailbox_post(SMOLKVM_MAILBOX_CMD_GETPARAMS, &getparams);
	printf("RAM 0x%lx @ 0x%lx\n", ipl_params.ram_sz, ipl_params.ram_base);
	printf("cmdline: %s\n", ipl_params.cmdline);

	printf("Asking for system RAM\n");
	sysramcmd.gpa = ipl_params.ram_base;
	sysramcmd.size = ipl_params.ram_sz;
	mailbox_post(SMOLKVM_MAILBOX_CMD_MAP_MEMORY, &sysramcmd);

	printf("Mapping system RAM into page tables\n");
	map_system_ram(ipl_params.ram_base, ipl_params.ram_sz);

	printf("Asking for kernel load\n");

	mailbox_post(SMOLKVM_MAILBOX_CMD_LOADKERNEL, &loadkernel);
	if (!kernel_entry) {
		printf("Kernel load failed, giving up\n");
		mailbox_post(SMOLKVM_MAILBOX_CMD_DIE, 0);
	}

	printf("Filling boot params\n");
	fill_boot_params();

	printf("Jumping to entry @ %p\n", (void *) kernel_entry);

	jump_to_kernel(kernel_entry, &linux_boot_params);
}
