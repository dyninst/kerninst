// springboardChunk.C

#include "util/h/out_streams.h"
#include "springboardChunk.h"
#include <iostream>

springboardChunk::springboardChunk(kptr_t startAddr, unsigned nbytes) {
   chunkStartAddr = startAddr;
   const unsigned total_num_items = nbytes / sizeOfSpringBoardItem();
   
   free_items.resize(total_num_items);
   for (unsigned lcv=0; lcv < total_num_items; lcv++) // change to STL style
      free_items[lcv] = lcv;

   top = total_num_items - 1;
}

springboardChunk::springboardChunk(const springboardChunk &src) :
      free_items(src.free_items) {
   chunkStartAddr = src.chunkStartAddr;
   top = src.top;
}

springboardChunk &springboardChunk::operator=(const springboardChunk &src) {
   if (this == &src) return *this;
   
   chunkStartAddr = src.chunkStartAddr;
   free_items = src.free_items;
   top = src.top;

   return *this;
}

springboardChunk::~springboardChunk() {
}

pair<uint32_t,uint32_t> springboardChunk::
calcNumBytesInChunk(kptr_t nucleusBoundsLow, kptr_t nucleusBoundsHi) const {
   pair<uint32_t, uint32_t> result;
   result.first = result.second = 0;

   const unsigned nbytes = free_items.size() * sizeOfSpringBoardItem();

   const bool startaddr_within_nucleus = (chunkStartAddr >= nucleusBoundsLow &&
                                          chunkStartAddr <= nucleusBoundsHi);

   const kptr_t chunkLastAddr =
      chunkStartAddr + nbytes - 1;
   
   const bool lastaddr_within_nucleus = (chunkLastAddr >= nucleusBoundsLow &&
                                         chunkLastAddr <= nucleusBoundsHi);

   assert(startaddr_within_nucleus == lastaddr_within_nucleus);

   if (startaddr_within_nucleus)
      result.first += nbytes;
   else
      result.second += nbytes;

   assert(result.first + result.second == nbytes);
   
   return result;
}

bool springboardChunk::
hasASpringboardWhoseStartAddrIsInRangeOf(kptr_t lowestOKAddr,
                                         kptr_t highestOKAddr) const {
   kptr_t lowestStartAddr = chunkStartAddr;
   kptr_t highestStartAddr = chunkStartAddr + getMaxNdx()*sizeOfSpringBoardItem();

   bool lowestStartAddrFallsWithinBounds = (lowestStartAddr >= lowestOKAddr &&
                                            lowestStartAddr <= highestOKAddr);
   bool highestStartAddrFallsWithinBounds = (highestStartAddr >= lowestOKAddr &&
                                             highestStartAddr <= highestOKAddr);
   
   return (lowestStartAddrFallsWithinBounds || highestStartAddrFallsWithinBounds);
}
   
static unsigned vaddr2icacheline(kptr_t addr,
                                 unsigned numICacheLines) {
   return (addr / 32) % numICacheLines;
}

unsigned springboardChunk::
allocSpringboardWithStartAddrWithinRangeOf(kptr_t lowestAddr,
                                           kptr_t highestAddr,
                                           kptr_t fromAddr,
                                           kptr_t toAddr,
                                           unsigned numICacheLines) {
   // numCacheLines: start it off with 256 (pretend 8K icache).  If
   // allocation fails then retry with 512 (pretend 16K icache with 2-way
   // set associativity).  If allocation fails again then retry with -1U,
   // which'll take the first within range springboard.

   if (top == UINT_MAX)
      return UINT_MAX;

   const unsigned fromAddrCacheLine = vaddr2icacheline(fromAddr, numICacheLines);
   const unsigned toAddrCacheLine = vaddr2icacheline(toAddr, numICacheLines);

   unsigned free_items_ndx = top;
   do {
      unsigned result = free_items[top];
      kptr_t address = chunkStartAddr + result*sizeOfSpringBoardItem();
      if (address >= lowestAddr && address <= highestAddr) {
         // We have a potential match.  
	 // But let's try to avoid cache line conflicts.

         const unsigned addressStartCacheLine = vaddr2icacheline(address,
                                                                 numICacheLines);
         const unsigned addressEndCacheLine = vaddr2icacheline(address, numICacheLines);

         if (numICacheLines == -1U ||
             (fromAddrCacheLine != addressStartCacheLine &&
              fromAddrCacheLine != addressEndCacheLine &&
              toAddrCacheLine != addressStartCacheLine &&
              toAddrCacheLine != addressEndCacheLine)) {
//              cout << "springboard alloc successful using cache lines: from="
//                   << fromAddrCacheLine << " to=" << toAddrCacheLine
//                   << " sbBeginning=" << addressStartCacheLine
//                   << " sbEnd=" << addressEndCacheLine << endl;

            if (top-- == 0)
               top = UINT_MAX;

            return result;
         }
         else
            dout << "Would have been springboard icache conflict...looking for a better match" << endl;
      }
   } while (free_items_ndx-- != 0);

   // failure
   return UINT_MAX;
}

void springboardChunk::free(unsigned ndx) {
   // first, find the ndx of the item
   if (top == UINT_MAX)
      // was empty; now not empty.
      top = 0;
   else
      top++;
   
   free_items[top] = ndx;
}
