#ifndef _MAILBOX_H
#define _MAILBOX_H

#include "machine.h"

#define SMOLKVM_MAILBOX_CMD_SHIFT	48

static inline void mailbox_post(uint16_t cmd, void *buffer)
{
	volatile uint64_t *mb = (void *) SMOLKVM_MAILBOX_SUBMIT;

	*mb = ((uint64_t) cmd << SMOLKVM_MAILBOX_CMD_SHIFT) | ((uint64_t) buffer);
}

#define SMOLKVM_MAILBOX_CMD_DIE			1

#endif /* _MAILBOX_H */
