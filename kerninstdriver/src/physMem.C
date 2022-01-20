// physMem.C

#include "util/h/kdrvtypes.h"
#include "physMem.h"
#include "loggedMem.h"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/param.h> // btop()
#include <sys/kmem.h> // kmem_alloc

#include "vm/seg_kmem.h"

#include <sys/mem.h> // for extern caddr_t mm_map

#include "mm_map.h" // get_mm_map()

// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>

extern "C" int kcopy(void *from, void *to, size_t cnt);

extern "C" void hot_patch_kernel_text(caddr_t iaddr, uint32_t new_instr, u_int size);
// in subr.c, used by lockstat

extern "C" void membar_sync();

extern "C" uint64_t ari_get_tick_raw();

extern "C" void flush_addr_from_dcache(uintptr_t va); /* asm file */

// This routine evicts the given address from the D-cache by reading 
// garbage from flush_buffer. This approach will not work with
// set-associative caches, so we use flush_addr_from_dcache() instead
int evict_addr_from_dcache(kptr_t iaddr)
{
    extern int dcache_size;
    extern int dcache_linesize;

    const int words_in_cache = dcache_size / sizeof(int);
    const int words_in_line = dcache_linesize / sizeof(int);
    static uintptr_t flush_buffer = 0;
    
    if (flush_buffer == 0) {
	flush_buffer = (uintptr_t)kmem_alloc(dcache_size, KM_SLEEP);
	if (flush_buffer == 0) {
	    cmn_err(CE_PANIC, "flush_dcache: kmem_alloc failure\n");
	}
    }

    uintptr_t delta = iaddr & (dcache_size - 1);
    uintptr_t eps = flush_buffer & (dcache_size - 1);
    uint32_t *vaddr = (uint32_t *)(flush_buffer + ((dcache_size + delta - eps) 
						   & (dcache_size - 1)));
    int rv = *vaddr;

    return rv;
}

int write1AlignedWordToNucleus(kptr_t addr, uint32_t val) 
{
    hot_patch_kernel_text((caddr_t)addr, val, 4);
    membar_sync(); // sparcv9_subr.s

    // hot_patch_kernel_text() maps the physical page we want to modify
    // with write permissions at a different virtual address. Writing
    // to this page may result in two copies of the data cached at different
    // D-cache locations (aka synonyms). To avoid inconsistencies, we need to
    // purge the stale data from the cache.
    flush_addr_from_dcache(addr);

    return 0; // success
}

int write1AlignedWordToNonNucleus(kptr_t addr, uint32_t val) {
   if (addr % 4 != 0)
      cmn_err(CE_PANIC, "write1AlignedWordToNonNucleus: addr %lx is not aligned! (trying to write value %x)\n", addr, val);
   
   *(uint32_t*)addr = val;

   return 0; // success
}

int write1UndoableAlignedWord(void *iLoggedWrites,
                              kptr_t addr, int maybeNucleus,
                              uint32_t val, unsigned howSoon,
                              uint32_t *oldval_fillin) {
   loggedWrites *theLoggedWrites = (loggedWrites*)iLoggedWrites;

   if (addr % 4 != 0)
      cmn_err(CE_PANIC, "write1UndoableAlignedWordMaybeNucleus: addr=%lx is not aligned!\n", addr);
   
   const uint32_t origval = *(uint32_t*)addr;
   *oldval_fillin = origval;

   return theLoggedWrites->performUndoableAlignedWrite(addr,
                                                       maybeNucleus ? true : false,
                                                       val, origval, howSoon);
}

int undoWriteWordNucleus(void *iLoggedWrites,
                         kptr_t addr,
                         uint32_t oldval_to_set_to) {
   loggedWrites *theLoggedWrites = (loggedWrites*)iLoggedWrites;
   return theLoggedWrites->undoWriteWordNucleus(addr,
                                                oldval_to_set_to);
}

int undoWriteWordNonNucleus(void *iLoggedWrites,
                            kptr_t addr,
                            uint32_t oldval_to_set_to) {
   loggedWrites *theLoggedWrites = (loggedWrites*)iLoggedWrites;
   return theLoggedWrites->undoWriteWordNonNucleus(addr,
                                                   oldval_to_set_to);
}

void changeUndoWriteWordHowSoon(void *iLoggedWrites,
                                kptr_t kernelAddr, unsigned newHowSoon) {
   loggedWrites *theLoggedWrites = (loggedWrites*)iLoggedWrites;
   theLoggedWrites->changeUndoWriteWordHowSoon(kernelAddr, newHowSoon);
}

   
