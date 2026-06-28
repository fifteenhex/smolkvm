#include <stdio.h>
#include <asm/bootparam.h>

#include "params.h"
#include "machine.h"
#include "mailbox.h"
#include "loadkernel.h"

static struct ipl_params ipl_params = { 0 };

/* --- ACPI tables --------------------------------------------------------- */

/* Common header on every ACPI system description table */
struct acpi_sdt_header {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __attribute__ ((packed));

/* Generic Address Structure (used by the FADT) */
struct acpi_gas {
	uint8_t space_id;
	uint8_t bit_width;
	uint8_t bit_offset;
	uint8_t access_size;
	uint64_t address;
} __attribute__ ((packed));

/* MADT entry types we emit */
struct acpi_madt_local_apic {
	uint8_t type;			/* 0 */
	uint8_t length;			/* 8 */
	uint8_t acpi_processor_id;
	uint8_t apic_id;
	uint32_t flags;			/* bit 0 = enabled */
} __attribute__ ((packed));

struct acpi_madt_io_apic {
	uint8_t type;			/* 1 */
	uint8_t length;			/* 12 */
	uint8_t io_apic_id;
	uint8_t reserved;
	uint32_t io_apic_address;
	uint32_t gsi_base;
} __attribute__ ((packed));

struct acpi_madt_interrupt_override {
	uint8_t type;			/* 2 */
	uint8_t length;			/* 10 */
	uint8_t bus;			/* 0 = ISA */
	uint8_t source;			/* source IRQ */
	uint32_t gsi;
	uint16_t flags;			/* 0 = bus default polarity/trigger */
} __attribute__ ((packed));

struct ipl_madt {
	struct acpi_sdt_header header;
	uint32_t local_apic_address;
	uint32_t flags;			/* bit 0 = PCAT_COMPAT (8259 present) */
	struct acpi_madt_local_apic lapic;
	struct acpi_madt_io_apic ioapic;
	struct acpi_madt_interrupt_override pit_override;
} __attribute__ ((packed));

struct ipl_dsdt {
	struct acpi_sdt_header header;
	uint8_t aml[7];			/* Scope(\_SB){} */
} __attribute__ ((packed));

#define ACPI_FADT_HW_REDUCED	(1u << 20)	/* no FACS / PM register blocks */
#define ACPI_IAPC_NO_VGA	(1u << 2)
#define ACPI_IAPC_NO_MSI	(1u << 3)

struct ipl_fadt {
	struct acpi_sdt_header header;
	uint32_t firmware_ctrl;
	uint32_t dsdt;
	uint8_t reserved0;
	uint8_t preferred_pm_profile;
	uint16_t sci_int;
	uint32_t smi_cmd;
	uint8_t acpi_enable;
	uint8_t acpi_disable;
	uint8_t s4bios_req;
	uint8_t pstate_cnt;
	uint32_t pm1a_evt_blk, pm1b_evt_blk, pm1a_cnt_blk, pm1b_cnt_blk;
	uint32_t pm2_cnt_blk, pm_tmr_blk, gpe0_blk, gpe1_blk;
	uint8_t pm1_evt_len, pm1_cnt_len, pm2_cnt_len, pm_tmr_len;
	uint8_t gpe0_blk_len, gpe1_blk_len, gpe1_base, cst_cnt;
	uint16_t p_lvl2_lat, p_lvl3_lat, flush_size, flush_stride;
	uint8_t duty_offset, duty_width, day_alrm, mon_alrm, century;
	uint16_t iapc_boot_arch;
	uint8_t reserved1;
	uint32_t flags;
	struct acpi_gas reset_reg;
	uint8_t reset_value;
	uint16_t arm_boot_arch;
	uint8_t minor_version;
	uint64_t x_firmware_ctrl;
	uint64_t x_dsdt;
	struct acpi_gas x_pm1a_evt_blk, x_pm1b_evt_blk, x_pm1a_cnt_blk, x_pm1b_cnt_blk;
	struct acpi_gas x_pm2_cnt_blk, x_pm_tmr_blk, x_gpe0_blk, x_gpe1_blk;
	struct acpi_gas sleep_control_reg, sleep_status_reg;
	uint64_t hypervisor_vendor_id;
} __attribute__ ((packed));

struct ipl_xsdt {
	struct acpi_sdt_header header;
	uint64_t entries[2];		/* FADT, MADT */
} __attribute__ ((packed));

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

static struct ipl_madt madt = {
	.header = {
		.signature = "APIC", .length = sizeof(struct ipl_madt), .revision = 4,
		.oem_id = "SMOLKV", .oem_table_id = "SMOLMADT",
		.oem_revision = 1, .creator_revision = 1,
	},
	.local_apic_address = (uint32_t) SMOLKVM_APIC_LAPIC_BASE,
	.flags = 1,	/* PCAT_COMPAT: KVM's 8259 PICs are present */
	.lapic = { .type = 0, .length = 8, .acpi_processor_id = 0, .apic_id = 0, .flags = 1 },
	.ioapic = {
		.type = 1, .length = 12, .io_apic_id = 0,
		.io_apic_address = (uint32_t) SMOLKVM_APIC_IOAPIC_BASE,
		.gsi_base = SMOLKVM_APIC_IOAPIC_GSI_BASE,
	},
	/* Standard PC wiring: the PIT (ISA IRQ0) lands on IOAPIC pin 2 */
	.pit_override = { .type = 2, .length = 10, .bus = 0, .source = 0, .gsi = 2, .flags = 0 },
};

static struct ipl_dsdt dsdt = {
	.header = {
		.signature = "DSDT", .length = sizeof(struct ipl_dsdt), .revision = 2,
		.oem_id = "SMOLKV", .oem_table_id = "SMOLDSDT",
		.oem_revision = 1, .creator_revision = 1,
	},
	.aml = { 0x10, 0x06, 0x5C, 0x5F, 0x53, 0x42, 0x5F },	/* Scope(\_SB){} */
};

static struct ipl_fadt fadt = {
	.header = {
		.signature = "FACP", .length = sizeof(struct ipl_fadt), .revision = 6,
		.oem_id = "SMOLKV", .oem_table_id = "SMOLFADT",
		.oem_revision = 1, .creator_revision = 1,
	},
	.flags = ACPI_FADT_HW_REDUCED,
	.iapc_boot_arch = ACPI_IAPC_NO_VGA | ACPI_IAPC_NO_MSI,
	.minor_version = 0,
	/* dsdt / x_dsdt are filled at build time */
};

static struct ipl_xsdt xsdt = {
	.header = {
		.signature = "XSDT", .length = sizeof(struct ipl_xsdt), .revision = 1,
		.oem_id = "SMOLKV", .oem_table_id = "SMOLXSDT",
		.oem_revision = 1, .creator_revision = 1,
	},
	/* entries are filled at build time */
};

static struct XSDP rsdp = {
	.signature = "RSD PTR ",
	.oemid = "SMOLKV",
	.revision = 2,
	.rsdtaddress = 0,		/* we provide an XSDT, not an RSDT */
	.length = sizeof(struct XSDP),
	/* xsdtAddress and the checksums are filled at build time */
};

static uint8_t acpi_csum(const void *buf, uint32_t len)
{
	const uint8_t *p = buf;
	uint8_t sum = 0;
	uint32_t i;

	for (i = 0; i < len; i++)
		sum += p[i];

	return sum;
}

static void acpi_set_checksum(struct acpi_sdt_header *h)
{
	h->checksum = 0;
	h->checksum = (uint8_t) (0 - acpi_csum(h, h->length));
}

static uint64_t build_madt(void)
{
	acpi_set_checksum(&madt.header);
	return (uint64_t) (uintptr_t) &madt;
}

static uint64_t build_dsdt(void)
{
	acpi_set_checksum(&dsdt.header);
	return (uint64_t) (uintptr_t) &dsdt;
}

static uint64_t build_fadt(uint64_t dsdt_addr)
{
	fadt.dsdt = (uint32_t) dsdt_addr;
	fadt.x_dsdt = dsdt_addr;
	acpi_set_checksum(&fadt.header);
	return (uint64_t) (uintptr_t) &fadt;
}

static uint64_t build_xsdt(uint64_t fadt_addr, uint64_t madt_addr)
{
	xsdt.entries[0] = fadt_addr;
	xsdt.entries[1] = madt_addr;
	acpi_set_checksum(&xsdt.header);
	return (uint64_t) (uintptr_t) &xsdt;
}

static uint64_t build_rsdp(uint64_t xsdt_addr)
{
	rsdp.xsdtAddress = xsdt_addr;

	rsdp.checksum = 0;
	rsdp.extendedchecksum = 0;
	rsdp.checksum = (uint8_t) (0 - acpi_csum(&rsdp, 20));		/* ACPI 1.0 part */
	rsdp.extendedchecksum = (uint8_t) (0 - acpi_csum(&rsdp, sizeof(rsdp)));

	return (uint64_t) (uintptr_t) &rsdp;
}

/* --- boot ---------------------------------------------------------------- */

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

static void build_acpi(void)
{
	uint64_t madt_addr = build_madt();
	uint64_t dsdt_addr = build_dsdt();
	uint64_t fadt_addr = build_fadt(dsdt_addr);
	uint64_t xsdt_addr = build_xsdt(fadt_addr, madt_addr);

	linux_boot_params.acpi_rsdp_addr = build_rsdp(xsdt_addr);
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

	printf("Building ACPI tables\n");
	build_acpi();

	printf("Filling boot params\n");
	fill_boot_params();

	printf("Jumping to entry @ %p\n", (void *) kernel_entry);

	jump_to_kernel(kernel_entry, &linux_boot_params);
}
