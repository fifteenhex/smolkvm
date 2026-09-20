#include <stdio.h>
#include <stdint.h>

#include "machine.h"
#include "mailbox.h"

void _c_start(void)
{
	printf("smolkvm test vm IPL\n");

	/* Nothing here boots anything yet: hand the machine back */
	mailbox_post(SMOLKVM_MAILBOX_CMD_DIE, 0);
}
