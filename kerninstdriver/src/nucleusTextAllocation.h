// nucleusTextAllocation.h

#ifndef _NUCLEUS_TEXT_ALLOCATION_H_
#define _NUCLEUS_TEXT_ALLOCATION_H_

#include <sys/types.h>
#include "util/h/kdrvtypes.h"

kptr_t try_allocate_within_nucleus_text(unsigned nbytes,
					unsigned desiredAlignment,
					kptr_t &result_mappable);
   // returns 0 on failure

void free_from_nucleus_text(kptr_t addr, kptr_t addr_mappable, 
			    unsigned nbytes);

#endif
