// hotBlockCalcOracle.h

#ifndef _HOT_BLOCK_CALC_ORACLE_H_
#define _HOT_BLOCK_CALC_ORACLE_H_

#include "common/h/Vector.h"
#include "funkshun.h"
#include "sparcTraits.h"
#include "fnBlockAndEdgeCounts.h"

class hotBlockCalcOracle {
 private:
   typedef function_t::bbid_t bbid_t;

   const function_t &fn;
      // no worries about a reference; addresses of function_t objects stay solid
   fnBlockAndEdgeCounts theFnBlockAndEdgeCounts;

   unsigned coldBlockThreshold;
      // A block count has to be > than, not >= than, this threshold to be considered
      // hot.  This is a feature, ensuring that a block count of zero is ALWAYS
      // considered cold, even if coldBlockThreshold is zero.

 public:
   enum ThresholdChoices {AnyNonZeroBlock, FivePercent};

//     class ManuallySetThreshold {};
//     static ManuallySetThreshold manuallySetThreshold;

   hotBlockCalcOracle(const fnBlockAndEdgeCounts &rootFnBlockAndEdgeCounts,
                         // used just to calculate "coldBlockThreshold", not stored.
                      const function_t &ifn,
                      const pdvector<unsigned> &ibbCounts,
                      ThresholdChoices theThresholdChoice) :
      fn(ifn), theFnBlockAndEdgeCounts(fn, ibbCounts)
   {
      // Use "rootFnBlockAndEdgeCounts" to calculate "coldBlockThreshold" (5%):
      const unsigned numTimesRootFnIsCalled =
         rootFnBlockAndEdgeCounts.getNumTimesFnWasCalled();
      if (rootFnBlockAndEdgeCounts.hadToFudgeEntryBlockCount())
         cout << "hotBlockCalcOracle WARNING: root fn does not have a reliably calculated #entries; recommend you use a DIFFERENT root fn!!!" << endl;

      switch (theThresholdChoice) {
         case AnyNonZeroBlock:
            coldBlockThreshold = 0;
            break;
         case FivePercent:
            coldBlockThreshold = numTimesRootFnIsCalled / 20; // that's 5%
            break;
         default:
            assert(0);
      }

      assert(theFnBlockAndEdgeCounts.getNumRegularBlocks() == fn.numBBs());
   }

   hotBlockCalcOracle(const function_t &ifn,
                      const pdvector<unsigned> &ibbCounts,
                      ThresholdChoices theThresholdChoice // MUST be AnyNonZeroBlock
                      ) :
      fn(ifn), theFnBlockAndEdgeCounts(fn, ibbCounts)
   {
      assert(theFnBlockAndEdgeCounts.getNumRegularBlocks() == fn.numBBs());

      assert(theThresholdChoice == AnyNonZeroBlock);
      coldBlockThreshold = 0;

      if (theFnBlockAndEdgeCounts.hadToFudgeEntryBlockCount()) {
         cout << "hotBlockCalcOracle WARNING: #entries is not reliablely calculated; don't use this fn as the root fn of an outlined group!" << endl;
      }
   }

   const function_t &getFn() const { return fn; }

   bool isBlockInlined(bbid_t bbid) const {
      return (bbid == fn.xgetEntryBB() || isBlockTrulyHot(bbid));
   }
   bool isBlockOutlined(bbid_t bbid) const { return !isBlockInlined(bbid); }

   bool isBlockTrulyHot(bbid_t bbid) const {
      // like the above isBlockInlined(), but doesn't "spuriously" always return true
      // for the entry basic block.  A more meritorious version.
      return theFnBlockAndEdgeCounts.getBlockCount(bbid) > coldBlockThreshold;
   }

   bool isBlockAHotterThanBlockB(bbid_t A, bbid_t B) const {
      return theFnBlockAndEdgeCounts.getBlockCount(A) >
             theFnBlockAndEdgeCounts.getBlockCount(B);
   }

   unsigned getEdgeWeight(bbid_t from_bbid, bbid_t to_bbid) const {
      return theFnBlockAndEdgeCounts.getEdgeWeight(from_bbid, to_bbid);
   }
   
//     unsigned getEdgeWeight(bbid_t /*from*/, bbid_t to) const {
//        return bbCounts[to];
//           // GAK!  Obviously incorrect, but since we only have block counts, not
//           // edge counts, c'est la vie.
//     }

   unsigned getNumFudgedEdges() const {
      return theFnBlockAndEdgeCounts.getNumFudgedEdges();
   }
   unsigned getTotalNumEdges() const {
      return theFnBlockAndEdgeCounts.getTotalNumEdges();
   }

   bbid_t getHottestOfTheColdBlocks() const;
};

#endif
