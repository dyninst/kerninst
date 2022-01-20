// patchHeap1Size.C

#include "util/h/out_streams.h"
#include "kernInstIoctls.h"
#include "patchHeap1Size.h"

/* ************************* private methods ************************************* */

patchHeap1Size::patchHeap1Size(SUBHEAP,
                               unsigned isizeOfPatch,
                               const pair<kptr_t, kptr_t> &kernelAddrs,
                               kernelDriver &iKernelDriver,
                               unsigned numPatches,
                               unsigned iactualHeapSize,
                               unsigned iAllocType) :
   inherited(inherited::SUBHEAP(), isizeOfPatch, 
	     numPatches, kernelAddrs.first),
   allocType(iAllocType),
   theKernelDriver(iKernelDriver),
   actualHeapSize(iactualHeapSize)
{
   // sizeOfT from the base class: should equal sizeof(patch)
   assert(sizeOfT == isizeOfPatch);
   assert(actualHeapSize >= numPatches*sizeOfT);
   assert(actualHeapSize == kernelDriver::roundUpToMultipleOfPageSize(actualHeapSize));
   kernelAddrForMapping = kernelAddrs.second;
}

HomogenousHeapBase<kptr_t, 1> * // return type
patchHeap1Size::create_subheap(unsigned numPatches) {
   const unsigned nbytes =
      kernelDriver::roundUpToMultipleOfPageSize(numPatches * sizeOfT);
   
   const pair<kptr_t, kptr_t> kernelAddrs =
       theKernelDriver.allocate_kernel_block(nbytes, 32, false, allocType);
       // 32 --> we want icache-line alignment please
   if (kernelAddrs.first == 0 || kernelAddrs.second == 0) {
       return NULL; // sentinel.  High-level alloc() will now return NULL.
   }
   
   return new patchHeap1Size(SUBHEAP(),
                             sizeOfT,
                             kernelAddrs,
                             theKernelDriver, numPatches, nbytes, allocType);
}

/* ************************* public methods ************************************* */

patchHeap1Size::
patchHeap1Size(unsigned isizeOfPatch,
               kernelDriver &iKernelDriver, unsigned numPatches, 
	       unsigned iAllocType) :
   inherited(isizeOfPatch),
   allocType(iAllocType),
   theKernelDriver(iKernelDriver)
{
   // The first heap is just a header.
   // 'nextHeap' is the first real heap.

   assert(sizeOfT == isizeOfPatch); // sizeOfT from the base class
   nextHeap = create_subheap(numPatches);
   if (nextHeap == NULL) {
       throw kerninst_alloc_fail();
   }
}

patchHeap1Size::~patchHeap1Size() {
   if (rawData != 0) { // in particular, the first heap is just a header
      assert(actualHeapSize ==
             kernelDriver::roundUpToMultipleOfPageSize(actualHeapSize));
      theKernelDriver.free_kernel_block(rawData, kernelAddrForMapping, 
					actualHeapSize);
      rawData = 0;
   }
   
   delete nextHeap; // harmless (and stops the recursion of dtors) if NULL
   nextHeap = NULL;
}

