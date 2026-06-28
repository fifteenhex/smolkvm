#include <stdio.h>

#include "params.h"
#include "mailbox.h"

static struct ipl_params ipl_params = { 0 };

void _c_start(void)
{
	struct smolkvm_mailbox_map_memory sysramcmd = {
		.gpa = 0x400000,
		.size = 0x4000000,
	};

	printf("smolkvm test vm IPL\n");

	mailbox_post(SMOLKVM_MAILBOX_CMD_GETPARAMS, &ipl_params);

	printf("Asking for system RAM\n");

	mailbox_post(SMOLKVM_MAILBOX_CMD_MAP_MEMORY, &sysramcmd);

	while(1) { };
}
