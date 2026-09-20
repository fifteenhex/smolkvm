#include <stdio.h>

#include "params.h"
#include "machine.h"
#include "mailbox.h"

static struct ipl_params ipl_params = { 0 };

/* --- boot ---------------------------------------------------------------- */

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

void _c_start(void)
{
	struct smolkvm_mailbox_map_memory sysramcmd = { 0 };
	struct cmd_buffer_getparams getparams = {
		.params_ptr = (uint64_t) &ipl_params,
		.size = sizeof(ipl_params),
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

	/* Nothing to boot yet: hand the machine back */
	mailbox_post(SMOLKVM_MAILBOX_CMD_DIE, 0);
}
