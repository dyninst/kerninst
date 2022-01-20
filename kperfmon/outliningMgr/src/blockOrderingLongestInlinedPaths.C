// blockOrderingLongestInlinedPaths.C

#include "blockOrderingLongestInlinedPaths.h"
#include <algorithm> // reverse()

typedef function_t::bbid_t bbid_t;

void
pickLongestPathFromBlock_excludingColdAndAlreadyEmittedBlocks
(bbid_t start_bb_id,
 pdvector<bool> &alreadyEmittedBlocks,
 const hotBlockCalcOracle &theBlockOracle,
 const function_t &fn,
 pdvector<bbid_t> &theInlinedBlocks) {

   // Dijkstra's algorithm
   pdvector<bool> finishedSet(fn.numBBs(), false); // nodes who we know the answer for

   // Initialize: finishedSet gets start_bb_id
   finishedSet[start_bb_id] = true;
   
   // Initialize working distance vector
   pdvector<unsigned> D(fn.numBBs(), 0);
      // initialize to minimum since we want longest paths
   const basicblock_t *entry_bb = fn.getBasicBlockFromId(start_bb_id);
   for (basicblock_t::ConstSuccIter siter = entry_bb->beginSuccIter();
        siter != entry_bb->endSuccIter(); ++siter) {
      bbid_t succ_bb_id = *siter;
      if (succ_bb_id == basicblock_t::UnanalyzableBBId ||
          succ_bb_id == basicblock_t::xExitBBId ||
          succ_bb_id == basicblock_t::interProcBranchExitBBId ||
          succ_bb_id == start_bb_id ||
          alreadyEmittedBlocks[succ_bb_id] ||
          !theBlockOracle.isBlockInlined(succ_bb_id))
         continue;

      D[succ_bb_id] = 1;
   }

   // Initialize the predSet vector: all entries to start_bb_id
   pdvector<bbid_t> predsSet(fn.numBBs(), (bbid_t)-1);
      // indexed by offset in the path, gives predecessor
   for (bbid_t bb_id = 0; bb_id < fn.numBBs(); ++bb_id) {
      if (alreadyEmittedBlocks[bb_id] || 
          !theBlockOracle.isBlockInlined(bb_id))
         continue;
      // even predsSet[start_bb_id] needs to be set, I believe
      predsSet[bb_id] = start_bb_id;
   }

   // Now the main loop.  Do it (numBlocks-1) times.  Num blocks is the number
   // of inlined blocks minus those already emitted.
   unsigned numBlocksLeftInGraph = 0;
   for (bbid_t bb_id=0; bb_id < fn.numBBs(); ++bb_id)
      if (theBlockOracle.isBlockInlined(bb_id) && !alreadyEmittedBlocks[bb_id])
         ++numBlocksLeftInGraph;

   bool graphMustHaveBeenDisconnected = false; // for now
   for (unsigned dummy=0; dummy < numBlocksLeftInGraph-1; ++dummy) {
      // among all the nodes except those in finishedSet, find the one w such that
      // D[w] is maximum.
      unsigned maximum_w_bbid = (bbid_t)-1;
      unsigned maximum_w_value = 0;
      for (bbid_t w=0; w < fn.numBBs(); ++w) {
         // Only consider real, non-finished nodes:
         if (alreadyEmittedBlocks[w] ||
             !theBlockOracle.isBlockInlined(w) ||
             finishedSet[w])
            continue;

         if (D[w] > maximum_w_value) {
            maximum_w_bbid = w;
            maximum_w_value = D[w];
         }
      }
      // If we didn't find any suitable "w" then it probably means that the
      // graph was disconnected, so that there *are* nodes yet to be processed, but
      // we can't reach them due to disconnectivity.
      if (maximum_w_bbid == (bbid_t)-1) {
         graphMustHaveBeenDisconnected = true;
         break;
      }

      // add maximum_w_bbid to finishedSet
      assert(!finishedSet[maximum_w_bbid]);
      finishedSet[maximum_w_bbid] = true;
      
      // each non-finished successor of this vertex: perhaps update its D[]
      // entry.
      const basicblock_t *w_bb = fn.getBasicBlockFromId(maximum_w_bbid);
      for (basicblock_t::ConstSuccIter siter = w_bb->beginSuccIter();
           siter != w_bb->endSuccIter(); ++siter) {
         const bbid_t succ_bb_id = *siter;

         if (succ_bb_id == basicblock_t::UnanalyzableBBId ||
             succ_bb_id == basicblock_t::xExitBBId ||
             succ_bb_id == basicblock_t::interProcBranchExitBBId ||
             alreadyEmittedBlocks[succ_bb_id] ||
             !theBlockOracle.isBlockInlined(succ_bb_id) ||
             finishedSet[succ_bb_id])
            continue;

         // See if going to maximum_w_bbid and then to "succ_bb_id" is longer
         // than the previously longest way to get to "succ_bb_id"
         // The "1" below represents the distance from w_bbid to succ_bb_id
         if (D[maximum_w_bbid] + 1 > D[succ_bb_id]) {
            D[succ_bb_id] = D[maximum_w_bbid] + 1;
            predsSet[succ_bb_id] = maximum_w_bbid;
         }
      }
   }

   // Done running the algorithm.  Pick the entry in D that is maximum.  We can't
   // really prefer any one "exit" bb because there are usually several.
   bbid_t chosen_exit_bbid = (bbid_t)-1;
   unsigned chosen_exit_bbdistance = 0;

   for (bbid_t bb_id=0; bb_id < fn.numBBs(); ++bb_id) {
      if (alreadyEmittedBlocks[bb_id] ||
          !theBlockOracle.isBlockInlined(bb_id))
         continue;
      
      if (graphMustHaveBeenDisconnected && !finishedSet[bb_id])
         // we allow !finishedSet[bb_id] only if graph was disconnected
         continue;

      assert(finishedSet[bb_id]);
      assert(predsSet[bb_id] != (bbid_t)-1);
      if (D[bb_id] > chosen_exit_bbdistance) {
         chosen_exit_bbid = bb_id;
         chosen_exit_bbdistance = D[bb_id];
      }
   }
   if (chosen_exit_bbid == (bbid_t)-1) {
      // Perhaps only entry_bb_id could be emitted.  Assert as such, and then
      // allow it.
      for (bbid_t bbid=0; bbid < fn.numBBs(); ++bbid)
         if (bbid != start_bb_id)
            assert(!finishedSet[bbid]);
      // OK, fine
      chosen_exit_bbid = start_bb_id;
   }
   
   assert(chosen_exit_bbid != (bbid_t)-1);
   // OK now we've got chosen_exit_bbid.  Following preds will lead back to start_bb_id
   pdvector<bbid_t> theChosenPathOfBlockIds;
   bbid_t curr_bb_id = chosen_exit_bbid;

   theChosenPathOfBlockIds += curr_bb_id;
   while (curr_bb_id != start_bb_id) {
      assert(curr_bb_id != (bbid_t)-1);
      const bbid_t next_bbid = predsSet[curr_bb_id];
      assert(next_bbid != (bbid_t)-1);

      curr_bb_id = next_bbid;
      theChosenPathOfBlockIds += curr_bb_id;
   }

   assert(theChosenPathOfBlockIds.size() > 0);
   std::reverse(theChosenPathOfBlockIds.begin(),
                theChosenPathOfBlockIds.end());
   assert(theChosenPathOfBlockIds[0] == start_bb_id);
   for (unsigned lcv=0; lcv < theChosenPathOfBlockIds.size(); ++lcv) {
      const bbid_t bbid = theChosenPathOfBlockIds[lcv];
      assert(!alreadyEmittedBlocks[bbid] && theBlockOracle.isBlockInlined(bbid));
      if (lcv < theChosenPathOfBlockIds.size()-1)
         assert(fn.getBasicBlockFromId(bbid)->hasAsASuccessor(theChosenPathOfBlockIds[lcv+1]));
      theInlinedBlocks += bbid;
      alreadyEmittedBlocks[bbid] = true;
   }

   // finally all done
}

void pickBlockOrdering_longestInlinedPaths(const hotBlockCalcOracle &theBlockOracle,
                                           const function_t &fn,
                                           pdvector<bbid_t> &theInlinedBlocks,
                                           pdvector<bbid_t> &theOutlinedBlocks) {
   theInlinedBlocks.clear();
   theOutlinedBlocks.clear();
   
   // Use Dijkstra's longest-path algorithm on the control flow graph.  Be sure to
   // exclude the cold blocks, however.

   pdvector<bool> alreadyEmittedBlocks(fn.numBBs(), false);

   // start with the entry block
   const bbid_t entry_bb_id = fn.xgetEntryBB();
   pickLongestPathFromBlock_excludingColdAndAlreadyEmittedBlocks(entry_bb_id,
                                                                 alreadyEmittedBlocks,
                                                                 theBlockOracle, fn,
                                                                 theInlinedBlocks);
   // For any remaining hot blocks, re-run longest path algorithm.  Need to pick
   // a block to start with.
   for (bbid_t bbid=0; bbid < fn.numBBs(); ++bbid) {
      if (alreadyEmittedBlocks[bbid] ||
          !theBlockOracle.isBlockInlined(bbid))
         continue;
      
      // OK this guy is a hot block that hasn't already been emitted.
      pickLongestPathFromBlock_excludingColdAndAlreadyEmittedBlocks(bbid,
                                                                    alreadyEmittedBlocks,
                                                                    theBlockOracle,
                                                                    fn,
                                                                    theInlinedBlocks);
   }

   // assert that all blocks that are hot have been emitted, and that no cold blocks
   // have been emitted:
   for (bbid_t bbid=0; bbid < alreadyEmittedBlocks.size(); ++bbid) {
      if (theBlockOracle.isBlockInlined(bbid))
         assert(alreadyEmittedBlocks[bbid]);
      else
         assert(!alreadyEmittedBlocks[bbid]);
   }
   // a similar assert, but looping thru theInlinedBlocks
   for (unsigned ndx=0; ndx < theInlinedBlocks.size(); ++ndx) {
      const bbid_t bbid = theInlinedBlocks[ndx];
      assert(alreadyEmittedBlocks[bbid]);
      assert(theBlockOracle.isBlockInlined(bbid));
   }
   assert(theOutlinedBlocks.size() == 0);

   // Assert that there are no duplicate inlined blocks
   pdvector<bbid_t> copy_inlinedBlocks = theInlinedBlocks;
   std::sort(copy_inlinedBlocks.begin(), copy_inlinedBlocks.end());
   assert(std::adjacent_find(copy_inlinedBlocks.begin(),
                             copy_inlinedBlocks.end()) == copy_inlinedBlocks.end());
   
   // OK, all done emitting inlined blocks.  Now emit the cold blocks, 
   // in the original ordering.
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

