#ifndef _MAILBOX_H
#define _MAILBOX_H

#include "machine.h"

#define SMOLKVM_MAILBOX_CMD_SHIFT	48
#define SMOLKVM_MAILBOX_CMD_MINUSER	16
#define SMOLKVM_MAILBOX_CMD_GETPARAMS (SMOLKVM_MAILBOX_CMD_MINUSER + 0)
#define SMOLKVM_MAILBOX_CMD_LOADKERNEL (SMOLKVM_MAILBOX_CMD_MINUSER + 1)

static inline void mailbox_post(uint16_t cmd, void *buffer)
{
	volatile uint64_t *mb = (void *) SMOLKVM_MAILBOX_SUBMIT;

	*mb = ((uint64_t) cmd << SMOLKVM_MAILBOX_CMD_SHIFT) | ((uint64_t) buffer);
}

#define SMOLKVM_MAILBOX_CMD_DIE			1
#define SMOLKVM_MAILBOX_CMD_MAP_MEMORY		3

struct smolkvm_mailbox_map_memory {
	uint64_t gpa;
	uint64_t size;
};

#endif /* _MAILBOX_H */
