#include <stdio.h>
#include <asm/bootparam.h>

#include "params.h"
#include "machine.h"
#include "mailbox.h"
#include "loadkernel.h"

static struct ipl_params ipl_params = { 0 };

/* See: https://wiki.osdev.org/RSDP */
struct XSDP {
	char signature[8];
	uint8_t checksum;
	char oemid[6];
	uint8_t revision;
	uint32_t rsdtaddress;

	uint32_t length;
	uint64_t xsdtAddress;
	uint8_t extendedchecksum;
	uint8_t reserved[3];
} __attribute__ ((packed));

static struct XSDP rsdp = {
};

static struct boot_params linux_boot_params = { 0 };

/* TODO: set console=/earlyprintk= to match your custom tty to see boot output */
static char cmdline[] = "";

#define PTE_PRESENT	(1ull << 0)
#define PTE_RW		(1ull << 1)
#define PTE_PS		(1ull << 7)	/* 2MB huge page */
#define HUGE_PAGE_SZ	0x200000ull

static void map_system_ram(uint64_t base, uint64_t size)
{
	uint64_t *pd = (uint64_t *) SMOLKVM_PAGETABLE_PD;
	uint64_t gpa;

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

	/* Off-limits to the kernel: the MMIO devices and the IPL's own region */
	e820_add(bp, SMOLKVM_CONSOLE_BASE,    SMOLKVM_CONSOLE_LEN,    E820_TYPE_RESERVED);
	e820_add(bp, SMOLKVM_MAILBOX_BASE,    SMOLKVM_MAILBOX_LEN,    E820_TYPE_RESERVED);
	e820_add(bp, SMOLKVM_BASEMEMORY_BASE, SMOLKVM_BASEMEMORY_LEN, E820_TYPE_RESERVED);

	/* The system RAM we mapped is usable */
	e820_add(bp, ipl_params.ram_base, ipl_params.ram_sz, E820_TYPE_RAM);

	bp->hdr.type_of_loader = 0xFF;
	bp->hdr.boot_flag      = 0xAA55;
	bp->hdr.header         = 0x53726448;	/* "HdrS" */
	bp->hdr.version        = 0x020d;
	bp->hdr.cmd_line_ptr   = (uint32_t) (uintptr_t) cmdline;
	bp->hdr.cmdline_size   = sizeof(cmdline);
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
	uint64_t kernel_entry;
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

	printf("Asking for system RAM\n");
	sysramcmd.gpa = ipl_params.ram_base;
	sysramcmd.size = ipl_params.ram_sz;
	mailbox_post(SMOLKVM_MAILBOX_CMD_MAP_MEMORY, &sysramcmd);

	printf("Mapping system RAM into page tables\n");
	map_system_ram(ipl_params.ram_base, ipl_params.ram_sz);

	printf("Asking for kernel load\n");

	mailbox_post(SMOLKVM_MAILBOX_CMD_LOADKERNEL, &loadkernel);

	printf("Filling boot params\n");
	fill_boot_params();

	printf("Jumping to entry @ %p\n", (void *) kernel_entry);

	jump_to_kernel(kernel_entry, &linux_boot_params);
}
