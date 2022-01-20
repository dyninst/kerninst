// patchHeap.C
// see .h file for comments

#include "util/h/out_streams.h"
#include "patchHeap.h"

#include <algorithm> // STL's lower_bound()

unsigned patchHeap::allHeapSizes[] = {
   64, 96, 128, 192,
   256, 384, 512, 768,
   960, 1024, 1536, 2048,
   3072, 4096, 8192, 16384,
   32768, 65536,
   UINT_MAX // necessary delimiter
};
patchHeap::patchHeap(kernelDriver &iKernelDriver) :
    nucleusAllocationHasFailed(false), belowAllocationHasFailed(false),
   theKernelDriver(iKernelDriver)
{
    for (unsigned ndx=0; ndx < numHeapSizes; ++ndx) {
	belowNucleusPatchHeaps[ndx] = NULL;
	nucleusPatchHeaps[ndx] = NULL;
	nonNucleusPatchHeaps[ndx] = NULL;
    }
}

patchHeap::~patchHeap() {
   for (unsigned ndx = 0; ndx < numHeapSizes; ++ndx) {
      delete belowNucleusPatchHeaps[ndx]; // harmless if NULL
      delete nucleusPatchHeaps[ndx]; // harmless if NULL
      delete nonNucleusPatchHeaps[ndx]; // harmless if NULL

      belowNucleusPatchHeaps[ndx] = NULL;
      nucleusPatchHeaps[ndx] = NULL;
      nonNucleusPatchHeaps[ndx] = NULL;
   }
}

static void allocateFirstTime(kernelDriver &theKernelDriver,
                              patchHeap1Size *&theHeap,
                              unsigned heapElemNumBytes,
                              unsigned allocType,
                              bool *allocationHasFailed) {
   assert(theHeap == NULL);
   assert(heapElemNumBytes % 4 == 0);

   unsigned numElems = 0;
#ifdef sparc_sun_solaris2_7   
   if (heapElemNumBytes < 1024) {
      // For starters, allocate just enough for a single page (8K)
      numElems = 8192 / heapElemNumBytes;
   }
   else if (heapElemNumBytes < 4096) {
      // Allocate enough for four pages (4*8K=32K)
      numElems = (4*8192) / heapElemNumBytes;
   }
   else if (heapElemNumBytes < 16384) {
      // Allocate 64K
      numElems = (8*8192) / heapElemNumBytes;
   }
#else //ppc64 has 4K pages
   if (heapElemNumBytes < 1024) {
      // For starters, allocate just enough for a single page (8K)
      numElems = 4096 / heapElemNumBytes;
   }
   else if (heapElemNumBytes < 4096) {
      // Allocate enough for four pages (4*8K=32K)
      numElems = (4*4096) / heapElemNumBytes;
   }
   else if (heapElemNumBytes < 16384) {
      // Allocate 64K
      numElems = (8*4096) / heapElemNumBytes;
   }
#endif
   else {
      // Allocate a single item for starting off, since extras have a quite a cost,
      // if unused, at such high allocation sizes.
      numElems = 1;
   }

   assert(numElems > 0);

   try {
      theHeap = new patchHeap1Size(heapElemNumBytes,
                                   theKernelDriver, numElems, allocType);
   }
   catch (kerninst_alloc_fail) {
       *allocationHasFailed = true;
       delete theHeap;
       theHeap = NULL;
       cout << "patchHeap allocateFirstTime(allocType=" << allocType 
	    << ") failed\n";
   }
}

kptr_t patchHeap::alloc(unsigned sz, bool tryForNucleus) {
   kptr_t result = 0;

   // Convert sz into a heap size
   assert(allHeapSizes[numHeapSizes] == UINT_MAX);
   unsigned *heapSizePtr = std::lower_bound(&allHeapSizes[0], &allHeapSizes[numHeapSizes],
                                            sz);
   if (*heapSizePtr == UINT_MAX) {
      dout << "Allocation request of " << sz << " bytes is too big for patch heap"
           << endl;
      throw AllocSizeTooBig();
   }

   const unsigned heapSizeNdx = heapSizePtr - &allHeapSizes[0];
   assert(heapSizeNdx < numHeapSizes);
   
   const unsigned heapSize = *heapSizePtr;
   assert(heapSize >= sz);

   // Note that nucleus allocate can fail, in which case we back off to
   // non-nucleus allocation.
   
   if (tryForNucleus) {
       if (!nucleusAllocationHasFailed &&
	   (result = alloc_nucleus(heapSizeNdx, heapSize)) != 0) {
	   // successfully allocated in nucleus
	   return result;
       }
       nucleusAllocationHasFailed = true;

       // Try allocating from below the nucleus, should be good enough
       if (!belowAllocationHasFailed &&
	   (result = alloc_below(heapSizeNdx, heapSize)) != 0) {
	   return result;
       }
       belowAllocationHasFailed = true;
       // Fall-thru to allocating from the data segment
   }

   result = alloc_nonnucleus(heapSizeNdx, heapSize);
   if (result == 0)
      throw OutOfFreeSpace();
   else
      return result;
}

kptr_t patchHeap::alloc_nucleus(unsigned heapSizeNdx, unsigned heapSize) {
   assert(!nucleusAllocationHasFailed);
   assert(allHeapSizes[heapSizeNdx] == heapSize);
   
   // unlike alloc_nonnucleus(), this routine is allowed to return NULL without
   // panicing.

   patchHeap1Size *&theHeap = nucleusPatchHeaps[heapSizeNdx];
   
   if (theHeap == NULL) {
      allocateFirstTime(theKernelDriver, theHeap, heapSize, 
			ALLOC_TYPE_NUCLEUS,
                        &nucleusAllocationHasFailed);
      if (nucleusAllocationHasFailed) {
	  dout << "patchHeap::alloc_nucleus() heapSize=" << heapSize
	       << " returning 0 because allocateFirstTime() failed" << endl;
	  return 0; // failure
      }
   }

   assert(theHeap);
   assert(nucleusPatchHeaps[heapSizeNdx]);
   assert(theHeap->getElemSize() == heapSize);

   kptr_t result = theHeap->alloc(); // can return 0 (if create_subheap() fails)

   if (result == 0)
      dout << "patchHeap::alloc_nucleus() heapSize="
           << heapSize << " returning 0 because patchHeap1Size's alloc() did"
           << endl;

   return result;
}

kptr_t patchHeap::alloc_nonnucleus(unsigned heapSizeNdx, unsigned heapSize) {
   assert(allHeapSizes[heapSizeNdx] == heapSize);

   patchHeap1Size *&theHeap = nonNucleusPatchHeaps[heapSizeNdx];
   
   if (theHeap == NULL) {
       bool dataAllocationHasFailed = false;
       allocateFirstTime(theKernelDriver, theHeap, heapSize, 
			 ALLOC_TYPE_KMEM32,
			 &dataAllocationHasFailed);
       if (dataAllocationHasFailed) {
	   dout << "patchHeap::alloc_nonnucleus() heapSize=" << heapSize
		<< " aborting because allocateFirstTime() failed" << endl;
	   assert(false);
       }
   }

   assert(theHeap);
   assert(nonNucleusPatchHeaps[heapSizeNdx]);
   assert(theHeap->getElemSize() == heapSize);
   kptr_t result = theHeap->alloc();
   assert(result != 0);
      // unlike nucleus alloc(), this one's not allowed to fail
   return result;
}

kptr_t patchHeap::alloc_below(unsigned heapSizeNdx, unsigned heapSize) 
{
    assert(!belowAllocationHasFailed);
    assert(allHeapSizes[heapSizeNdx] == heapSize);
    patchHeap1Size *&theHeap = belowNucleusPatchHeaps[heapSizeNdx];

    if (theHeap == NULL) {
	allocateFirstTime(theKernelDriver, theHeap, heapSize, 
			  ALLOC_TYPE_BELOW_NUCLEUS,
			  &belowAllocationHasFailed);
	if (belowAllocationHasFailed) {
	    dout << "patchHeap::alloc_below() heapSize=" << heapSize
		 << " returning 0 because allocateFirstTime() failed" << endl;
	    return 0;
	}
    }

    assert(theHeap);
    assert(belowNucleusPatchHeaps[heapSizeNdx]);
    assert(theHeap->getElemSize() == heapSize);
    kptr_t result = theHeap->alloc();

    return result; // may be NULL
}

void patchHeap::free(kptr_t kernelAddr, unsigned sz) {
   // Convert sz into a heap size
   assert(allHeapSizes[numHeapSizes] == UINT_MAX);
   unsigned *heapSizePtr = std::lower_bound(&allHeapSizes[0], &allHeapSizes[numHeapSizes],
                                            sz);
   if (*heapSizePtr == UINT_MAX) {
      dout << "Allocation request of " << sz << " bytes is too big for patch heap"
           << endl;
      throw AllocSizeTooBig();
   }

   const unsigned heapSizeNdx = heapSizePtr - &allHeapSizes[0];
   assert(heapSizeNdx < numHeapSizes);
   
   const unsigned heapSize = *heapSizePtr;
   assert(heapSize >= sz);

   if (theKernelDriver.isInNucleus(kernelAddr)) {
       free_nucleus(kernelAddr, heapSizeNdx, heapSize);
   }
   else if (theKernelDriver.isBelowNucleus(kernelAddr)) {
       free_below(kernelAddr, heapSizeNdx, heapSize);
   }
   else {
       free_nonnucleus(kernelAddr, heapSizeNdx, heapSize);
   }
}

void patchHeap::free_nucleus(kptr_t kernelAddr,
                             unsigned heapSizeNdx, unsigned heapSize) {
   assert(allHeapSizes[heapSizeNdx] == heapSize);
   patchHeap1Size *theHeap = nucleusPatchHeaps[heapSizeNdx];
   assert(theHeap);
   assert(theHeap->getElemSize() == heapSize);

   theHeap->free(kernelAddr);
}

void patchHeap::free_nonnucleus(kptr_t kernelAddr,
                                unsigned heapSizeNdx, unsigned heapSize) {
   assert(allHeapSizes[heapSizeNdx] == heapSize);
   patchHeap1Size *theHeap = nonNucleusPatchHeaps[heapSizeNdx];
   assert(theHeap);
   assert(theHeap->getElemSize() == heapSize);
   
   theHeap->free(kernelAddr);
}

void patchHeap::free_below(kptr_t kernelAddr,
			   unsigned heapSizeNdx, unsigned heapSize) 
{
    assert(allHeapSizes[heapSizeNdx] == heapSize);
    patchHeap1Size *theHeap = belowNucleusPatchHeaps[heapSizeNdx];
    assert(theHeap);
    assert(theHeap->getElemSize() == heapSize);
   
    theHeap->free(kernelAddr);
}
