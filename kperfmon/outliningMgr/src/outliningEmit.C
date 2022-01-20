// outliningEmit.C

#include "outliningEmit.h"
#include "codeObject.h"
#include "fnBlockAndEdgeCounts.h"
#include "hotBlockCalcOracle.h"

typedef function_t::bbid_t bbid_t;

static bbid_t
figureOutNextBlockIdInOrigCode(const function_t &fn,
                               const basicblock_t &bb) {
   const bbid_t nextBlockIdInOrigCode = fn.searchForBB(bb.getEndAddrPlus1());
      // NOT necessarily the same as the one that'll be emitted next here.
   if (nextBlockIdInOrigCode == (bbid_t)-1)
      return (bbid_t)-1;

   if (fn.getBasicBlockFromId(nextBlockIdInOrigCode)->getStartAddr() !=
       bb.getEndAddr() + 1)
      // Hmmm, perhaps there was a piece of data (jump table?) between the end
      // of the present bb and the next one in the original code.  Thus, set
      // nextBlockIdInOrigCode to none (-1U) so it doesn't try to be clever
      // with falling through.
      return (bbid_t)-1;
   
   return nextBlockIdInOrigCode;
}

void
emitAGroupOfBlocksInNewChunk(bool tryToKeepOriginalCode,
                             tempBufferEmitter &em,
                             outliningLocations &whereBlocksNowReside,
                                // not const on purpose
                             const function_t &fn,
                             const fnCodeObjects &theFnCodeObjects,
                             const pdvector<bbid_t> &blockIds) {
   em.beginNewChunk();
   
   pdvector<bbid_t>::const_iterator bbiter = blockIds.begin();
   pdvector<bbid_t>::const_iterator bbfinish = blockIds.end();
   for (; bbiter != bbfinish; ++bbiter) {
      const bbid_t bb_id = *bbiter;
      const basicblock_t *bb = fn.getBasicBlockFromId(bb_id);

      const basicBlockCodeObjects &theBlockCodeObjects =
         theFnCodeObjects.getCodeObjectsForBlockById(bb_id);

      // Write basic block labels/symAddr's to "em", and make a note of its (relative)
      // address (remember that we won't know, for quite a while, any exact insn
      // addrs, and that's a feature, not a bug...allowing the code to be relocated).
      whereBlocksNowReside.aBlockIsAboutToBeEmitted(em, bb_id);

      const pdvector< pair<unsigned, codeObject*> >::const_iterator co_start =
         theBlockCodeObjects.begin();
      const pdvector< pair<unsigned, codeObject*> >::const_iterator co_finish =
         theBlockCodeObjects.end();
      for (pdvector< pair<unsigned, codeObject*> >::const_iterator co_iter = co_start;
           co_iter != co_finish; ++co_iter) {
         const codeObject *co = co_iter->second;

         whereBlocksNowReside.aCodeObjectIsAboutToBeEmitted(em, bb_id,
                                                            co_iter - co_start);

         const bool lastCodeObjectInBasicBlock = (co_iter + 1 == co_finish);
         const bbid_t nextBlockIdInOrigCode = lastCodeObjectInBasicBlock ?
            figureOutNextBlockIdInOrigCode(fn, *bb) : (bbid_t)-1;
         
         co->emitWhenRelocatedTo(tryToKeepOriginalCode, // true iff testing
                                 fn, bb_id,
                                 em,
                                 whereBlocksNowReside,
                                 lastCodeObjectInBasicBlock,
                                 nextBlockIdInOrigCode);
      }
   }
}

static void
emit_common(unsigned groupUniqueId,
            tempBufferEmitter &em,
            const pdvector< pair<pdstring, const function_t*> > &allFuncsInGroup,
            bool justTestingNoMovement,
            const function_t &fn, const fnCodeObjects &theFnCodeObjects,
            const pdvector<bbid_t> &theInlinedBlockIds,
            const pdvector<bbid_t> &theOutlinedBlockIds) {
   // This routine will emit outlined code in 3 chunks:
   // pre-function data (such as jump tables), inlined (hot) blocks, and
   // outlined (cold) blocks.  Only blocks from the same function can be emitted
   // (this is not a limiting problem).

   assert(theInlinedBlockIds.size() + theOutlinedBlockIds.size() == fn.numBBs());
   
   // assert no duplicates
   pdvector<bbid_t> checkset = theInlinedBlockIds;
   checkset += theOutlinedBlockIds;
   std::sort(checkset.begin(), checkset.end());
   assert(std::adjacent_find(checkset.begin(), checkset.end()) == checkset.end());
   
   outliningLocations whereBlocksNowReside(groupUniqueId,
                                           outliningLocations::afterInlinedBlocks,
                                              // where outlined blocks reside
                                           allFuncsInGroup,
                                           justTestingNoMovement,
                                           theInlinedBlockIds,
                                           theOutlinedBlockIds);
   
   // Emit!
   assert(em.numChunks() == 1);
      // the pre-fn data chunk.  Presently empty, but we'll add to it later
      // on when and if jump table data is emitted (code object emit methods can
      // switch to this chunk, emit some data, and go back to the previous chunk).

   // Emit inlined blocks first
   emitAGroupOfBlocksInNewChunk(false, // don't try to keep original sequence;
                                       // we're not just testing
                                em,
                                whereBlocksNowReside,
                                fn, theFnCodeObjects, theInlinedBlockIds);
   assert(em.numChunks() == 2);

   // Emit outlined blocks last
   emitAGroupOfBlocksInNewChunk(false, // don't try to keep original sequence;
                                       // we're not just testing
                                em,
                                whereBlocksNowReside,
                                fn, theFnCodeObjects, theOutlinedBlockIds);
   assert(em.numChunks() == 3);

   // XXX -- to do -- if this fn falls thru to the next one, then we need to emit,
   // after what *used* to be the last block (it might have moved to the middle now,
   // for all we know), an unconditional jump to the fallee.

   em.complete();
}

static bool
chosenHotAndColdBlockOrderingsOK(const pdvector<bbid_t> &chosenHotBlocks,
                                 const pdvector<bbid_t> &chosenColdBlocks,
                                 const hotBlockCalcOracle &theOracle) {
   pdvector<bbid_t> allBlocks = chosenHotBlocks;
   allBlocks += chosenColdBlocks;
   std::sort(allBlocks.begin(), allBlocks.end());
   assert(allBlocks.size() == theOracle.getFn().numBBs() &&
          allBlocks.end() == std::adjacent_find(allBlocks.begin(), allBlocks.end()));
   
   for (pdvector<bbid_t>::const_iterator iter = chosenHotBlocks.begin();
        iter != chosenHotBlocks.end(); ++iter) {
      const bbid_t bbid = *iter;
      assert(theOracle.isBlockInlined(bbid) == true);
   }
   
   for (pdvector<bbid_t>::const_iterator iter = chosenColdBlocks.begin();
        iter != chosenColdBlocks.end(); ++iter) {
      const bbid_t bbid = *iter;
      assert(theOracle.isBlockInlined(bbid) == false);
   }

   return true; // OK
}

void
pickBlockOrderingAndEmit1Fn(pickBlockOrderingFnType pickBlockOrdering,
                            unsigned groupUniqueId,
                            tempBufferEmitter &em,
                            const pdvector< pair<pdstring, const function_t *> > &allFuncsInGroup,
                            const fnBlockAndEdgeCounts &rootFnBlockAndEdgeCounts,
                            const function_t &fn,
                            const fnCodeObjects &theFnCodeObjects,
                            const pdvector<unsigned> &bbCounts,
                            hotBlockCalcOracle::ThresholdChoices theHotBlockThresholdChoice
                            ) {
   // We emit in 3 chunks (the current one plus 2 added ones): pre fn data (like
   // jump table data), hot blocks ("inlined" code), and cold blocks ("outlined" code).
   // The entry basic block will always be emitted among the hot blocks, and furthermore
   // will always be the first block in that chunk.

   // STEP 1: pick a block ordering.
   hotBlockCalcOracle isBlockInlined(rootFnBlockAndEdgeCounts, fn, bbCounts,
                                     theHotBlockThresholdChoice); // fast ctor
   pdvector<bbid_t> theInlinedBlockIds;
   pdvector<bbid_t> theOutlinedBlockIds;

   // indirect function call through a pointer:
   pickBlockOrdering(isBlockInlined, fn,
                     theInlinedBlockIds,
                     theOutlinedBlockIds);

   assert(theInlinedBlockIds[0] == fn.xgetEntryBB() &&
          "entry bb must always be emitted first");

   // Some assertions: assert that theInlinedBlockIds + theOutlinedBlockIds add up
   // to all of the block ids
   assert(chosenHotAndColdBlockOrderingsOK(theInlinedBlockIds, theOutlinedBlockIds,
                                           isBlockInlined));

   // STEP 2: emit!
   emit_common(groupUniqueId,
               em, allFuncsInGroup,
               false, // not just testing
               fn, theFnCodeObjects,
               theInlinedBlockIds, theOutlinedBlockIds);

   assert(em.isCompleted());
}

