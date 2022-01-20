// loggedAllocRequests.C

#include "util/h/kdrvtypes.h"
#include "loggedAllocRequests.h"
#include "nucleusTextAllocation.h"
#include "kernInstIoctls.h"
#include "kmem32Allocation.h"
#include "belowAllocation.h"
#include <sys/sysmacros.h> // roundup()

// These must always go last:
#include <sys/ddi.h>
#include <sys/sunddi.h>

static bool allocate_nonnucleus(unsigned nbytes,
                                unsigned requiredAlignment,
                                kptr_t &result,
                                kptr_t &result_mappable) {
   result = result_mappable = (kptr_t)kmem_alloc(nbytes, KM_SLEEP);

   if (result % requiredAlignment == 0)
      return true;
   else {
      // failed to fulfill alignment requirements
      kmem_free((void*)result, nbytes);
      return false;
   }
}
static void free_nonnucleus(kptr_t addr,
                            kptr_t addr_mappable,
                            unsigned nbytes_allocated) {
   kmem_free((void*)addr, nbytes_allocated);
}

static bool allocate_nucleus(unsigned nbytes,
                             unsigned requiredAlignment,
                             kptr_t &result,
                             kptr_t &result_mappable) {
   result = try_allocate_within_nucleus_text(nbytes,
                                             requiredAlignment, result_mappable);
   if (result == 0)
      return false;
   else
      return true;
}
static void free_nucleus(kptr_t addr, kptr_t addr_mappable,
                         unsigned nbytes_allocated) {
   free_from_nucleus_text(addr, addr_mappable, nbytes_allocated);
}

// Free memory from the 32-bit region
static void free_kmem32(kptr_t addr,
			kptr_t addr_mappable,
			unsigned nbytes_allocated) 
{
    free_from_kmem32(addr, nbytes_allocated);
}

// Allocate nbytes from the 32-bit region with the desired alignment.
// Currently, we allocate memory with the page granularity. A finer grained
// approach may be called for.
static bool allocate_kmem32(unsigned nbytes,
			    unsigned requiredAlignment,
			    kptr_t &result,
			    kptr_t &result_mappable) 
{
    result = result_mappable = allocate_from_kmem32(nbytes);

    if (result == 0) {
	cmn_err(CE_WARN, "allocate_kmem32: could not find memory in "
		"the 32-bit address space\n");
	return false;
    }
    else if (result % requiredAlignment != 0) {
	cmn_err(CE_WARN, "allocate_kmem32: failed to satisfy "
		"the alignment requirements. Freeing up the region\n");
	free_from_kmem32(result, nbytes);
	return false;
    }
    return true;
}

static bool allocate_below(unsigned nbytes, unsigned requiredAlignment,
			   kptr_t &result, kptr_t &result_mappable) 
{
    result = result_mappable = allocate_from_below(nbytes, requiredAlignment);

    return (result != 0);
}

static void free_below(kptr_t addr, kptr_t addr_mappable,
			       unsigned nbytes_allocated) 
{
   free_from_below(addr, nbytes_allocated);
}

static bool allocate(bool (*allocfunc)(unsigned nbytes,
                                       unsigned requiredAlignment,
                                       kptr_t &result,
                                       kptr_t &result_mappable),
                     void (*freefunc)(kptr_t actuallyAllocatedAt,
                                      kptr_t actuallyMappedAt,
                                      unsigned actualNumBytesAllocated),
                     unsigned nbytes,
                     unsigned requiredAlignment,
                     kptr_t &result, kptr_t &result_mappable,
                     kptr_t &actuallyAllocatedAt, kptr_t &actuallyMappedAt,
                     unsigned &actualNumBytesAllocated) {
   // This routine tries twice (if needed), the second time using padding to achieve
   // the desired alignment

   // STEP 1: Try without padding
   bool success = allocfunc(nbytes, requiredAlignment, result, result_mappable);
   if (success) {
      actuallyAllocatedAt = result;
      actuallyMappedAt = result_mappable;
      actualNumBytesAllocated = nbytes;
      return true;
   }

   // STEP 1.5: if requiredAlignment was 1, then failure was NOT due to alignment
   // problems, so give up now.
   if (requiredAlignment == 1)
      return false;
   if (requiredAlignment < 4 || requiredAlignment % 4 != 0)
      cmn_err(CE_PANIC, "loggedAllocRequests.C allocate() requiredAlignment of %d is no good", requiredAlignment);
   
   // STEP 2: Try with padding.  Re-invoke func() with no alignment restrictions
   // but an increased size.
   const unsigned maxPaddingNeeded = requiredAlignment - 4;
      // "- 4" --> we assume that any allocator in the world at least aligns
      // its result to 4 bytes.
   actualNumBytesAllocated = nbytes + maxPaddingNeeded;
   
   success = allocfunc(actualNumBytesAllocated,
                       1, // no alignment restrictions this time (or should we pass 4?)
                       actuallyAllocatedAt, actuallyMappedAt);
      // note the last 2 params we pass; they're not result and result_mappable, on
      // purpose.
   if (!success)
      return false;

   result = roundup(actuallyAllocatedAt, requiredAlignment);
   const unsigned paddingToUse = result - actuallyAllocatedAt;
   result_mappable = actuallyMappedAt + paddingToUse;

   if (result % requiredAlignment != 0 || result_mappable % requiredAlignment != 0) {
      // Allocation succeeded but we still can't get the alignment that we want.
      // Probably means that result and result_mappable don't have the same alignment
      // (one's aligned the way we want it, but the other isn't.)  Can this happen?
      cmn_err(CE_WARN, "loggedAllocRequests.C allocate(): strange, either result or result_mappable isn't aligned after supposedly-successful allocation.");
      freefunc(actuallyAllocatedAt, actuallyMappedAt, actualNumBytesAllocated);
      return false;
   }

   // The end of the useful range, (result+nbytes), should be <= the end of the
   // allocated range, (actuallyAllocatedAt+actualNumBytesAllocated)
   // [<=, and not ==, because we may end up not needed all of that padding after all]
   if (result + nbytes > actuallyAllocatedAt + actualNumBytesAllocated)
      cmn_err(CE_PANIC, "loggedAllocRequests allocate() something's wrong: result=0x%lx nbytes=%d actuallyAllocatedAt=0x%lx maxPaddingNeeded=%d.", result, nbytes, actuallyAllocatedAt, maxPaddingNeeded);

   // invariants thanks to the above asserts:
   // 1) allocation succeeded
   // 2) result is aligned
   // 3) result_mappable is also properly aligned
   // 4) sanity check on the allocation range.
   // We're done.  Have a nice day.
   
   return true;
}


void loggedAllocRequests::alloc(unsigned nbytes,
                                unsigned requiredAlignment,
                                unsigned allocType,
                                kptr_t &result,
                                kptr_t &result_mappable) {
    if (requiredAlignment > 1 && requiredAlignment % 4 != 0) {
	cmn_err(CE_PANIC, "loggedAllocRequests::alloc(): requiredAlignment=%d "
		"is no good; make it 1 or a multiple of 4 please.",
		requiredAlignment);
    }

    kptr_t actuallyAllocatedAt;
    kptr_t actuallyMappedAt;
    unsigned actualNumBytesAllocated;

    switch (allocType) {
    case ALLOC_TYPE_TRY_NUCLEUS:
	cmn_err(CE_WARN, "Obsolete allocation type, returning NULL!\n");
	result = result_mappable = 0;
	return; // don't add to "v" in this case!
    case ALLOC_TYPE_BELOW_NUCLEUS:
	if (!allocate(allocate_below,
		      free_below,
		      nbytes, requiredAlignment,
		      result, result_mappable,
		      actuallyAllocatedAt, actuallyMappedAt, 
		      actualNumBytesAllocated)) {
	    cmn_err(CE_WARN, "loggedAllocRequests::alloc(ALLOC_TYPE_BELOW)"
		    " returning NULL!\n");
	    result = result_mappable = 0;
	    return; // don't add to "v" in this case!
	}
	break;
    case ALLOC_TYPE_NUCLEUS:
	if (!allocate(allocate_nucleus,
		      free_nucleus,
		      nbytes, requiredAlignment,
		      result, result_mappable,
		      actuallyAllocatedAt, actuallyMappedAt, 
		      actualNumBytesAllocated)) {
	    cmn_err(CE_WARN, "loggedAllocRequests::alloc(ALLOC_TYPE_NUCLEUS)"
		    " returning NULL!\n");
	    result = result_mappable = 0;
	    return; // don't add to "v" in this case!
	}
	break;
    case ALLOC_TYPE_DATA:
	if (!allocate(allocate_nonnucleus,
		      free_nonnucleus,
		      nbytes, requiredAlignment,
		      result, result_mappable,
		      actuallyAllocatedAt, actuallyMappedAt, 
		      actualNumBytesAllocated)) {
	    // failed to allocate, even non-nucleus.  Gad-zooks!
	    cmn_err(CE_WARN, "loggedAllocRequests::alloc() returning NULL!\n");
	    result = result_mappable = 0;
	    return; // don't add to "v" in this case!
	}
	break;
    case ALLOC_TYPE_KMEM32:
	if (!allocate(allocate_kmem32,
		      free_kmem32,
		      nbytes, requiredAlignment,
		      result, result_mappable,
		      actuallyAllocatedAt, actuallyMappedAt, 
		      actualNumBytesAllocated)) {
	    // failed to allocate in kmem32
	    cmn_err(CE_WARN, "loggedAllocRequests::alloc() returning NULL!\n");
	    result = result_mappable = 0;
	    return; // don't add to "v" in this case!
	}
	break;
    default:
        cmn_err(CE_PANIC, "loggedAllocRequests::alloc(): Invalid "
	        "allocation type\n");
        break;
    }
   
    // success!
    if (actualNumBytesAllocated < nbytes + (result - actuallyAllocatedAt)) {
	// "<", not "!=", since (result - actuallyAllocatedAt) represents
	// the amount of padding actually used; we may have allocated more.
	cmn_err(CE_PANIC, "loggedAllocRequests.C allocate() "
		"actualNumBytesAllocated mismatch. actualNumBytesAllocated=%d "
		"nbytes=%d result=0x%lx actuallyAllocatedAt=0x%lx",
		actualNumBytesAllocated, nbytes, result, actuallyAllocatedAt);
    }

    v += oneAllocReq(result, // "pretty" result
		     result_mappable, // "pretty" mappable_result
		     nbytes, // "pretty" numbytes
		     allocType,
		     actuallyAllocatedAt, // actual "result"
		     actuallyMappedAt,    // actual "result_mappable"
		     actualNumBytesAllocated // actual nbytes allocated
	);
}

void loggedAllocRequests::undo() {
    const unsigned sz = v.size();
    if (sz > 0) {
	cmn_err(CE_CONT, "kerninst NOTE: deleting %d leftover alloc "
		"requests.\n", sz);
    }
   
    vectype::iterator iter = v.begin();
    vectype::iterator finish = v.end();
    for (; iter != finish; ++iter) {
	switch (iter->allocType) {
	case ALLOC_TYPE_BELOW_NUCLEUS:
	    free_below(iter->actuallyAllocatedAt,
		       iter->actuallyMappedAt,
		       iter->actualNumBytesAllocated);
	    break;
	case ALLOC_TYPE_NUCLEUS:
	    free_from_nucleus_text(iter->actuallyAllocatedAt,
				   iter->actuallyMappedAt,
				   iter->actualNumBytesAllocated);
	    break;
	case ALLOC_TYPE_DATA:
	    kmem_free((void*)iter->actuallyAllocatedAt, 
		      iter->actualNumBytesAllocated);
	    break;
	case ALLOC_TYPE_KMEM32:
	    free_kmem32(iter->actuallyAllocatedAt,
			iter->actuallyMappedAt,
			iter->actualNumBytesAllocated);
	    break;
	default:
	    cmn_err(CE_PANIC, "loggedAllocRequests::undo(): Invalid "
		    "allocation type\n");
	    break;
	}
    }

    v.clear();
    destroy_below_allocator();
}

void loggedAllocRequests::free(kptr_t kernelAddr,
                               kptr_t mappedKernelAddr,
                               unsigned nbytes) {
    if (kernelAddr == 0) {
	cmn_err(CE_WARN, "loggedAllocRequests::free ignoring attempt "
		"to free NULL pointer");
	return;
    }
   
    // search for it
    vectype::iterator iter = v.begin();
    vectype::iterator finish = v.end();
    for (; iter != finish; ++iter) {
	const oneAllocReq &info = *iter;
         
	if (info.prettyAddr == kernelAddr) {
	    if (info.prettyMappedAddr != mappedKernelAddr ||
		info.prettyNumBytes != nbytes) {
		cmn_err(CE_WARN, "loggedAllocRequests::free() mismatched "
			"pretty mapped addr and/or numbytes; not freeing");
		continue;
	    }

	    switch (info.allocType) {
	    case ALLOC_TYPE_BELOW_NUCLEUS:
		free_below(info.actuallyAllocatedAt,
			   info.actuallyMappedAt,
			   info.actualNumBytesAllocated);
		break;
	    case ALLOC_TYPE_NUCLEUS:
		free_from_nucleus_text(info.actuallyAllocatedAt,
				       info.actuallyMappedAt,
				       info.actualNumBytesAllocated);
		break;
	    case ALLOC_TYPE_DATA:
		kmem_free((void*)info.actuallyAllocatedAt, 
			  info.actualNumBytesAllocated);
		break;
	    case ALLOC_TYPE_KMEM32:
		free_kmem32(info.actuallyAllocatedAt,
			    info.actuallyMappedAt,
			    info.actualNumBytesAllocated);
		break;
	    default:
		cmn_err(CE_PANIC, "loggedAllocRequests::free(): Invalid "
			"allocation type\n");
		break;
	    }

	    *iter = v.back();
	    v.pop_back();
	    return;
	}
    }
      
    // could not find that kernelAddr
    cmn_err(CE_CONT, "kerninst warning: attempt to free logged alloc request "
	    "at 0x%lx: unknown kernelAddr.  Ignoring.\n",
	    kernelAddr);
}

// ------------------------------------------------------------

extern "C" void *initialize_logged_allocRequests(void) {
   loggedAllocRequests *theLoggedAllocRequests = 
      (loggedAllocRequests*)kmem_alloc(sizeof(loggedAllocRequests), KM_SLEEP);
   (void)new((void*)theLoggedAllocRequests)loggedAllocRequests();
      // no params to ctor

   return theLoggedAllocRequests;
}

extern "C" void destroy_logged_allocRequests(void *ptr) {
   loggedAllocRequests &theLoggedAllocRequests = *(loggedAllocRequests*)ptr;
   theLoggedAllocRequests.undo();
   
   theLoggedAllocRequests.~loggedAllocRequests();
   kmem_free(ptr, sizeof(loggedAllocRequests));
}

