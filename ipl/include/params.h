#ifndef _PARAMS_H
#define _PARAMS_H

#include <stdint.h>

struct cmd_buffer_getparams {
	uint64_t params_ptr;
	uint64_t size;
};

/*
 * What the host tells the IPL about the machine. The initrd (if any) is
 * loaded by the host into system RAM at initrd_base as part of LOADKERNEL.
 */
struct ipl_params {
	uint64_t ram_base;
	uint64_t ram_sz;
	uint64_t initrd_base;
	uint64_t initrd_sz;
	char cmdline[256];
};

#endif /* _PARAMS_H */
