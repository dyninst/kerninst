// ctr64SimpleMetric.C

#include "ctr64SimpleMetric.h"
#include "dataHeap.h"
#include "pidInSetPredicate.h"
#include "add2IntCounter64.h" // the primitive
#include "simpleMetricFocus.h"
#include "common/h/String.h"
#include "kapi.h"

#include <utility> // STL's pair
using std::pair;

extern kapi_manager kmgr;

// Initialize the counter data in the kernel memory
void ctr64SimpleMetric::initialize_counter(kptr_t ctrAddrInKernel,
					   unsigned elemsPerVector,
					   unsigned bytesPerStride) const
{
    sample_t zero = 0;
    kptr_t addr = ctrAddrInKernel;
    for (unsigned i=0; i<elemsPerVector; i++) {
	// Do the writing itself
	kmgr.memcopy(&zero, addr, sizeof(zero));
	addr += bytesPerStride;
    }
}

bool ctr64SimpleMetric::canInstantiateFocus(const focus &theFocus) const {
    if (theFocus.isRoot() || theFocus.isModule()) {
	return false;
    }
    else {
	// Function, basic block, or even instruction.
	// We'll return 'success' as long as the fn has been successfully
	// parsed into basic blocks.
	kapi_function kFunc;
	if (kmgr.findFunctionByAddress(theFocus.getFnAddr(), &kFunc) < 0) {
	    assert(false);
	}
	return !kFunc.isUnparsed();
    }
}

void ctr64SimpleMetric::asyncInstantiate(simpleMetricFocus &smf,
                                         cmfInstantiatePostSmfInstantiation &cb) const {
   const focus &theFocus = smf.getFocus();
   assert(canInstantiateFocus(theFocus));
   
   // STEP 1: Allocate 64-bit integer
   extern dataHeap *theGlobalDataHeap;
   
   const kptr_t uint8KernelAddr = theGlobalDataHeap->alloc8();
   smf.addUint8(uint8KernelAddr);
   
   // STEP 2: Initialize the vector of 64-bit integers:
   initialize_counter(uint8KernelAddr,
		      theGlobalDataHeap->getElemsPerVector8(),
		      theGlobalDataHeap->getBytesPerStride8());
   
   // STEP 3: Generate snippet to increment counter64 by 1, and add them to smf:
   const pdvector<kptr_t> instrumentationPoints = getInstrumentationPoints(theFocus);
   assert(instrumentationPoints.size() > 0); // too strong an assert?  probably not.

   for (pdvector<kptr_t>::const_iterator iter = instrumentationPoints.begin();
        iter != instrumentationPoints.end(); ++iter) {
      const kptr_t launchAddr = *iter;
      assert(launchAddr != 0);

      const pdvector<long> &pids = theFocus.getPids();
      if (pids.size() == 0) {
         snippet s(getPlacement(theFocus),
                   add2IntCounter64(uint8KernelAddr, 1) // 1 is the delta
                   );
         smf.addSnippet(launchAddr, s);
      }
      else {
         snippet s(getPlacement(theFocus),
                   pidInSetPredicate(pids),
                   add2IntCounter64(uint8KernelAddr, 1) // 1 is the delta
                   );
         smf.addSnippet(launchAddr, s);
      }
   }

   // All done with smf instantiation, so we can invoke the callback now.
   cb.smfInstantiationHasCompleted();
}

