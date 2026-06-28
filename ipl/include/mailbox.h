#ifndef _MAILBOX_H
#define _MAILBOX_H

#define SMOLKVM_MAILBOX_PHYS		0x2000
#define __SMOLKVM_MAILBOX_REG_SUBMIT	0x00
#define __SMOLKVM_MAILBOX_REG_STATUS	0x08

#define SMOLKVM_MAILBOX_CMD_SHIFT	48
#define SMOLKVM_MAILBOX_CMD_MINUSER	16
#define SMOLKVM_MAILBOX_CMD_GETPARAMS (SMOLKVM_MAILBOX_CMD_MINUSER + 0)

static inline void mailbox_post(uint16_t cmd, void *buffer)
{
	volatile uint64_t *mb = (void *) SMOLKVM_MAILBOX_PHYS;

	*mb = ((uint64_t) cmd << SMOLKVM_MAILBOX_CMD_SHIFT) | ((uint64_t) buffer);
}

#define SMOLKVM_MAILBOX_CMD_MAP_MEMORY		3

struct smolkvm_mailbox_map_memory {
	uint64_t gpa;
	uint64_t size;
};

#endif /* _MAILBOX_H */
