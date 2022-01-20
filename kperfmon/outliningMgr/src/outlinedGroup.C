// outlinedGroup.C

#include "outlinedGroup.h"
#include "instrumenter.h"
#include "tempBufferEmitter.h"
#include "hotBlockCalcOracle.h"
#include "moduleMgr.h"
#include "util/h/minmax.h"
#include "util/h/rope-utils.h"
#include "util/h/mrtime.h"
#include "outliningMisc.h"
#include "codeObject.h"
#include "fnCodeObjects.h"
#include "outliningLocations.h"
#include "blockOrderingOrigSeq.h"
#include "blockOrderingLongestInlinedPaths.h"
#include "blockOrderingGreedyHottestPath.h"
#include "blockOrderingChains.h"
#include "funcsToIncludeInGroupOracle.h"
#include "outliningTest.h"
#include "outliningEmit.h"
#include "groupMgr.h"
#include "machineInfo.h"

   // pickBlockOrderingAndEmit1Fn(), emitAGroupOfBlocksInNewChunk()

// static member vrble initialization:
unsigned outlinedGroup::nextUnusedGroupId = 0;

void outlinedGroup::
updateICacheStatsFor1Fn(usparc_icache &icstats,
                        const function_t &fn, const pdvector<bbid_t> &hotBlockIds) {
   // a static method

   pdvector<bbid_t>::const_iterator iter = hotBlockIds.begin();
   pdvector<bbid_t>::const_iterator finish = hotBlockIds.end();
   for (; iter != finish; ++iter) {
      const bbid_t bbid = *iter;
      
      const basicblock_t *bb = fn.getBasicBlockFromId(bbid);
      const kptr_t startAddrOfLastBBInsn = bb->getEndAddrPlus1() - 4;
      assert(startAddrOfLastBBInsn >= bb->getStartAddr());
      
      const instr_t *i = fn.get1OrigInsn(startAddrOfLastBBInsn);
      sparc_instr::sparc_cfi cfi;
      i->getControlFlowInfo(&cfi);
      if (cfi.fields.controlFlowInstruction)
         // This basic block ends on a control flow instruction; the next
         // instruction is always fetched and therefore must be considered
         // w.r.t. icache footprint.  Yes, even the "ba,a" instruction, which will
         // always squash the delay insn, still fetches that instruction.
         // (Kind of a shame, don't you think?)
         icstats.placeBasicBlock(bb->getStartAddr(), bb->getNumInsnBytes()+4);
      else
         icstats.placeBasicBlock(bb->getStartAddr(), bb->getNumInsnBytes());
   }
}

outlinedGroup::outlinedGroup(
    bool idoTheReplaceFunction,
    bool ireplaceFunctionPatchUpCallSitesToo,
    blockPlacementPrefs iBlockPlacementPrefs,
    hotBlockCalcOracle::ThresholdChoices iHotBlockThresholdPrefs,
    const pdstring &irootFnModName,
    const function_t &irootFn,
    const pdvector<kptr_t> iForceIncludeDescendants,
    const dictionary_hash< kptr_t, pdvector<unsigned> > &iFnBlockCounters,
    groupMgr &iGroupMgr,
    kerninstdClientUser *iconnection_to_kerninstd) :
   doTheReplaceFunction(idoTheReplaceFunction),
   replaceFunctionPatchUpCallSitesToo(ireplaceFunctionPatchUpCallSitesToo),
   blockOrderingFunc(NULL), // for now
   theHotBlockThresholdChoice(iHotBlockThresholdPrefs),
   groupUniqueId(nextUnusedGroupId++),
   rootFnModName(irootFnModName),
   rootFn(irootFn),
   rootFnGroupId(outliningMisc::parseOutlinedLevel(rootFnModName, rootFn)),
   rootFnBlockAndEdgeCounts(NULL),
   forceIncludeDescendants(iForceIncludeDescendants),
   theGroupMgr(iGroupMgr),
   fnBlockCounters(iFnBlockCounters),
   codeObjectParseRequestsNotYetSent(addrHash4),
   pendingCodeObjectParseRequests(unsignedIdentityHash),
   parsedFnCodeObjects(addrHash4),
   numNonNullEntriesInParsedFnCodeObjects(0),
   connection_to_kerninstd(iconnection_to_kerninstd)
{
   downloadedToKernelReqId = -1U; // undefined
   outlinedCodeEntryAddr = 0;

   // set blockOrderingFunc:
   changeOutliningBlockPlacementTo(iBlockPlacementPrefs);
   assert(blockOrderingFunc != NULL);

   // Calculate root fn edge counts
   const pdvector<unsigned> &rootFnBBCounts = 
       fnBlockCounters.get(rootFn.getEntryAddr());
   rootFnBlockAndEdgeCounts = new fnBlockAndEdgeCounts(rootFn, rootFnBBCounts);
   assert(rootFnBlockAndEdgeCounts);
}

outlinedGroup::~outlinedGroup() {
   assert(downloadedToKernelReqId == -1U);
   assert(outlinedCodeEntryAddr == 0);
   assert(replaceFnReqIds.size() == 0);
}

void outlinedGroup::kperfmonIsGoingDownNow() {
   unOutline(); // works no matter what stage we're in
}

// --------------------

void outlinedGroup::changeDoTheReplaceFunctionFlag(bool flag) {
   doTheReplaceFunction = flag;
}

void outlinedGroup::changeOutliningReplaceFunctionPatchUpCallSitesTooFlag(bool flag) {
   replaceFunctionPatchUpCallSitesToo = flag;
}

void outlinedGroup::changeOutliningBlockPlacementTo(blockPlacementPrefs p) {
   switch (p) {
      case origSeq:
         blockOrderingFunc = pickBlockOrderingOrigSeq;
         break;
      case chains:
         blockOrderingFunc = pickBlockOrderingChains;
         break;
      default:
         blockOrderingFunc = NULL;
         assert(false);
   }
}

void outlinedGroup::asyncSend1CodeObjectParseRequest() {
   // pick an entry from codeObjectParseRequestsNotYetSent[] and send it.
   // Can assert non-empty codeObjectParseRequestsNotYetSent[].

   assert(codeObjectParseRequestsNotYetSent.size() > 0);
   dictionary_hash<kptr_t, bool>::const_iterator parseReqIter =
      codeObjectParseRequestsNotYetSent.begin();
   assert(parseReqIter != codeObjectParseRequestsNotYetSent.end());
   const kptr_t fnStartAddr = parseReqIter.currkey();

   (void)codeObjectParseRequestsNotYetSent.get_and_remove(fnStartAddr);
      // asserts found

   // fnsToQueryCodeObjectsFor[]: a vector, one entry per fn being inlined/outlined.
   // each entry is a pair:
   // .first: fn's *original* entryAddr
   // .second: fnName, *not* its primary name or any such thing, but rather the name
   //          that shall be assigned to this function when we call the igen routine
   //          "downloadToKernelAndParse()", which requires a name.  TODO: isn't the
   //          name some combination of module and function?  (And reqid?)
   pdvector<fnAddrBeingRelocated> fnsToQueryCodeObjectsFor;
      // see .I file for "fnAddrBeingRelocated"
   fnsToQueryCodeObjectsFor.reserve_exact(fnBlockCounters.size());

   assert(rootFnBlockAndEdgeCounts != NULL);
   const funcsToIncludeInGroupOracle theFuncsToIncludeInGroupOracle(*this, *rootFnBlockAndEdgeCounts);

   dictionary_hash<kptr_t, pdvector<unsigned> >::const_iterator blkCountIter =
      fnBlockCounters.begin();
   dictionary_hash<kptr_t, pdvector<unsigned> >::const_iterator blkCountFinish =
      fnBlockCounters.end();
   for (; blkCountIter != blkCountFinish; ++blkCountIter) {
      const kptr_t fnAddr = blkCountIter.currkey();

      extern moduleMgr *global_moduleMgr;
      pair<pdstring, const function_t*> modAndFn =
         global_moduleMgr->findModAndFn(fnAddr, true); // true --> startOnly

      if (theFuncsToIncludeInGroupOracle.worthQueryingCodeObjectsForFn(modAndFn.first, *modAndFn.second)) {
         const pdstring fnNameForRelocPurposes =
            outliningMisc::getOutlinedFnModName(groupUniqueId) + "/" +
            outliningMisc::getOutlinedFnName(modAndFn.first, *modAndFn.second);

         fnsToQueryCodeObjectsFor += make_pair(fnAddr, fnNameForRelocPurposes);
      }
   }

   const unsigned reqid =
       theGroupMgr.reqParseFnIntoCodeObjects(*this, fnStartAddr,
					     fnsToQueryCodeObjectsFor);
      // We go through the group mgr for one reason only: because it manages
      // globally unique reqid's, no matter how many outlined groups there are.
      // note: fnsToQueryCodeObjectsFor should have fn names of the format
      // "modName/fnName".  And it does; see above.

   pendingCodeObjectParseRequests.set(reqid, fnStartAddr);
   parsedFnCodeObjects.set(fnStartAddr, NULL);
      // of course, we do *not* increment numNonNullEntriesInParsedFnCodeObjects
      // to correspond.
}

// ----------------------------------------------------------------------

void outlinedGroup::handle1FnCodeObjects(unsigned reqid, const fnCodeObjects &fco) {
   // STEP 1: simply store the codeObjects that have been passed in
   // NOTE: kerninstd could have had trouble parsing the code objects.
   // Consider unix/blkclr for a current example

   kptr_t fnEntryAddr = 0;
   if (!pendingCodeObjectParseRequests.find_and_remove(reqid, fnEntryAddr)) {
      cout << "received code objects for an unknown parse request; ignoring"
           << endl;
      return;
   }
   assert(fnEntryAddr != 0);

   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   pair<pdstring, const function_t*> modAndFn =
      theModuleMgr.findModAndFn(fnEntryAddr, true); // true --> startOnly

   const pdstring &modName = modAndFn.first;
   const function_t *fn = modAndFn.second;
   assert(fn);

   // Surely block counting information is still around for this fn:
   fnBlockCounters.get(fnEntryAddr); // will barf if not found

   if (fco.isUnparsed())
      cout << "Kerninstd could not parse code objects for fn ";
   else
      cout << "Got code objects for fn ";
   cout << addr2hex(fnEntryAddr)
        << " (" << modName << "/" << fn->getPrimaryName().c_str() << ")" << endl;
   
   // Sentinel should have been placed; we're going to replace the NULL with a real
   // entry momentarily.
   if (parsedFnCodeObjects.get_and_remove(fnEntryAddr) != NULL)
      assert(false);
   parsedFnCodeObjects.set(fnEntryAddr, new fnCodeObjects(fco));
      // reminder: fco could be unParsed(); it's OK.

   ++numNonNullEntriesInParsedFnCodeObjects;
   assert(numNonNullEntriesInParsedFnCodeObjects <= parsedFnCodeObjects.size());
   assert(numNonNullEntriesInParsedFnCodeObjects <= fnBlockCounters.size());

   // Request the next code object, if needed
   if (codeObjectParseRequestsNotYetSent.size() > 0) {
      asyncSend1CodeObjectParseRequest();
      return;
   }

   // Otherwise, there are 2 possibilities: (1) all done, or (2) not yet all done
   // (we've certainly sent all requests but we haven't necessarily received all
   // of the code objects)

   assert(parsedFnCodeObjects.size() == fnBlockCounters.size());
      // equal due to the NULL sentinels that are in place for those fns for which we
      // haven't yet received codeObjects.

   const bool readyToMoveToNextStage =
      numNonNullEntriesInParsedFnCodeObjects == fnBlockCounters.size();

   if (!readyToMoveToNextStage) {
      // We have faith that more code object responses are on the way soon...
      // until then, all we can do is go back to the main loop and wait for them.
      return;
   }

   // Ready to move onto the next stage, which is actual outlining!
   cout << "That was the last remaining code object...doing outlining now..."
        << endl;
   
   doTheActualOutlining();
}

// --------------------

class hasSameClassicFnAs {
 private:
   kptr_t theClassicFnEntryAddr;
   
 public:
   hasSameClassicFnAs(const pair<pdstring, const function_t *> &iModAndFn) :
      theClassicFnEntryAddr(outliningMisc::getOutlinedFnOriginalFn(iModAndFn.first, *iModAndFn.second)) {
   }
   
   bool operator()(const pair<pdstring, const function_t *> &other) const {
      return outliningMisc::getOutlinedFnOriginalFn(other.first, *other.second) ==
         theClassicFnEntryAddr;
   }
};

void outlinedGroup::doTheActualOutlining() {
   // All needed info (block counts & code objects, for perhaps several functions)
   // is present, we're ready to rock and roll.
   //
   // Kerninstd may not have been able to parse a given function's code objects;
   // that's one of the things checked for within class funcsToIncludeInGroupOracle;
   // such a function won't be included in the outlined group.

   assert(parsedFnCodeObjects.size() > 0);
   assert(parsedFnCodeObjects.size() == numNonNullEntriesInParsedFnCodeObjects);

   assert(allFuncsInGroup.size() == 0);
   allFuncsInGroup.reserve_exact(fnBlockCounters.size());

   rootFnNdx = 0;
   bool rootFnNdxHasBeenSet = false;

   usparc_icache theICacheStats;
   
   assert(rootFnBlockAndEdgeCounts != NULL);
   funcsToIncludeInGroupOracle isFunctionInGroupOracle(*this,
                                                       *rootFnBlockAndEdgeCounts);
      // fast ctor: stores a reference to root fn block/edge counts structure

   dictionary_hash<kptr_t, pdvector<unsigned> >::const_iterator iter =
      fnBlockCounters.begin();
   dictionary_hash<kptr_t, pdvector<unsigned> >::const_iterator finish =
      fnBlockCounters.end();
   for (; iter != finish; ++iter) {
      kptr_t fnAddr = iter.currkey();
      extern moduleMgr *global_moduleMgr;
      pair<pdstring, const function_t*> modAndFn =
	  global_moduleMgr->findModAndFn(fnAddr, true); // true --> startOnly

      const pdstring &modName = modAndFn.first;
      const function_t &fn = *modAndFn.second;
      // Tough assert; prevents divergences when re-outlining:
      assert(outliningMisc::parseOutlinedLevel(modName, fn) == rootFnGroupId);

      // The below test can be false under two circumstances: (a) cold function,
      // or (b) kerninstd couldn't parse that function's code objects (see the
      // isUnparsed() method of class fnCodeObjects).
      if (isFunctionInGroupOracle.willFnBeInOptimizedGroup(modName, fn)) {
         cout << "Including in the group: "
              << '\"' << modName << '/' << fn.getPrimaryName().c_str() << '\"'
              << endl;

         // Sorry "duplicates" (at least in the sense of having the same classic
         // function) are not yet supported
         assert(allFuncsInGroup.end() ==
                find_if(allFuncsInGroup.begin(), allFuncsInGroup.end(),
                        hasSameClassicFnAs(make_pair(modName, &fn))));
         
         allFuncsInGroup += make_pair(modName, &fn);

         if (&fn == &rootFn) {
            assert(!rootFnNdxHasBeenSet);
            rootFnNdxHasBeenSet = true;
            cout << " (group's root function)";
         }
         else if (!rootFnNdxHasBeenSet)
            ++rootFnNdx;

         // Print a performance warning if any of the hot blocks of this function
         // contain a call thru a function pointer, since the destination will not
         // have had a chance to be outlined.
         pdvector< pair<kptr_t, kptr_t> > ignore_regularCalls;
         pdvector< pair<kptr_t, kptr_t> > ignore_interProcBranches;
         pdvector<kptr_t> unAnalyzableCallsThruRegister;

         pdvector<bbid_t> fnHotBlocks =
            isFunctionInGroupOracle.getInlinedBlocksOfGroupFn(modName, fn);
            
         fn.getCallsMadeBy_WithSuppliedCodeAndBlocks
                                (fn.getOrigCode(),
                                 fnHotBlocks,
                                 ignore_regularCalls,
                                 false,
                                 ignore_interProcBranches,
                                 true,
                                 unAnalyzableCallsThruRegister);

         if (unAnalyzableCallsThruRegister.size()) {
            cout << endl << "PERFORMANCE WARNING: The following 'hot' instruction address(es) have an unanalyzable call:" << endl;
            pdvector<kptr_t>::const_iterator iter = unAnalyzableCallsThruRegister.begin();
            pdvector<kptr_t>::const_iterator finish = unAnalyzableCallsThruRegister.end();
            for (; iter != finish; ++iter) {
               const kptr_t insnAddr = *iter;
               cout << addr2hex(insnAddr) << ' ';
            }
            cout << endl;
         }

         updateICacheStatsFor1Fn(theICacheStats, fn, fnHotBlocks);
      }
      else {
         cout << "NOT including in the group: "
              << '\"' << modName << '/' << fn.getPrimaryName().c_str()
              << "\"" << endl;
         cout << "   (no hot blocks, or fn's code objects unparsable)"
              << endl;
      }
   }
   assert(rootFnNdxHasBeenSet && allFuncsInGroup.size() > 0);
   assert(allFuncsInGroup[rootFnNdx].second == &rootFn);
   assert(allFuncsInGroup.size() <= fnBlockCounters.size());
      // <= because we might have decided to ignore some functions

   // Print out i-cache stats
   cout << "I-Cache usage of hot blocks, by cache block (2 is OK)" << endl;
   const pdvector<unsigned> cacheBlockCounts = theICacheStats.getHowCrowdedStats();
   unsigned totalNumCacheBlocksCoveredByBasicBlocks = 0;
   pdvector<unsigned>::const_iterator cache_block_iter = cacheBlockCounts.begin();
   pdvector<unsigned>::const_iterator cache_block_finish = cacheBlockCounts.end();
   for (; cache_block_iter != cache_block_finish; ++cache_block_iter) {
      const unsigned numBasicBlocksResidingInThisCacheBlock = *cache_block_iter;
      if ((cache_block_iter - cacheBlockCounts.begin()) % 16 != 0)
         cout << '\t';

      cout << numBasicBlocksResidingInThisCacheBlock;

      if ((cache_block_iter + 1 - cacheBlockCounts.begin()) % 16 == 0)
         cout << endl;
      
      totalNumCacheBlocksCoveredByBasicBlocks += numBasicBlocksResidingInThisCacheBlock;
   }
   cout << endl;
   cout << "Total # of cache blocks: " << totalNumCacheBlocksCoveredByBasicBlocks
        << endl;
   cout << "which is ";
   // Total # of blocks in the I-cache is twice that of cacheBlockCounts[],
   // assuming 2-way associativity
   cout << 100.0 * (double)totalNumCacheBlocksCoveredByBasicBlocks / ((double)cacheBlockCounts.size()*2) << "% of the L1 I-Cache size" << endl;

   // outlining each fn individually, and "emitting" it to unchanged location,
   // should result in unchanged code:
   {
      // First, fill in "knownDownloadedCodeAddrsDict[]"
      dictionary_hash<pdstring, kptr_t> knownDownloadedCodeAddrsDict(stringHash);
      
      pdvector< pair<pdstring, const function_t *> >::const_iterator fniter =
         allFuncsInGroup.begin();
      pdvector< pair<pdstring, const function_t *> >::const_iterator fnfinish =
         allFuncsInGroup.end();
      for (; fniter != fnfinish; ++fniter) {
         const pdstring &modName = fniter->first;
         const function_t *fn = fniter->second;

         const pdstring &fnLongName =
            outliningMisc::getOutlinedFnModName(groupUniqueId) + "/" +
            outliningMisc::getOutlinedFnName(modName, *fn);

         if (!knownDownloadedCodeAddrsDict.defines(fnLongName))
            // the "if" is regrettably needed until I can do some cleaning up.
            // It happens when an original version of a fn, and an already-outlined
            // version try to get placed in the group at the same time, causing the
            // same "fnLongName[]" and hence a dictionary key conflict.
            knownDownloadedCodeAddrsDict.set(fnLongName, fn->getEntryAddr());
      }

      // Second, "test" outlining each function in isolation
      for (fniter = allFuncsInGroup.begin(); fniter != fnfinish; ++fniter) {
         const pdstring &modName = fniter->first;
         const function_t *fn = fniter->second;

         const fnCodeObjects *theFnCodeObjects =
            parsedFnCodeObjects.get(fn->getEntryAddr());

         pdvector<bbid_t> blocksPresentlyInlined;
         pdvector<bbid_t> blocksPresentlyOutlined;
         kptr_t presentPreFnDataStartAddr = 0;
         kptr_t presentInlinedBlocksStartAddr = 0;
         kptr_t presentOutlinedBlocksStartAddr = 0;
         getInlinedAndOutlinedInfoAboutFn(*fn, *theFnCodeObjects,
                                          blocksPresentlyInlined, 
                                          blocksPresentlyOutlined,
                                          presentPreFnDataStartAddr,
                                          presentInlinedBlocksStartAddr,
                                          presentOutlinedBlocksStartAddr);

         testOutlineCoupledNoChanges(0, // dummy group unique id
                                     modName, *fn,
                                     *theFnCodeObjects,
                                     blocksPresentlyInlined,
                                     blocksPresentlyOutlined,
                                     presentPreFnDataStartAddr,
                                     presentInlinedBlocksStartAddr,
                                     presentOutlinedBlocksStartAddr,
                                     knownDownloadedCodeAddrsDict);
      }
   }

   // Using a whole bunch of emitters (one 3-chunker per function in this group),
   // emit code.
   dictionary_hash<kptr_t, const relocatableCode_t *> fn2EmittedCode(addrHash4);
   {
      pdvector< pair<pdstring, const function_t *> >::const_iterator fniter =
         allFuncsInGroup.begin();
      pdvector< pair<pdstring, const function_t *> >::const_iterator fnfinish =
         allFuncsInGroup.end();
      for (; fniter != fnfinish; ++fniter) {
         const pdstring &modName = fniter->first;
         const function_t *fn = fniter->second;

         // If it was in allFuncsInGroup[], then we're including it in our
         // inlined/outlined stuff:
         assert(isFunctionInGroupOracle.willFnBeInOptimizedGroup(modName, *fn));

         const kptr_t fnEntryAddr = fn->getEntryAddr();
         const fnCodeObjects *theFnCodeObjects = parsedFnCodeObjects.get(fnEntryAddr);
         const pdvector<unsigned> &theFnBlockCounts = fnBlockCounters.get(fnEntryAddr);
         assert(theFnBlockCounts.size() == fn->numBBs());

         extern instrumenter *theGlobalInstrumenter;
         const abi &theKernelABI = theGlobalInstrumenter->getKernelABI();
         sparc_tempBufferEmitter em(theKernelABI);

         assert(blockOrderingFunc == pickBlockOrderingOrigSeq ||
                blockOrderingFunc == pickBlockOrderingChains);

         assert(rootFnBlockAndEdgeCounts != NULL);
         pickBlockOrderingAndEmit1Fn(blockOrderingFunc,
                                     groupUniqueId,
                                     em, allFuncsInGroup,
                                     *rootFnBlockAndEdgeCounts,
                                     *fn, *theFnCodeObjects,
                                     theFnBlockCounts,
                                     theHotBlockThresholdChoice
                                     );
         assert(em.isCompleted());
         
         const relocatableCode_t *theRelocatableCode = new sparc_relocatableCode(em);
         assert(theRelocatableCode != NULL);

         fn2EmittedCode.set(fnEntryAddr, theRelocatableCode);
      }
   }

   // Emit contiguous multiple-function'd group.  Keep track of fn boundaries,
   // so we'll be able to parse each one later on.  We use "chunkRange",
   // which is typedef'd in the .I file.  (It's a pair; .first giving chunk's
   // startAddr, and .second giving the chunk's numbytes.)

   pdvector<downloadToKernelFn> fns; // we're gonna fill this in

   pdvector< pair<pdstring, const function_t *> >::const_iterator fniter =
      allFuncsInGroup.begin();
   pdvector< pair<pdstring, const function_t *> >::const_iterator fnfinish =
      allFuncsInGroup.end();
   for (; fniter != fnfinish; ++fniter) {
      const pdstring &origModName = fniter->first;
      const function_t *origFn = fniter->second;
      
      // If it was in allFuncsInGroup[], then we're including it in the optimized group
      // (because allFuncsInGroup[] has properly weeded out the functions that are
      // undeserving).
      assert(isFunctionInGroupOracle.willFnBeInOptimizedGroup(origModName, *origFn));

      const pdstring modName = outliningMisc::getOutlinedFnModName(groupUniqueId);
      const pdstring modDescriptionIfNew = outliningMisc::getOutlinedFnModDescription(groupUniqueId);
      const relocatableCode_t *theCode = 
	 relocatableCode_t::getRelocatableCode(*fn2EmittedCode.get(origFn->getEntryAddr()));
      assert(theCode->numChunks() == 3);
         // pre-code data, hot blocks, cold blocks.  Since (for now at least) the
         // entry block is always made hot and always emitted first in the hot chunk,
         // all we have to say is the entry chunk is chunk 1:
      const unsigned entry_chunk_ndx = 1;
      const bool parseAndAddToModule = true;

      const pdstring outlinedFnName =
         outliningMisc::getOutlinedFnName(origModName, *origFn);

      downloadToKernelFn *newFn = fns.append_with_inplace_construction();
      
      new((void*)newFn)downloadToKernelFn(modName, modDescriptionIfNew,
                                          outlinedFnName,
                                          theCode,
                                          entry_chunk_ndx,
                                          0, // ndx of chunk containing useful data
                                          parseAndAddToModule);
   }

   assert(fns.size() == allFuncsInGroup.size());
      // which might be less than fnBlockCounters.size() since some fns might have
      // been ignored.

   // emit ordering: all pre-fn data, then all hot blocks, then all cold blocks
   emitOrdering theEmitOrdering; // we're gonna fill this in

   // First, all pre-fn data chunks...
   for (unsigned fnlcv=0; fnlcv < allFuncsInGroup.size(); ++fnlcv)
      theEmitOrdering += make_pair(fnlcv, 0);

   // Second, all hot chunks...
   for (unsigned fnlcv=0; fnlcv < allFuncsInGroup.size(); ++fnlcv)
      theEmitOrdering += make_pair(fnlcv, 1);

   // Third, all cold chunks...
   for (unsigned fnlcv=0; fnlcv < allFuncsInGroup.size(); ++fnlcv)
      theEmitOrdering += make_pair(fnlcv, 2);

   // Here we go!
   extern machineInfo *theGlobalKerninstdMachineInfo;
   const bool tryForNucleus = theGlobalKerninstdMachineInfo->
       isKernelAddrInNucleus(rootFn.getEntryAddr());
//     const bool tryForNucleus = true;
   
   downloadCodeToKernelAndParseAndReplaceFn(fns, theEmitOrdering, tryForNucleus);
}

// ------------------------------------------------------------

void outlinedGroup::
downloadCodeToKernelAndParseAndReplaceFn(const pdvector<downloadToKernelFn> &fns,
                                         const emitOrdering &theEmitOrdering,
                                         bool tryForNucleus) {
   // This is the big one.
   assert(fns.size() == allFuncsInGroup.size());

   unsigned totalNumChunks = 0;
   pdvector<downloadToKernelFn>::const_iterator fniter = fns.begin();
   pdvector<downloadToKernelFn>::const_iterator fnfinish = fns.end();
   for (; fniter != fnfinish; ++fniter) {
      const downloadToKernelFn &fn = *fniter;
      totalNumChunks += fn.numChunks();
   }
   assert(theEmitOrdering.size() == totalNumChunks);

   extern instrumenter *theGlobalInstrumenter;
   assert(downloadedToKernelReqId == -1U);
   downloadedToKernelReqId =
      theGlobalInstrumenter->downloadToKernelAndParse(fns, theEmitOrdering,
                                                      tryForNucleus);
   assert(downloadedToKernelReqId != -1U);

   // now figure out outlinedCodeEntryAddr
   const downloadToKernelFn &rootFn = fns[rootFnNdx];
      // can't assert that rootFn's modName/fnName match those of
      // allFuncsInGroup[rootFnNdx] since rootFn's modname will be kludged up
      // to have "outlined" prepended to those names, whereas allFuncsInGroup[] contains
      // the original names.
   const unsigned rootFn_chunk_ndx = rootFn.entry_chunk_ndx;

   const pdvector< pair<pdvector<kptr_t>,unsigned> > downloadedAddrs =
      theGlobalInstrumenter->queryDownloadedToKernelAddresses(downloadedToKernelReqId);
   const pair<pdvector<kptr_t>,unsigned> &rootFnDownloadedAddrs =
      downloadedAddrs[rootFnNdx];
   assert(rootFnDownloadedAddrs.second == rootFn_chunk_ndx);
                     
   outlinedCodeEntryAddr = rootFnDownloadedAddrs.first[rootFnDownloadedAddrs.second];
   cout << "Code group's entrypoint: " << addr2hex(outlinedCodeEntryAddr) << endl;
   
   assert(replaceFnReqIds.size() == 0);

   if (doTheReplaceFunction) {
      cout << "Doing replaceFunction..." << endl;

      doReplaceFunctions(downloadedAddrs, replaceFunctionPatchUpCallSitesToo);

      assert(replaceFnReqIds.size() != 0);
   }
   else {
      cout << "NOT doing replaceFunction (just testing for now)" << endl;
   }
}

// --------------------

void outlinedGroup::doReplaceFunctions(
    const pdvector< pair<pdvector<kptr_t>,unsigned> > &downloadedAddrs,
    bool callSitesToo)
{
    extern instrumenter *theGlobalInstrumenter;

    const unsigned numFuncs = allFuncsInGroup.size();
    assert(downloadedAddrs.size() == numFuncs);

    for (unsigned ndx=0; ndx < numFuncs; ndx++) {
	const function_t *fn = allFuncsInGroup[ndx].second;
	const kptr_t oldEntryAddr = fn->getEntryAddr();

	const pair<pdvector<kptr_t>,unsigned> &fnDownloadedAddrs =
	    downloadedAddrs[ndx];
	const kptr_t outlinedEntryAddr =
	    fnDownloadedAddrs.first[fnDownloadedAddrs.second];

	unsigned reqid = theGlobalInstrumenter->replaceAFunction(
	    oldEntryAddr, // old fn
	    outlinedEntryAddr, // new fn. outlinedEntryAddr already skips past
	    // pre-code data, if any. This is indeed what we want here.
	    callSitesToo);
	replaceFnReqIds.push_back(reqid);

	cout << "Did replaceFunction from "
	     << addr2hex(oldEntryAddr) << " to "
	     << addr2hex(outlinedEntryAddr) << endl;
    }
}

// --------------------

// Initiate the process of outlining
void outlinedGroup::asyncOutline()
{
    assert(codeObjectParseRequestsNotYetSent.size() == 0);
    assert(pendingCodeObjectParseRequests.size() == 0);
    assert(parsedFnCodeObjects.size() == 0);
    assert(numNonNullEntriesInParsedFnCodeObjects == 0);
    assert(fnBlockCounters.size() > 0);

    for (dictionary_hash<kptr_t, pdvector<unsigned> >::const_iterator iter =
	     fnBlockCounters.begin(); iter != fnBlockCounters.end(); ++iter) {
	const kptr_t fnStartAddr = iter.currkey();
	codeObjectParseRequestsNotYetSent.set(fnStartAddr, true);
	// the rvalue is a dummy
    }

    // Now gently start the process of sending over code object parsing
    // requests, if necessary.
    asyncSend1CodeObjectParseRequest();
}

// Undo the outlining changes
void outlinedGroup::unOutline() {
   // XXX: the half-second delay is ugly, especially since kerninstd now has the
   // ability to do such delays on its own.

   extern instrumenter *theGlobalInstrumenter;

   // Drop pendingCodeObjectParseRequests[] and
   // codeObjectParseRequestsNotYetSent[] on the floor:
   codeObjectParseRequestsNotYetSent.clear();
   pendingCodeObjectParseRequests.clear();

   // First un-replace the outlined functions, if we've gotten that far
   if (replaceFnReqIds.size() > 0) {
       cout << "unOutline: doing un-replaceFns...";

       pdvector<unsigned>::const_iterator reqIdIter = replaceFnReqIds.begin();
       for (; reqIdIter != replaceFnReqIds.end(); reqIdIter++) {
	   theGlobalInstrumenter->unReplaceAFunction(*reqIdIter);
       }

       cout << "done" << endl;
       replaceFnReqIds.clear();
       usleep(500000);
       // half a second delay for safety before we fry the downloaded code
   }

   if (downloadedToKernelReqId != -1U) {
      // Before we tell the kernel to destroy the code, we need to perhaps un-instrument
      // a subset of metric-focuses.
      pdvector< pair<pdvector<kptr_t>,unsigned> > theAddrs = 
         theGlobalInstrumenter->queryDownloadedToKernelAddresses(downloadedToKernelReqId);
      pdvector< pair<pdvector<kptr_t>,unsigned> >::const_iterator fniter = theAddrs.begin();
      pdvector< pair<pdvector<kptr_t>,unsigned> >::const_iterator fnfinish = theAddrs.end();
      for (; fniter != fnfinish; ++fniter) {
#if 0      
         const pair<pdvector<kptr_t>,unsigned> &fninfo = *fniter;
         destroySomeCMFsDueToRemovalOfDownloadedKernelCode(fninfo.first[fninfo.second]);
         // instrument.C
#else
	 cout << "FIXME: destroy possible CMFs first!\n";
#endif
      }

      // XXX TODO: there's nice new infrastructure in kerninstd for delaying a bit
      // to ensure that no thread is still executing within code (like that block we're
      // about to fry); however, we're not using it and thus we're not as safe as
      // we could be.  (Although the 1/2 second delay @ the top of this method
      // probably helped.) It's not as simple as one whose actions are contained
      // entirely within kerninstd, like unsplicing.
      theGlobalInstrumenter->removeDownloadedToKernel(downloadedToKernelReqId);

      cout << "kperfmon unOutline(): de-allocated replacement group"
           << endl;

      downloadedToKernelReqId = -1U; // dtor asserts this
      outlinedCodeEntryAddr = 0; // dtor asserts this
   }
}
