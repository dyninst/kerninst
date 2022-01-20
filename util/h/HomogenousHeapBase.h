// HomogenousHeapBase.h
// Pure virtual base class; provides fast allocation and deallocation
// of a heap of equal-sized items (hence, "homogenous").
//
// Note that the only virtual fns are the dtor and create_subheap() -- the allocation
// and deallocation rotuines don't need to be virtual.

// What a derived class needs to provide:
// 1) a create_subheap() method
// 2) A destructor, which will fry all of the subheaps recursively.

#ifndef _HOMOGENOUS_HEAP_BASE_H_
#define _HOMOGENOUS_HEAP_BASE_H_

#include <sys/param.h> // roundup(), Linux
#include <sys/sysmacros.h> // roundup(), Solaris
#include <assert.h>

// Tptr: uint32_t or uint64_t.  Please do NOT make it a real pointer type, or
// we'll get a little confused with pointer arithmetic in the routine
// belongs(), among other places.
// 
// -->>> TO REPEAT: Tptr must be an integer type, not a pointer!!!
//

template <class Tptr, unsigned numAtATime> // Tptr: uint32_t or uint64_t
class HomogenousHeapBase {
 protected:
   unsigned sizeOfT;
   unsigned nBasicElems;
   Tptr rawData; // derived class is responsible for creating this

   unsigned numFree; // first free ndx is numFree-1, but check for numFree==0 first!
   Tptr* freeList;
      // Stack of Tptr.  Allocation pops; deallocation pushes.  Each item is a ptr to
      // a contiguous chunk of <numAtATime> T's.  Of course, numAtATime can be 1.

   HomogenousHeapBase<Tptr, numAtATime> *nextHeap; // actual type is the derived type
      // a linked list of heaps...we cannot keep just one heap (and resize it when
      // needed) because items would dangle in freed memory.
      // When we need to create a new heap, we give it twice at many entries as
      // this one, and put it _before_ this one in the linked list.  Since each
      // new heap has 2x entries of the previous one, the #heaps grows logarithmically
      // The first element in the linked list is always NULL; this makes it possible
      // to squeeze in a new linked list _before_ the current one.

   Tptr allocate();
      // Allocates an item within this heap.  Called by alloc(), below.
      // On failure, returns NULL (derived class can/should then get more space for
      // this heap).  Else returns a ptr to some bits; they will have been intialized
      // to whatever the derived class' ctor decided (perhaps uninitialized raw bits).

   bool belongs(const Tptr ptr) const;
      // Returns true iff the ptr belongs to this heap; does not search next
      // subheap is false.

   void deallocate(Tptr ptr);

   unsigned numBytesThisHeap() const {
      return sizeOfT * nBasicElems;
      // note: we don't also multiply by numAtATime, on purpose (would be wrong)
   }

   // The derived class is responsible for creating subheaps.  It allocates
   // and initializes to taste.  A return value of NULL is allowed; it indicates
   // failure to allocate a sub-heap (out of memory).  Although not yet tested, I
   // hope that we don't choke in such an instance.
   virtual HomogenousHeapBase *create_subheap(unsigned nelems) = 0;

   HomogenousHeapBase(const HomogenousHeapBase &); // explicitly disallowed
   HomogenousHeapBase& operator=(const HomogenousHeapBase &); // explicitly disallowed

   class SUBHEAP {};
   HomogenousHeapBase(SUBHEAP, unsigned sizeOfT,
                      unsigned nBasicElems, Tptr iRawDataToUse);
      // create a sub-heap

   void getMemUsage_internal(unsigned &forRawData, unsigned &forMetaData) const;

 public:

   HomogenousHeapBase(unsigned sizeOfT);
   virtual ~HomogenousHeapBase();

   // NOTE: alloc() doesn't call the ctor for T, and free doesn't call the dtor for T.
   // Not calling the dtor is necessary -- otherwise, it'd get called for a second time
   // when the entire heap is deleted.  The solution is to change the heap classes to
   // allocate just raw memory (malloc).
   Tptr alloc(); // note: not a virtual fn on purpose (only create_subheap() need be)
      // can return 0 (failure), if create_subheap() can return NULL
   void free(Tptr item); // note: not virtual fn on purpose

   unsigned getElemSize() const {
      return sizeOfT;
   }
   
   void getMemUsage(unsigned &forRawData, unsigned &forMetaData) const;
      // returns memory usage, including raw data, free list, etc.
      // won't include any fragmentation, however.
};

template <class Tptr, unsigned numAtATime>
Tptr HomogenousHeapBase<Tptr, numAtATime>::allocate() {
   // allocate w/in a subheap
   if (numFree == 0) {
      // Hmmm; can't do it in this heap; try next heap (if any),
      // even though, because it'll be a smaller heap,
      // there's less of a chance of success with him.

      if (nextHeap == NULL)
         // forget it; no next heap; failure to allocate.
         return 0;
      else
         return nextHeap->allocate();
   }
   else {
      // Got enough in this heap; no need to search the next heap.
      Tptr result = freeList[--numFree];
      assert(belongs(result));
      return result;
   }
}

template <class Tptr, unsigned numAtATime>
void HomogenousHeapBase<Tptr, numAtATime>::
getMemUsage(unsigned &forRawData, unsigned &forMetaData) const {
   // The first heap is just a header.
   nextHeap->getMemUsage_internal(forRawData, forMetaData);
}

template <class Tptr, unsigned numAtATime>
void HomogenousHeapBase<Tptr, numAtATime>::
getMemUsage_internal(unsigned &forRawData, unsigned &forMetaData) const {
   assert(rawData);

   forRawData += nBasicElems * sizeOfT; // raw data

   forMetaData += nBasicElems * sizeof(Tptr*); // freeList
   forMetaData += sizeof(*this); // ourself

   if (nextHeap != NULL)
      nextHeap->getMemUsage_internal(forRawData, forMetaData);
      // and the other heap(s)...
}

// REMINDER: Tptr is an integer type, not a real pointer.  The difference is
// important to avoid ptr arithmetic being performed when we're expecting to
// do regular integer arithmetic!!!

template <class Tptr, unsigned numAtATime>
bool HomogenousHeapBase<Tptr, numAtATime>::belongs(const Tptr ptr) const {
   // can't assert that ptr % sizeOfT is zero; that's way too harsh, like requiring
   // that-much alignment, when in practice, 4 or 8 byte alignment is OK.
   assert(rawData);

   if (ptr < rawData)
      return false;

   const Tptr byte_difference = ptr - rawData;
      // please, no ptr arithmetic here, just raw subtraction of integer addresses!!!

   // asserts for alignment, etc. must wait until we're ready to return a result
   // of true...

   unsigned basic_elem_ndx = byte_difference / sizeOfT;
   if (basic_elem_ndx < nBasicElems) {
      assert(byte_difference % sizeOfT == 0); // helps assert no ptr arithmetic was done
      assert(basic_elem_ndx % numAtATime == 0); // don't call belongs() on a portion
      return true;
   }
   else
      // no asserts
      return false;
}

template <class Tptr, unsigned numAtATime>
void HomogenousHeapBase<Tptr, numAtATime>::deallocate(Tptr freemedata) {
   // does freemedata belong to this heap?
   if (belongs(freemedata))
      freeList[numFree++] = freemedata;
   else {
      // ugh; this can be slow.  Heaps get smaller on down the line.
      assert(nextHeap);
      nextHeap->deallocate(freemedata);
   }
}

template <class Tptr, unsigned numAtATime>
HomogenousHeapBase<Tptr, numAtATime>::
HomogenousHeapBase(SUBHEAP, unsigned isizeOfT,
                   unsigned iNBasicElems, Tptr iRawDataToUse) :
   sizeOfT(isizeOfT),
   nBasicElems(iNBasicElems),
   rawData(iRawDataToUse),
   numFree(nBasicElems / numAtATime),
   freeList(new Tptr[numFree]),
   nextHeap(NULL)
{
   assert(iNBasicElems % numAtATime == 0);
   assert(freeList);

   assert(rawData != 0);
   Tptr freeChunkAddr = rawData; // an integer, not a true pointer
   size_t chunkSize = numAtATime * sizeOfT;
   for (unsigned lcv=0; lcv < numFree; lcv++, freeChunkAddr += chunkSize) {
      // because freeChunkAddr is an integer, not a true pointer, the above
      // "+=chunkSize" is not going to do pointer arithmetic when we don't want
      // it to, thankfully.

      assert((freeChunkAddr - rawData) % sizeOfT == 0);
         // ... % (numAtATime*sizeOfT) would be too harsh
         // pointer arithmetic
     
      freeList[lcv] = freeChunkAddr;
   }
}

/* ************************* public methods ************************************* */

template <class Tptr, unsigned numAtATime>
HomogenousHeapBase<Tptr, numAtATime>::
HomogenousHeapBase(unsigned isizeOfT) :
   sizeOfT(isizeOfT),
   nBasicElems(0),
   rawData(0), // undefined, actually
   numFree(0), // undefined, actually [first heap is just a header]
   freeList(NULL) // could be undefined
{
   // The next sub-heap is really the first one:
   nextHeap = NULL; // we'd like to call create_subheap() now but can't, since one
                    // cannot call a virtual fn within a ctor
}

template <class Tptr, unsigned numAtATime>
HomogenousHeapBase<Tptr, numAtATime>::~HomogenousHeapBase() {
   delete [] freeList; // harmless when NULL (header)

   freeList = NULL; // not strictly necessary, but helps purify find mem leaks
   
   // We don't delete[] rawData; the derived class created it, so it fries it, too.
}

template <class Tptr, unsigned numAtATime>
Tptr HomogenousHeapBase<Tptr, numAtATime>::alloc() {
   // The first HomogenousHeap is just a 'header'; it allows us to adjust who's the
   // first heap.
   assert(nextHeap);
   Tptr result = nextHeap->allocate();
   if (result == 0) {
      // Create new subheap 150% the size of the (soon to be formerly) largest sub-heap
      const unsigned formeryLargestSubHeapNElems = nextHeap->nBasicElems;
      unsigned newBigHeapNBasicElems = formeryLargestSubHeapNElems + (formeryLargestSubHeapNElems >> 1);
      if (newBigHeapNBasicElems == formeryLargestSubHeapNElems)
         ++newBigHeapNBasicElems;

      // align newBigNewNBasicElems:
      newBigHeapNBasicElems = roundup(newBigHeapNBasicElems, numAtATime);
      assert(newBigHeapNBasicElems % numAtATime == 0);

      // The derived class is responsible for creating sub-heaps:
      HomogenousHeapBase *newBigHeap = create_subheap(newBigHeapNBasicElems);

      // NEW: if create_subheap() returns NULL, we don't choke
      if (newBigHeap == NULL)
         return 0; // failure

      newBigHeap->nextHeap = nextHeap;
      nextHeap = newBigHeap;

      // Now the allocation will succeed
      result = nextHeap->allocate();
   }
   
   assert(result);
   return result;
}

template <class Tptr, unsigned numAtATime>
void HomogenousHeapBase<Tptr, numAtATime>::free(Tptr item) {
   // in the spirit of c++ delete, free(NULL) should be harmless:
   if (item == 0)
      return;

   // The first heap is just a header
   nextHeap->deallocate(item);
}

#endif
