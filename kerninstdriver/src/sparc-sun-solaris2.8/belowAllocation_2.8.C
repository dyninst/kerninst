// nucleusTextAllocation_2.8.C

#include "kernInstIoctls.h"
#include "belowAllocation.h"
#include "util/h/kdrvtypes.h"

#include <sys/vmem.h>
#include <sys/kobj.h>
#include <sys/kmem_impl.h>
#include <vm/seg_kmem.h>

// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>


static vmem_t *below_nucleus_source;
static vmem_t *below_nucleus_arena = 0; // worth having a lock protecting it

static vmem_t *get_below_nucleus_arena()
{
    struct kernInstAddressMap address_map;
    kptr_t base;
    uint32_t size;

    if (below_nucleus_arena == 0) { // cache the result for subsequent calls
	getKernInstAddressMap(&address_map);
	base = address_map.startBelow;
	size = address_map.endBelow - address_map.startBelow;
	cmn_err(CE_CONT, "Creating below_nucleus_arenas(0x%lx, 0x%x)\n",
		base, size);
	// Create an arena that spans the 4MB range below nucleus
	// We do not use this arena for real allocations -- its
	// allocations are page aligned and not physically backed
	if ((below_nucleus_source = vmem_create("below_source", 
						(caddr_t)base, size, 
						PAGESIZE, NULL, NULL, NULL, 
						0, VM_NOSLEEP)) == NULL) {
	    cmn_err(CE_WARN, "vmem_create failed\n");
	    return 0;
	}
	// Create a sub-arena that allows fine-grained allocations
	// Physical memory is automatically reserved, thanks to segkmem_alloc
	if ((below_nucleus_arena = vmem_create("below_arena", 0, 0, 1, 
					      segkmem_alloc, segkmem_free,
					      below_nucleus_source,
					      0, VM_NOSLEEP)) == NULL) {
	    vmem_destroy(below_nucleus_source);
	    cmn_err(CE_WARN, "vmem_create failed\n");
	    return 0;
	}
    }
    return below_nucleus_arena;
}

// Clean up when the client exits
void destroy_below_allocator()
{
    if (below_nucleus_arena != 0) {
	cmn_err(CE_CONT, "About to vmem_destroy below_nucleus_arenas\n");
	vmem_destroy(below_nucleus_arena);
	below_nucleus_arena = 0;
	vmem_destroy(below_nucleus_source);
    }
}

// Allocate nbytes from below the nucleus or return 0 on failure
kptr_t allocate_from_below(unsigned nbytes, unsigned desiredAlignment)
{
    vmem_t *bna = get_below_nucleus_arena();
    kptr_t rv;

    if (bna == 0) {
	return 0;
    }
    // Allocate virtual and physical memory in one shot
    if ((rv = (kptr_t)vmem_xalloc(bna, nbytes, desiredAlignment,
				  0, 0, NULL, NULL, VM_NOSLEEP)) == 0) {
	return 0;
    }
    if (rv % desiredAlignment != 0) {
	// Should not happen, but let's check anyway
	cmn_err(CE_PANIC, "allocate_from_below: did not "
		"satisfy alignment requirements");
	return 0;
    }
    return rv;
}

// De-allocate the previously allocated region
void free_from_below(kptr_t addr, unsigned nbytes) 
{
    vmem_t *bna = get_below_nucleus_arena();

    if (bna == 0) {
	// Should not have happened
	cmn_err(CE_PANIC, "free_from_below: internal error\n");
	return;
    }
    cmn_err(CE_CONT, "free_from_below(0x%lx, %u)\n", addr, nbytes);
    vmem_xfree(bna, (void *)addr, nbytes);
}



