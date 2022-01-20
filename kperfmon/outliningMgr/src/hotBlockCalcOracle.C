// hotBlockCalcOracle.C

#include "hotBlockCalcOracle.h"
#include <algorithm> // STL's count()

//hotBlockCalcOracle::ManuallySetThreshold hotBlockCalcOracle::manuallySetThreshold;

hotBlockCalcOracle::bbid_t
hotBlockCalcOracle::getHottestOfTheColdBlocks() const {
   // dummyEntryBlockId and dummyExitBlockId are not considered.
   bbid_t result = (bbid_t)-1;
   unsigned the_count = 0;

   const unsigned numBBs = theFnBlockAndEdgeCounts.getNumRegularBlocks();
   for (bbid_t bbid = 0; bbid < numBBs; ++bbid) {
      if (!isBlockOutlined(bbid))
         continue;
      
      const unsigned thisBlockCount = theFnBlockAndEdgeCounts.getBlockCount(bbid);

      if (result == (bbid_t)-1 || thisBlockCount > the_count) {
         result = bbid;
         the_count = thisBlockCount;
      }
   }
      
   if (result != (bbid_t)-1)
      assert(isBlockOutlined(result));

   return result;
}
