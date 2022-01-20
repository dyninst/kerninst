#include "kernInstIoctls.h"
#include "belowAllocation.h"
#include "util/h/kdrvtypes.h"

#include <sys/kobj.h>
#include <vm/seg_kmem.h>

// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>

static int initialized = 0;
static struct seg *seg_below;
static struct map *map_below;
static kptr_t base;

// Initialize the allocator. Performed on a first call to the allocation
// routine below
static int create_below_allocator()
{
    struct kernInstAddressMap bounds;
    unsigned nbytes;
    pgcnt_t npages;
    
    if (!initialized) {
	getKernInstAddressMap(&bounds);
	base = bounds.startBelow;
	nbytes = bounds.endBelow - bounds.startBelow;
	npages = btopr(nbytes);

	rw_enter(&kas.a_lock, RW_WRITER);
	if ((seg_below = seg_alloc(&kas, (caddr_t)base, nbytes)) == NULL ||
	    segkmem_create(seg_below, NULL) < 0) {
	    cmn_err(CE_WARN, "seg_alloc or _create failed\n");
	    rw_exit(&kas.a_lock);
	    return (-1);
	}

	if ((map_below = rmallocmap(npages)) == NULL) {
	    cmn_err(CE_WARN, "test_below_nucleus: rmallocmap failed\n");
	    seg_free(seg_below);
	    rw_exit(&kas.a_lock);

	    return (-1);
	}
	rw_exit(&kas.a_lock);

	rmfree(map_below, npages, 1);
	
	initialized = 1;
    }
    return 0;
}

// Allocate nbytes with required_alignment from below the nucleus
kptr_t allocate_from_below(unsigned nbytes, unsigned required_alignment)
{
    unsigned npages = btopr(nbytes);
    kptr_t addr, ndx;

    //cmn_err(CE_CONT, "Entered allocate_from_below()\n");
    
    if (create_below_allocator() < 0) {
	return 0;
    }
    // Allocate a region of the virtual address space
    if ((ndx = rmalloc(map_below, npages)) == 0) {
	cmn_err(CE_WARN, "rmalloc failed\n");
	return 0;
    }
    addr = base + ((ndx - 1) << PAGESHIFT);
    
    // Get the physical backing for this address
    if (segkmem_alloc(seg_below, (caddr_t)addr, npages * PAGESIZE, 0) == 0) {
	cmn_err(CE_WARN, "segkmem_alloc failed\n");
	return 0;
    }

    //cmn_err(CE_CONT, "Returning from allocate_from_below()\n");

    return addr;
}

// Free the given address
void free_from_below(kptr_t addr, unsigned nbytes)
{
    kptr_t ndx = ((addr - base) >> PAGESHIFT) + 1;
    unsigned npages = btopr(nbytes);

    //cmn_err(CE_CONT, "Entered free_from_below()\n");

    if (create_below_allocator() < 0) {
	return;
    }
    // Free the physical memory
    segkmem_free(seg_below, (caddr_t)addr, npages * PAGESIZE); 

    // Free the virtual memory
    rmfree(map_below, npages, ndx);

    //cmn_err(CE_CONT, "Returning from free_from_below()\n");
}

// Clean-up when the client exits
void destroy_below_allocator()
{
    if (initialized) {
	rmfreemap(map_below);
	
	rw_enter(&kas.a_lock, RW_WRITER);
	seg_free(seg_below);
	rw_exit(&kas.a_lock);

	initialized = 0;
    }
}
