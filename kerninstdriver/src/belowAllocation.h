#ifndef _BELOW_ALLOCATION_H_
#define _BELOW_ALLOCATION_H_

#include "util/h/kdrvtypes.h"

kptr_t allocate_from_below(unsigned nbytes, unsigned required_alignment);
void free_from_below(kptr_t addr, unsigned nbytes);
void destroy_below_allocator();

#endif
