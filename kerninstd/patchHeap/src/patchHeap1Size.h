// patchHeap1Size.h
// Ariel Tamches
// This class manges a heap of a single patch size.
// You'll need several "patchHeap1Size"s (of different nbytes)
// to accomodate varying-length patches.

#ifndef _PATCH_HEAP_1SIZE_H_
#define _PATCH_HEAP_1SIZE_H_

#include "util/h/HomogenousHeapBase.h"
#include <iostream.h>
#include <unistd.h> // ioctl()
#include <assert.h>
#include <stdio.h> // perror()
#include "kernelDriver.h"

class kerninst_alloc_fail {};
   
// Note kptr_t as a template argument: as it should be: an integer type,
// NOT a true pointer type.  See HomogenousHeapBase.h for commentary on that.
class patchHeap1Size : public HomogenousHeapBase<kptr_t, 1> {
 private:
   typedef HomogenousHeapBase<kptr_t, 1> inherited;

   unsigned allocType;
   kernelDriver &theKernelDriver;

   // NOTE: There is no ptr for location in our memory.  So, to read or write
   // the patch, read() or write() from /dev/kmem.  If we needed quick, frequent
   // access to patches (which we don't), then we would mmap /dev/kmem.
   uint32_t actualHeapSize; // a multiple of page len
   kptr_t kernelAddrForMapping; // complements "rawData" (of the base class)

   class SUBHEAP {};
   patchHeap1Size(SUBHEAP,
                  unsigned sizeOfPatch, // becomes sizeOfT in the base class
                  const pair<kptr_t, kptr_t> &kernelAddrs,
                  kernelDriver &theKernelDriver, unsigned numPatches,
                  uint32_t actualHeapSize,
                  unsigned iAllocType);

   inherited *create_subheap(unsigned numPatches);
      // called when initially creating the sub-heap; also called when
      // HomogenousHeapBase()'s alloc() needs more space.  Thus, patchHeap
      // automatically get new space when it runs out.
      // Returns NULL on failure

   patchHeap1Size(const patchHeap1Size &);
   patchHeap1Size& operator=(const patchHeap1Size &);
   
 public:
   patchHeap1Size(unsigned sizeOfPatch, // becomes sizeOfT in the base class
                  kernelDriver &, unsigned numPatches, unsigned iAllocType);
      // can throw nucleus_alloc_fail() if nucleus is true
  ~patchHeap1Size();
};

#endif
