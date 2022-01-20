#ifndef _KMEM32_ALLOCATION_H_
#define _KMEM32_ALLOCATION_H_

#include "util/h/kdrvtypes.h"

kptr_t allocate_from_kmem32(unsigned nbytes);
void free_from_kmem32(kptr_t addr, unsigned nbytes);

#endif
