// SpringBoardHeap.h
// The sb heap contains springboard chunks, which in turn contain springboards.

#ifndef _SPRING_BOARD_HEAP_H_
#define _SPRING_BOARD_HEAP_H_

#include "springboardChunk.h"
#include "util/h/kdrvtypes.h"
#include "util/h/mrtime.h"

class SpringBoardHeap {
 public:
   class CouldntFindOneFreeInRange {};
   
   struct item {
      unsigned chunk_ndx; // identifies a springboardChunk
      unsigned item_within_chunk_ndx; // identifies an item within that oneHeap
   };

   // The following 2 structures are used when performing a binary search
   struct okItemBounds {
      kptr_t lowOK;
      kptr_t hiOK;
   };
   struct okItemBoundsComparitor_less {
      bool operator()(const springboardChunk &oneHeap, 
		      const okItemBounds &ok) const {
         // return true iff oneHeap is below ok
         kptr_t heapEnd = oneHeap.ndxToAddr(oneHeap.getMaxNdx());

         return (heapEnd < ok.lowOK);
            // true if the heap is entirely before ok
      }
      bool operator()(const okItemBounds &ok, const springboardChunk &oneHeap) const {
         // return true iff ok is below oneHeap
         kptr_t heapStart = oneHeap.ndxToAddr(0);
         return (ok.hiOK < heapStart);
      }
   };
   
 private:
   static bool verboseTimings;
   
   pdvector<springboardChunk> chunks;
   bool chunksPresentlySorted;

   item alloc_part2(kptr_t lowestOKAddr,
                    kptr_t highestOKAddr,
                    pdvector<springboardChunk>::iterator low,
                    pdvector<springboardChunk>::iterator hi,
                    kptr_t fromAddr, kptr_t toAddr,
                    unsigned num32ByteICacheLines,
                    mrtime_t startTime)
                                               throw(CouldntFindOneFreeInRange);
 public:
   SpringBoardHeap();
  ~SpringBoardHeap();

   static void setVerboseTimings();

   unsigned numChunks() const { return chunks.size(); }
   pair<uint32_t,uint32_t> calcNumBytesInHeap(const kernelDriver &theKernelDriver) const;
      // .first: # bytes within nucleus; .second: # bytes outside it
      // returns the total value, regardless of how much, if any, is presently
      // allocated.
   
   void addChunk(kptr_t startAddr, unsigned len);
   void removeChunk(kptr_t startAddr);
   void sortChunks(); // must be called before alloc()
   
   item alloc(kptr_t lowestOKAddr, kptr_t highestOKAddr,
              kptr_t comingFromAddr, // so we can avoid a cache line conflict
              kptr_t goingToAddr // so we can avoid a cache line conflict
              )
         throw(CouldntFindOneFreeInRange);
   // params: since reach isn't infinite, the caller passes in an address
   // range; we make sure that we allocate a springboard whose *start* addr
   // falls within this range, since one that doesn't can't be reached from
   // the splice point.

   void free(const item &);

   kptr_t item2addr(const item &);
};

#endif
