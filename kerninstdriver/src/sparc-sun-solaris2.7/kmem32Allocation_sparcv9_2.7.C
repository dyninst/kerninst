#include "kernInstIoctls.h"
#include "kmem32Allocation.h"

#include <sys/kmem.h>
// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>

extern "C" void* kmem_getpages32(pgcnt_t npages, u_int flag);
extern "C" void  kmem_freepages32(void *addr, pgcnt_t npages);

kptr_t allocate_from_kmem32(unsigned nbytes)
{
    pgcnt_t npages = mmu_btopr(nbytes);
    void * rv;

    cmn_err(CE_CONT, "About to allocate %d bytes\n", nbytes);
    rv = kmem_getpages32(npages, KM_SLEEP);
    cmn_err(CE_CONT, "Allocated %d bytes at %lx\n", nbytes, (kptr_t)rv);

    return (kptr_t)rv;
}

void free_from_kmem32(kptr_t addr, unsigned nbytes)
{
    pgcnt_t npages = mmu_btopr(nbytes);
    cmn_err(CE_CONT, "About to free %d bytes at %lx\n", nbytes, addr);
    kmem_freepages32((void *)addr, npages);
    cmn_err(CE_CONT, "Freed %d bytes at %lx\n", nbytes, addr);
}
