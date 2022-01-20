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

#include "util/h/HomogenousBlockHeap.h"

template <class T>
int HomogenousBlockHeap<T>::cmpfunc(const void *ptrptr1, const void *ptrptr2) {
   T *ptr1 = *(T**)ptrptr1;
   T *ptr2 = *(T**)ptrptr2;
   return (ptr1 - ptr2);
}

template <class T>
unsigned HomogenousBlockHeap<T>::getTotalNumBaseBlocks() const {
   // returns the number of items allocated in the 'rawData' vector
   // It's 2^(nLevels-1)
   return 1U << (freeLists.size()-1);
}

template <class T>
T *HomogenousBlockHeap<T>::allocateFailure(unsigned lgnblocks) {
   if (nextHeap == NULL) {
      nextHeap = new HomogenousBlockHeap(2 * getTotalNumBaseBlocks(), iBlock);
      assert(nextHeap);
   }
         
   return nextHeap->allocate(lgnblocks);
}

template <class T>
bool HomogenousBlockHeap<T>::obtainFreeBySplittingHigherLevel(unsigned level) {
   if (level == freeLists.size()-1)
      return false; // already at the highest level...sorry

   pdvector<T*> &nextLevelFreeList=freeLists[level+1];
   if (nextLevelFreeList.size() == 0)
      if (!obtainFreeBySplittingHigherLevel(level+1))
	 return false; // it didn't work

   assert(nextLevelFreeList.size() > 0);

   // success.  Grab an item off [level+1]'s free list, split it in half, and
   // put the two pieces in [level]'s free list.
   unsigned nextLevelFreeListMaxNdx = nextLevelFreeList.size()-1;
   T *nextLevelItem = nextLevelFreeList[nextLevelFreeListMaxNdx];
   nextLevelFreeList.resize(nextLevelFreeListMaxNdx);

   // Items at level l are chunks comprised of 2^l T's.
   // Thus, the level l+1 chunk we are splitting contains 2^(l+1) T's.  We will split
   // it into 2 chunks of 2^(l) T's.  Now knowing the sizes, we can obtain the addr
   // of piece1 from the addr of piece0:
   T *piece0 = nextLevelItem;
   unsigned blockNum = getBlockNum(piece0);
   assert(blockNum < getTotalNumBaseBlocks());

   T *piece1 = piece0 + (1U << level); // ptr arith
   blockNum = getBlockNum(piece1);
   assert(blockNum < getTotalNumBaseBlocks());
   

   pdvector<T*> &thisLevelFreeList = freeLists[level];
   thisLevelFreeList += piece0;
   thisLevelFreeList += piece1;

   // Level l contains 2^(n-l-1) items (combined free and allocated); thus, the free
   // list should certainly never exceed that size.
   assert(thisLevelFreeList.size() <= 1U << (freeLists.size()-level-1));
   return true;
}

template <class T>
bool HomogenousBlockHeap<T>::obtainFreeByCoalescingLowerLevel(unsigned level) {
   // Try to obtain a free item at this level, by coalescing two blocks at the next
   // lowest level.  For efficiency, we sort the free blocks at the next lowest level.
   // Since this may be called recursively, we coalesce all that we can.

   if (level == 0)
      return false;

   // Since we may have been called recursively, we should coalesce all that we can.
   (void)obtainFreeByCoalescingLowerLevel(level-1);

   pdvector<T*> &freeListSmaller = freeLists[level-1];
   if (freeListSmaller.size() < 2)
      // need at least 2 entries to coalesce
      return false;

   freeListSmaller.sort(cmpfunc);

   // Note: don't pull the .size() out of the while loop, because .size()
   // changes when we coalesce.
   bool coalescedSome = false;

   unsigned lcv=0;
   while (freeListSmaller.size() >= 2 && lcv < freeListSmaller.size()-1) {
      // If this item is the left side of a buddy combo, and the next guy is our
      // buddy, then coalesce.

      // At level l, blocks are aligned to (2^l)-multiples of T.  Thus, the block
      // number % (2^l) will be zero.  To determine if a block is the left side of a
      // buddy pair, simply see if it _could_ be a block in level l+1; i.e. whether
      // blocknum % 2^(l+1) is zero.

      T *blockPtr = freeListSmaller[lcv];
      const unsigned blockNum = getBlockNum(blockPtr);

      const bool isLeftSideOfABuddy = (blockNum % (1U << (level+1)) == 0);
      if (isLeftSideOfABuddy) {
	 T *buddy = blockPtr + (1U << level);
	    // ptr arith; at level l, blocks are each 2^l minimal blocks
	 assert(blockNum < getTotalNumBaseBlocks());

	 if (freeListSmaller[lcv+1] == buddy) {
	    // bingo; we can coalesce these two
	    freeLists[level] += blockPtr;
               
	    // Now remove these two entries from the smaller-block list.
	    const unsigned lastNdx = freeListSmaller.size()-1;
	    const unsigned nextToLastNdx = lastNdx-1;
	    freeListSmaller[lcv] = freeListSmaller[nextToLastNdx];
	    freeListSmaller[lcv+1] = freeListSmaller[lastNdx];
	    freeListSmaller.resize(nextToLastNdx);

	    coalescedSome = true;
	    continue; // _don't_ increase lcv
	 }
      }
      lcv++;
   }

   return coalescedSome;
}

template <class T>
T *HomogenousBlockHeap<T>::allocate(unsigned lgnblocks) {
   pdvector<T*> &theFreeList = freeLists[lgnblocks];

   if (theFreeList.size() == 0)
      // try to split s.t. we have room at this level
      if (!obtainFreeBySplittingHigherLevel(lgnblocks))
         // failed to find room by splitting bigger blocks; maybe
         // we can find from my coasescing smaller blocks:
         if (!obtainFreeByCoalescingLowerLevel(lgnblocks))
            // failed to find room by coalescing; use 'nextHeap'
	    return allocateFailure(lgnblocks);

   assert(theFreeList.size() > 0);
   unsigned lastndx = theFreeList.size()-1;
   T *result = theFreeList[lastndx];
   theFreeList.resize(lastndx);

   unsigned blockNum = getBlockNum(result);
   assert(blockNum < getTotalNumBaseBlocks());

   return result;
}

/* **************************** public member fns ******************************* */

template <class T>
HomogenousBlockHeap<T>::HomogenousBlockHeap(unsigned totalNumBlocks, const T &iBlocks) :
                                        iBlock(iBlocks) {
   totalNumBlocks = roundUpToPowerOf2(totalNumBlocks);
   const unsigned lgTotalNumBlocks = lg2(totalNumBlocks);

   rawData = new T[totalNumBlocks](iBlock);
   assert(rawData);

   freeLists.resize(lgTotalNumBlocks);
   // Initially, there's just a single item on the free list: it's on
   // the highest free list, and it contains the entire 'rawData'
   freeLists[freeLists.size()-1] += &rawData[0];

   nextHeap = NULL;
}

template <class T>
HomogenousBlockHeap<T>::~HomogenousBlockHeap() {
   if (nextHeap)
      delete nextHeap;
   delete [] rawData;
}

template <class T>
T *HomogenousBlockHeap<T>::alloc(unsigned nBlocks) {
   nBlocks = roundUpToPowerOf2(nBlocks);
   const unsigned lgnBlocks = lg2(nBlocks);
      
   T *result = allocate(lgnBlocks);

   return result;
}

template <class T>
void HomogenousBlockHeap<T>::free(T freeme[], unsigned nBlocks) {
   if (freeme == NULL)
      return; // to match C++'s delete semantics, free of NULL must be harmless

   if (!addressBelongsToHeap(&freeme[0])) {
      assert(nextHeap);
      nextHeap->free(freeme, nBlocks);
      return;
   }

   nBlocks = roundUpToPowerOf2(nBlocks);
   const unsigned lgnBlocks = lg2(nBlocks);

   const unsigned blockNum = getBlockNum(freeme);

   // An item could be a block at level l if blocknum % (2^l) is zero.
   assert(blockNum % (1U << lgnBlocks) == 0);

   freeLists[lgnBlocks] += &freeme[0];
}
