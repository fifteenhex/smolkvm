#ifndef _PARAMS_H
#define _PARAMS_H

#include <stdint.h>

struct cmd_buffer_getparams {
	uint64_t params_ptr;
	uint64_t size;
};

struct ipl_params {
    uint64_t ram_base;
    uint64_t ram_sz;
};

#endif /* _PARAMS_H */
