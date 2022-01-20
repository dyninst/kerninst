// HomogenousBlockHeap.h
// Ariel Tamches
// Class to provde fast allocation and deallocation of a heap of homogenous
// items.  The difference is that this class also lets you allocate a contiguous
// block of such items (vectors), whereas HomogenousHeap only lets you allocate
// one at a time.
// Basically, this is a "buddy heap" algorithm.  We use lazy coalescing
// (only when allocation fails), so deletion is always fast.
// We avoid the need for a bitmap to track allocated & free blocks;
// the only time we'd need to use a bitmap is in coalescing (to detect
// when a buddy is free)...and we get around this by sorting a level's free
// lists.

#ifndef HOMOGENOUS_BLOCK_HEAP_H_
#define HOMOGENOUS_BLOCK_HEAP_H_

#include <iostream.h>
#include "common/h/Vector.h"

template <class T>
class HomogenousBlockHeap {
 private:
   T *rawData;
   pdvector< pdvector<T*> > freeLists; // indexed by lg(nblocks)
   
   HomogenousBlockHeap *nextHeap;

   T iBlock;

 private:
   unsigned getTotalNumBaseBlocks() const;
      // the # items allocated within 'rawData'; it's 2^(nLevels-1)

   T *allocateFailure(unsigned lgnblocks);

   bool obtainFreeBySplittingHigherLevel(unsigned level);

   bool obtainFreeByCoalescingLowerLevel(unsigned level);

   T *allocate(unsigned lgnblocks);

   static int cmpfunc(const void *ptrptr1, const void *ptrptr2);

   unsigned roundUpToPowerOf2(unsigned num) {
      unsigned result = 1;

      while (result < num)
	 result <<= 1;

      return result;
   }

   unsigned lg2(register unsigned num) {
      register unsigned result=0;
      while (num > 1) {
         result++;
         num /= 2;
      }
      return result;
   }

   bool addressBelongsToHeap(T freeme[]) const {
      T *firstItemPtr = &rawData[0];
      T *firstFreemePtr = &freeme[0];

      if (firstFreemePtr < firstItemPtr)
         return false;

      const unsigned blockNum = firstFreemePtr - firstItemPtr;
      if (blockNum >= getTotalNumBaseBlocks())
         return false;

      return true;
   }

   unsigned getBlockNum(T freeme[]) const {
      T *firstItemPtr = &rawData[0];
      T *firstFreemePtr = &freeme[0];

      assert(firstFreemePtr >= firstItemPtr);
      
      const unsigned blockNum = firstFreemePtr - firstItemPtr;
      assert(blockNum < getTotalNumBaseBlocks());

      return blockNum;
   }   

 public:
   HomogenousBlockHeap(unsigned totalNumBlocks, const T &iBlocks);
  ~HomogenousBlockHeap();

   T *alloc(unsigned nBlocks);

   void free(T freeme[], unsigned nBlocks);
};

#endif
