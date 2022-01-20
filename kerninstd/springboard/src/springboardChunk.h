// springboardChunk.h

#ifndef _ONE_SPRING_BOARD_HEAP_H_
#define _ONE_SPRING_BOARD_HEAP_H_

// We can't use HomogenousHeap since its initialization facilities aren't
// suited for us (we'd like to initialize each item with a different
// springboard_addr)

#include "common/h/Vector.h"
#include <limits.h>
#include <inttypes.h>
#include "util/h/kdrvtypes.h"
#include "kernelDriver.h"

class springboardChunk {
 private:
   kptr_t chunkStartAddr;

   pdvector<unsigned> free_items;
   // stack of ndxs of the free items, so allocation & deallocation can be
   // fast.  free_items.size() remains constant.
   // Think of an ndx as being an offset from 'chunkStartAddr'.  So if
   // we pop the number 5 off the stack, it corresponds to the springboard
   // item starting at (chunkStartAddr + 5*sizeOfSpringBoardItem())

   unsigned top; // UINT_MAX if no more free items

 public:
   springboardChunk(kptr_t ichunkStartAddr, unsigned totalnbytes);
  ~springboardChunk();

   springboardChunk(const springboardChunk &src);
   springboardChunk &operator=(const springboardChunk &);

   pair<uint32_t, uint32_t> calcNumBytesInChunk(kptr_t nucleusAddrLow,
                                                kptr_t nucleusAddrHi) const;
      // .first: # bytes within the nucleus; .second: # bytes outside the nucleus
      // Note that this returns how many bytes are in the heap, regardless of
      // whether some of it has already been allocated.

   bool operator<(const springboardChunk &other) const {
      return (chunkStartAddr < other.chunkStartAddr);
   }

   kptr_t getStartAddr() const {
      return chunkStartAddr;
   }
   
   bool containsAddress(kptr_t addr) const {
      const kptr_t heapEndAddr = chunkStartAddr +
	      sizeOfSpringBoardItem()*free_items.size();
      return (addr >= chunkStartAddr && addr < heapEndAddr);
   }
   
   static unsigned sizeOfSpringBoardItem() {
      if (sizeof(kptr_t) == sizeof(uint32_t)) {
	 // 32-bit version: save; call; restore
	 return 12; 
      }
      else {
         // 64-bit version: save; genImmAddr(hi) -> l0; jmpl l0+lo, l0; restore
         assert(sizeof(kptr_t) == sizeof(uint64_t));
         return 24; 
      }
   }
   
   kptr_t ndxToAddr(unsigned ndx) const {
      return chunkStartAddr + ndx*sizeOfSpringBoardItem();
   }

   unsigned getMaxNdx() const {
      assert(free_items.size() > 0);
      return free_items.size()-1;
   }

   bool hasASpringboardWhoseStartAddrIsInRangeOf(kptr_t lowestOKAddr,
                                                 kptr_t highestOKAddr) const;
      // (not that the springboard is free...it could be allocated and we still
      // return true)
      
   unsigned allocSpringboardWithStartAddrWithinRangeOf(kptr_t lowestAddr,
                                                       kptr_t highestAddr,
                                                       kptr_t fromAddr,
                                                       kptr_t toAddr,
                                                       unsigned num32ByteICacheLines);
   // A suitable springboard is one whose starting address falls within the
   // bounds of the passed-in parameters.  returns an ndx, you can get an
   // address by calling ndxToAddr().  returns UINT_MAX if a suitable entry
   // isn't found in this chunk.  The last two params help us avoid icache
   // line conflicts.  numCacheLines: if -1U then we don't pay attention to
   // icache placement.  Else pretend that the cache has that many lines
   // (assume cache line size is 32 bytes) and try to allocate without
   // conflicts.  First pass 256 (pretend 8K cache), then pass 512 if that
   // fails (pretend 16K cache).  This is the best way to allocate under a
   // 2-way set associative scheme.

   void free(unsigned ndx);
};


#endif
