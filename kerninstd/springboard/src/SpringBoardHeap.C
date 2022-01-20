// SpringBoardHeap.C

#include "kernelDriver.h"
#include "SpringBoardHeap.h"

#include <algorithm> // stl's sort()
#include "util/h/out_streams.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"

bool SpringBoardHeap::verboseTimings = false;

void SpringBoardHeap::setVerboseTimings() {
   verboseTimings = true;
}

SpringBoardHeap::SpringBoardHeap() {
   // chunks: initially, an empty vector

   chunksPresentlySorted = true;
}

SpringBoardHeap::~SpringBoardHeap() {
}

pair<uint32_t,uint32_t>
SpringBoardHeap::calcNumBytesInHeap(const kernelDriver &theKernelDriver) const {
   // result.first: in nucleus
   // result.second: outside the nucleus
   pair<uint32_t, uint32_t> result;
   result.first = result.second = 0;

   const pair<kptr_t, kptr_t> theNucleusTextBounds = theKernelDriver.queryNucleusTextBounds();
   
   pdvector<springboardChunk>::const_iterator iter = chunks.begin();
   pdvector<springboardChunk>::const_iterator finish = chunks.end();
   for (; iter != finish; ++iter) {
      const pair<uint32_t, uint32_t> one_chunk_result =
         iter->calcNumBytesInChunk(theNucleusTextBounds.first,
                                   theNucleusTextBounds.second);

      // TEMP for debugging:
      dout << "in nucleus: " << one_chunk_result.first
           << "; outside nucleus: " << one_chunk_result.second
           << endl;
      
      result.first += one_chunk_result.first;
      result.second += one_chunk_result.second;
   }
   
   return result;
}

void SpringBoardHeap::addChunk(kptr_t startAddr, unsigned len) {
   springboardChunk chunk(startAddr, len);
   chunks += chunk;

   chunksPresentlySorted = false;
}

struct chunk_finder {
   kptr_t startAddr;

   chunk_finder(kptr_t istartAddr) : startAddr(istartAddr) {
   }

   bool operator()(const springboardChunk &theChunk) {
      return theChunk.getStartAddr() == startAddr;
   }
};

void SpringBoardHeap::removeChunk(kptr_t startAddr) {
   // Find the springboard chunk
   pdvector<springboardChunk>::iterator iter = std::find_if(chunks.begin(), 
						          chunks.end(),
                                                          chunk_finder(startAddr));
   assert(iter != chunks.end() && "tried to remove a springboard chunk, but not found");
   
   *iter = chunks.back();
   chunks.pop_back();
}

void SpringBoardHeap::sortChunks() { // required before allocation
   std::sort(chunks.begin(), chunks.end());
   chunksPresentlySorted = true;
}

SpringBoardHeap::item
SpringBoardHeap::alloc(kptr_t lowestOKAddr, kptr_t highestOKAddr,
                       // The next 2 params help us avoid cache line conflicts:
                       kptr_t fromAddr, kptr_t toAddr)
      throw(CouldntFindOneFreeInRange) {
   assert(chunksPresentlySorted);
   
   // As a first step, eliminate all out-of-range springboardChunks from consideration

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;

   okItemBounds oib = {lowestOKAddr, highestOKAddr};
   typedef pair<pdvector<springboardChunk>::iterator,
                pdvector<springboardChunk>::iterator> PI;
   PI bounds = std::equal_range(chunks.begin(),
                                chunks.end(),
                                oib,
                                okItemBoundsComparitor_less());

   if (bounds.first == bounds.second) {
      // yikes; no chunk falls within that range, so there is no hope of reaching
      // *any* springboard.  Note that we're not saying that a chunk exists but is fully
      // allocated at the moment -- we're saying that a chunk doesn't even exist.
      cerr << "SpringBoard problem -- no chunk starts within the bounds "
           << addr2hex(lowestOKAddr) << " and " << addr2hex(highestOKAddr) << endl;
      throw CouldntFindOneFreeInRange();
   }

   // Now for some assertion checking to make sure that equal_bounds() worked right.

   // Assert that every sb chunk in the equalRange at least partially overlaps
   // the range [lowestOKAddr, highestOKAddr].
   
   for (pdvector<springboardChunk>::iterator iter = bounds.first;
        iter != bounds.second; ++iter) {
      springboardChunk &chunk = *iter;
      const bool usableChunk =
         chunk.hasASpringboardWhoseStartAddrIsInRangeOf(lowestOKAddr,
                                                        highestOKAddr);
      assert(usableChunk);
   }

   try {
      const unsigned num32ByteICacheLines = 8192 / 32; // assume 8K icache
      return alloc_part2(lowestOKAddr, highestOKAddr, bounds.first, bounds.second,
                         fromAddr, toAddr, num32ByteICacheLines, startTime);
   }
   catch (CouldntFindOneFreeInRange) {
      const unsigned num32ByteICacheLines = 16384 / 32; // assume 8K icache
      dout << "SpringBoardHeap alloc(): could not allocate with no icache conflicts, assuming 8K icache; trying assuming 16K now..." << endl;

      try {
         return alloc_part2(lowestOKAddr, highestOKAddr, bounds.first, bounds.second,
                            fromAddr, toAddr, num32ByteICacheLines, startTime);
      }
      catch (CouldntFindOneFreeInRange) {
         dout << "SpringBoardHeap alloc(): could not allocate with no icache conflicts, assuming 16K icache; allocating w/o regard to icache placement..." << endl;
         
         return alloc_part2(lowestOKAddr, highestOKAddr, bounds.first, bounds.second,
                            fromAddr, toAddr, -1U, startTime);
      }
   }
   
   // bounds.second will act as our end()
}

SpringBoardHeap::item
SpringBoardHeap::alloc_part2(kptr_t lowestOKAddr, kptr_t highestOKAddr,
                             pdvector<springboardChunk>::iterator low,
                             pdvector<springboardChunk>::iterator hi,
                             kptr_t fromAddr, kptr_t toAddr,
                             unsigned num32ByteICacheLines,
                             mrtime_t startTime)
                                           throw(CouldntFindOneFreeInRange) {
   // In part 2 of the search, we do a rather dumb linear search through the
   // chunks.  Unfortunately, we don't exclude chunks which are fully
   // allocated.  num32ByteICacheLines: if -1U then we pay no attention,
   // else we try to avoid conflicts from "fromAddr" and "toAddr" addresses.
   while (low != hi) {
      // search this chunk.
      unsigned ndx_within_chunk =
            low->allocSpringboardWithStartAddrWithinRangeOf(lowestOKAddr,
                                                            highestOKAddr,
                                                            fromAddr, toAddr,
                                                            num32ByteICacheLines);
      if (ndx_within_chunk != UINT_MAX) {
         // success
         item result = {low - chunks.begin(), // chunk ndx
                        ndx_within_chunk};

         if (verboseTimings) {
            const mrtime_t allocationTotalTime = getmrtime() - startTime;
            cout << "Allocated a springboard in " << allocationTotalTime / 1000
                 << " usecs." << endl;
         }

         return result;
      }

      low++;
   }

   throw CouldntFindOneFreeInRange();
}

void SpringBoardHeap::free(const item &i) {
   pdvector<springboardChunk>::iterator iter = chunks.begin() + i.chunk_ndx;
   iter->free(i.item_within_chunk_ndx);
}

kptr_t SpringBoardHeap::item2addr(const item &i) {
   pdvector<springboardChunk>::iterator iter = chunks.begin() + i.chunk_ndx;
   return iter->ndxToAddr(i.item_within_chunk_ndx);
}
