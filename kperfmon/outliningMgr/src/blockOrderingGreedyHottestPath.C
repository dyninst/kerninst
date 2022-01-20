// blockOrderingGreedyHottestPath.C

#include "blockOrderingGreedyHottestPath.h"
#include <algorithm>

typedef function_t::bbid_t bbid_t;

void doGreedyHottestPathFrom(bbid_t start_bb_id,
                             pdvector<bool> &alreadyEmittedBlocks,
                             const hotBlockCalcOracle &theBlockOracle,
                             const function_t &fn,
                             pdvector<bbid_t> &theInlinedBlocks) {
   assert(!alreadyEmittedBlocks[start_bb_id]);
   assert(theBlockOracle.isBlockInlined(start_bb_id));
   
   theInlinedBlocks += start_bb_id;
   alreadyEmittedBlocks[start_bb_id] = true;
   
   // For all not-yet-emitted to-be-inlined successors of start_bb_id, find the
   // hottest
   const basicblock_t *bb = fn.getBasicBlockFromId(start_bb_id);
   bbid_t hottest_child_bbid = (bbid_t)-1;
   for (basicblock_t::ConstSuccIter siter = bb->beginSuccIter();
        siter != bb->endSuccIter(); ++siter) {
      const bbid_t succ_bb_id = *siter;
      if (succ_bb_id != basicblock_t::UnanalyzableBBId &&
          succ_bb_id != basicblock_t::xExitBBId &&
          succ_bb_id != basicblock_t::interProcBranchExitBBId &&
          !alreadyEmittedBlocks[succ_bb_id] &&
          theBlockOracle.isBlockInlined(succ_bb_id) &&
          (hottest_child_bbid == (bbid_t)-1 ||
           theBlockOracle.isBlockAHotterThanBlockB(succ_bb_id, hottest_child_bbid)))
         hottest_child_bbid = succ_bb_id;
   }

   // Did we find one?
   if (hottest_child_bbid == (bbid_t)-1)
      // can't emit anything more than start_bb_id; too bad.  Hopefully it was exit.
      return;
   
   assert(!alreadyEmittedBlocks[hottest_child_bbid]);
   assert(theBlockOracle.isBlockInlined(hottest_child_bbid));

   // Recurse on this block
   doGreedyHottestPathFrom(hottest_child_bbid, alreadyEmittedBlocks,
                           theBlockOracle, fn, theInlinedBlocks);
}
                           
// ------------------------------------------------------------

void 
pickBlockOrdering_greedyHottestPaths(const hotBlockCalcOracle &theBlockOracle,
                                     const function_t &fn,
                                     pdvector<bbid_t> &theInlinedBlocks,
                                     pdvector<bbid_t> &theOutlinedBlocks) {
   // a static method

   // beginning with entry block, pick not-yet-emitted successor
   // that is hot enough to be inlined and has the hottest block count.

   pdvector<bool> alreadyEmittedBlocks(fn.numBBs(), false);
   
   // start with entry_bb_id
   const bbid_t entry_bb_id = fn.xgetEntryBB();
   doGreedyHottestPathFrom(entry_bb_id,
                           alreadyEmittedBlocks,
                           theBlockOracle,
                           fn, theInlinedBlocks);
   // If any inlined blocks still haven't been emitted, do them now
   for (bbid_t bb_id=0; bb_id < fn.numBBs(); ++bb_id) {
      if (alreadyEmittedBlocks[bb_id] ||
          !theBlockOracle.isBlockInlined(bb_id))
         continue;
      
      doGreedyHottestPathFrom(bb_id,
                              alreadyEmittedBlocks,
                              theBlockOracle,
                              fn, theInlinedBlocks);
   }

   // Should be all done emitting inlined blocks, without having emitted any
   // outlined blocks
   for (bbid_t bbid=0; bbid < alreadyEmittedBlocks.size(); ++bbid) {
      if (theBlockOracle.isBlockInlined(bbid))
         assert(alreadyEmittedBlocks[bbid]);
      else
         assert(!alreadyEmittedBlocks[bbid]);
   }
   // assert no duplicate blocks in theInlinedBlocks:
   pdvector<bbid_t> copy_inlinedBlocks = theInlinedBlocks;
   std::sort(copy_inlinedBlocks.begin(), copy_inlinedBlocks.end());
   assert(std::adjacent_find(copy_inlinedBlocks.begin(), copy_inlinedBlocks.end()) ==
          copy_inlinedBlocks.end());

   // Now do the outlined blocks, in orig order:
   const pdvector<kptr_t> allBBAddrsSorted = fn.getAllBasicBlockStartAddressesSorted();
   pdvector<kptr_t>::const_iterator iter = allBBAddrsSorted.begin();
   pdvector<kptr_t>::const_iterator finish = allBBAddrsSorted.end();
   for (; iter != finish; ++iter) {
      const kptr_t bbStartAddr = *iter;
      
      // now get a bb_id to go along with this bbStartAddr:
      const bbid_t bb_id = fn.searchForBB(bbStartAddr);
      const basicblock_t *bb = fn.getBasicBlockFromId(bb_id);
      assert(bb->getStartAddr() == bbStartAddr && "fn.getAllBasicBlockStartAddressesSorted() returned an item that doesn't start off a basic block?");

      if (!theBlockOracle.isBlockInlined(bb_id)) {
         assert(!alreadyEmittedBlocks[bb_id]);
         theOutlinedBlocks += bb_id;
         alreadyEmittedBlocks[bb_id] = true;
      }
   }

   assert(theInlinedBlocks.size() + theOutlinedBlocks.size() == fn.numBBs());
   pdvector<bbid_t> checkvec = theInlinedBlocks;
   checkvec += theOutlinedBlocks;
   std::sort(checkvec.begin(), checkvec.end());
   assert(std::adjacent_find(checkvec.begin(), checkvec.end()) == checkvec.end());
}

