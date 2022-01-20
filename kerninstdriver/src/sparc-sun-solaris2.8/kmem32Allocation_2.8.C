#include "kernInstIoctls.h"
#include "kmem32Allocation.h"

#include <sys/vmem.h>
#include <vm/seg_kmem.h>
// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>

extern vmem_t *heap32_arena;

// Allocate nbytes of virtual + physical memory in the 32-bit address range
kptr_t allocate_from_kmem32(unsigned nbytes)
{
    kptr_t rv;

    // cmn_err(CE_CONT, "About to allocate %d bytes\n", nbytes);
    if ((rv = (kptr_t)segkmem_alloc(heap32_arena, nbytes, VM_NOSLEEP)) == 0) {
	cmn_err(CE_CONT, "allocate_from_kmem32: alloc failed");
	return 0;
    }
    // cmn_err(CE_CONT, "Allocated %d bytes at %lx\n", nbytes, rv);

    return rv;
}

// Free nbytes at addr in the 32-bit address range
void free_from_kmem32(kptr_t addr, unsigned nbytes)
{
    // cmn_err(CE_CONT, "About to free %d bytes at %lx\n", nbytes, addr);
    segkmem_free(heap32_arena, (void *)addr, nbytes);
    // cmn_err(CE_CONT, "Freed %d bytes at %lx\n", nbytes, addr);
}
