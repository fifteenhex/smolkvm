#include <stdio.h>
#include <asm/bootparam.h>

#include "params.h"
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

static void fill_boot_params(void)
{

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

	printf("Asking for kernel load\n");

	mailbox_post(SMOLKVM_MAILBOX_CMD_LOADKERNEL, &loadkernel);

	printf("Jumping to entry @ %p\n", (void *) kernel_entry);

	jump_to_kernel(kernel_entry, &linux_boot_params);
}
