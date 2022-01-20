// nucleusTextAllocation_2.8.C

#include "kernInstIoctls.h"
#include "nucleusTextAllocation.h"
#include "util/h/kdrvtypes.h"

#include <sys/vmem.h>
#include <sys/kobj.h>

// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>

// Find the text_arena symbol and return its contents
vmem_t *get_text_arena()
{
    static vmem_t** rv = 0; // Cache the address of the symbol

    if (rv == 0) {
	rv = (vmem_t **)kobj_getsymvalue((char *)"text_arena", 0);
	if (rv == 0) {
	    cmn_err(CE_WARN, 
		    "get_text_arena: could not find the text_arena symbol\n");
	    return 0;
	}
    }
    if (*rv == 0) {
	cmn_err(CE_WARN, "get_text_arena: illegal value of text_arena\n");
	return 0;
    }
    return *rv;
}

// Allocate nbytes within the nucleus or return 0 on failure
kptr_t try_allocate_within_nucleus_text(unsigned nbytes,
					unsigned desiredAlignment,
					kptr_t &mapped_addr)
{
    struct kernInstAddressMap result_info;
    vmem_t *the_text_arena;
    kptr_t rv;

    mapped_addr = 0;
    // Get the contents of the text_arena symbol
    if ((the_text_arena = get_text_arena()) == 0) {
	return 0;
    }
    // Acquire desired bounds of allocation
    getKernInstAddressMap(&result_info);
    
    if ((rv = (kptr_t)vmem_xalloc(the_text_arena, nbytes, desiredAlignment,
				  0, 0, (void*)result_info.startNucleus,
				  (void*)result_info.endNucleus, 
				  VM_NOSLEEP)) == 0) {
	return 0;
    }
    if (rv % desiredAlignment != 0) {
	// Should not happen, but let's check anyway
	cmn_err(CE_PANIC, "try_allocate_within_nucleus_text: Failed to "
		"satisfy alignment requirements");
	return 0;
    }
	
    mapped_addr = rv;
    return rv;
}

// De-allocate the previously allocated nucleus region
void free_from_nucleus_text(kptr_t addr, kptr_t mapped_addr, 
			    unsigned nbytes) 
{
    vmem_t *the_text_arena = get_text_arena();
    if (the_text_arena != 0) {
	cmn_err(CE_CONT, "free_from_nucleus_text(0x%lx, 0x%lx, %u)\n",
		addr, mapped_addr, nbytes);
	vmem_xfree(the_text_arena, (void *)addr, nbytes);
    }
}



