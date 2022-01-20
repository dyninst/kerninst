// patchHeap.h
// Why do we use our own patch heap, instead of relying on kmem_alloc()?
// Because we may want to measure kmem_alloc().
// Also because each call to kmem_alloc() would require jumping into the kernel.
// Also because we want better control for allocating out of the nucleus verses
// non-nucleus, etc.

// Thanks to logic within the base class (HomogenousHeapBase.h), each individual
// patchHeap1Size<> will automatically grab more kernel heap space when it needs
// to (method create_subheap() of patchHeap1Size).  Thus OutOfFreeSpace will
// probably never be thrown.  AllocSizeTooBig, on the other hand, ...

#ifndef _PATCH_HEAP_H_
#define _PATCH_HEAP_H_

#include "patchHeap1Size.h"
#include "kernelDriver.h"

class patchHeap {
 public:
   class AllocSizeTooBig {};
   class OutOfFreeSpace {};
   
 private:
   static unsigned allHeapSizes[];
   static const unsigned numHeapSizes = 18;

   // note: patchHeap1Size wants multiples of 4 bytes in size.
   // Each of the following heaps is a pointer so that it can be NULL (i.e., lazy
   // allocation of the heap to save memory).
   patchHeap1Size *belowNucleusPatchHeaps[numHeapSizes];
   patchHeap1Size *nucleusPatchHeaps[numHeapSizes];
   patchHeap1Size *nonNucleusPatchHeaps[numHeapSizes];

   bool nucleusAllocationHasFailed;
   bool belowAllocationHasFailed;

   kernelDriver &theKernelDriver;

   kptr_t alloc_below(unsigned heapSizeNdx, unsigned heapSize);
   kptr_t alloc_nucleus(unsigned heapSizeNdx, unsigned heapSize);
   kptr_t alloc_nonnucleus(unsigned heapSizeNdx, unsigned heapSize);

   void free_below(kptr_t, unsigned heapSizeNdx, unsigned heapSize);
   void free_nucleus(kptr_t, unsigned heapSizeNdx, unsigned heapSize);
   void free_nonnucleus(kptr_t, unsigned heapSizeNdx, unsigned heapSize);
   
 public:
   patchHeap(kernelDriver &theKernelDriver);
  ~patchHeap();
      // frees up anything left in the individual patchHeap1Size's
   
   kptr_t alloc(unsigned sz, bool tryForNucleus);
      // returns in-kernel address
      // could throw AllocSizeTooBig or OutOfFreeSpace exceptions
      // tryForNucleus: we try to allocate in the nucleus; if we can't, then we
      //    grudgingly allocate elsewhere.

   void free(kptr_t kernelAddr, unsigned sz);
      // first arg must be the in-kernel address returned
      // by an earlier call to alloc()
};

#endif
