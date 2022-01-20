// outliningTest.C

#include "outliningTest.h"
#include "codeObject.h"
#include "hotBlockCalcOracle.h"
#include "blockOrderingOrigSeq.h"
#include "instrumenter.h"
#include "outliningEmit.h" // emitAGroupOfBlocksInNewChunk()

typedef fnCode fnCode_t;

static unsigned
exactNumBytesForPreFunctionData(const function_t &fn,
                                const fnCodeObjects &theFnCodeObjects) {
   // the params are necessary; we can't store them in this class, since this class
   // is inherently multi-function.

   // a static method
   // e.g. for jump table data

   // It's important to accurately reflect the original ordering of emission.
   // For now, let's hope that it's the same as the ordering basic blocks.
   // In other words, basic block X emitted before basic block Y would
   // have jump table information before Y's

   unsigned result = 0;

   pdvector<kptr_t> allBlockAddrsSorted = fn.getAllBasicBlockStartAddressesSorted();
   pdvector<kptr_t>::const_iterator iter = allBlockAddrsSorted.begin();
   pdvector<kptr_t>::const_iterator finish = allBlockAddrsSorted.end();
   for (; iter != finish; ++iter) {
      const kptr_t bb_addr = *iter;
      bbid_t bb_id = fn.searchForBB(bb_addr);
      const basicblock_t *bb = fn.getBasicBlockFromId(bb_id);
      assert(bb->getStartAddr() == bb_addr);

      const basicBlockCodeObjects &theBasicBlockCodeObjects =
         theFnCodeObjects.getCodeObjectsForBlockById(bb_id);

      pdvector< pair<unsigned, codeObject*> >::const_iterator co_iter =
         theBasicBlockCodeObjects.begin();
      pdvector< pair<unsigned, codeObject*> >::const_iterator co_finish =
         theBasicBlockCodeObjects.end();
      for (; co_iter != co_finish; ++co_iter) {
         const codeObject *co = co_iter->second;
         result += co->exactNumBytesForPreFunctionData();
      }
   }
   
   return result;
}

void
getInlinedAndOutlinedInfoAboutFn(const function_t &fn,
                                 const fnCodeObjects &fco,
                                    // used to verify exact pre fn data num bytes
                                 pdvector<bbid_t> &blocksPresentlyInlined,
                                    // filled in; keeps present ordering, too
                                 pdvector<bbid_t> &blocksPresentlyOutlined,
                                    // filled in; keeps present ordering, too
                                 kptr_t &preFnDataStartAddr, // filled in
                                 kptr_t &inlinedBlocksStartAddr, // filled in
                                 kptr_t &outlinedBlocksStartAddr // filled in
   ) {
   const fnCode_t::const_iterator entryChunkIter =
      fn.getOrigCode().findChunk(fn.getEntryAddr(), false);
   // false --> not definitely start of chunk
   assert(entryChunkIter != fn.getOrigCode().end());
   const fnCode_t::codeChunk &entryChunk = *entryChunkIter;
         
   pdvector< pair<kptr_t,unsigned> > theCodeRanges =
      fn.getOrigCode().getCodeRanges();
   // reminder: 0-sized chunks won't be present
   assert(theCodeRanges.size() <= 3); // at most: prefndata, hot, cold
         
   // find and remove entry chunk from "theCodeRanges":
   if (theCodeRanges[0].first == fn.getEntryAddr()) {
      theCodeRanges[0] = theCodeRanges.back();
      theCodeRanges.pop_back();
   }
   else if (theCodeRanges[1].first == fn.getEntryAddr()) {
      theCodeRanges[1] = theCodeRanges.back();
      theCodeRanges.pop_back();
   }
   else
      // no need to check theCodeRanges[2]; that's never the entry chunk
      assert(false);

   // Now "theCodeRanges[]" contains from 0 to 2 entries giving information
   // on the pre-fn data and/or the cold chunk.

   blocksPresentlyInlined.clear();
   blocksPresentlyOutlined.clear();

   const pdvector<bbid_t> allBlockIdsInEmitOrder =
      fn.getAllBasicBlockIdsSortedByAddress();
   pdvector<bbid_t>::const_iterator block_iter = allBlockIdsInEmitOrder.begin();
   pdvector<bbid_t>::const_iterator block_finish = allBlockIdsInEmitOrder.end();
   for (; block_iter != block_finish; ++block_iter) {
      const bbid_t bbid = *block_iter;
      const kptr_t bbStartAddr = fn.getBasicBlockFromId(bbid)->getStartAddr();

      if (entryChunk.enclosesAddr(bbStartAddr))
         blocksPresentlyInlined += bbid;
      else
         // we assume that if a bb isn't inlined then it must be outlined
         // (i.e. we don't consider that it might be part of the pre-fn data)
         blocksPresentlyOutlined += bbid;
   }

   const unsigned manuallyCalculatedPreFnDataSize =
      exactNumBytesForPreFunctionData(fn, fco);

   if (theCodeRanges.size() > 0 && theCodeRanges[0].first < entryChunk.startAddr) {
      // there is a chunk having lower addresses than the entry chunk; we'll
      // assume that this refers to pre-fn data.  This usually happens for outlined
      // code.  Hopefully some day, code parsed at kerninstd startup will also
      // contain this chunk.

      preFnDataStartAddr = theCodeRanges[0].first;
      theCodeRanges[0] = theCodeRanges.back();
      theCodeRanges.pop_back();

      assert(preFnDataStartAddr + manuallyCalculatedPreFnDataSize ==
             entryChunk.startAddr);
   }
   else {
      // There wasn't any chunk for the pre-fn data.  This usually happens for
      // originally parsed code.
      preFnDataStartAddr = entryChunk.startAddr - manuallyCalculatedPreFnDataSize;
   }
   
   // no need to check theCodeRanges[1] -- only [0] can possibly be pre-fn data.

   assert(preFnDataStartAddr + manuallyCalculatedPreFnDataSize == entryChunk.startAddr);

   inlinedBlocksStartAddr = entryChunk.startAddr;
   
   outlinedBlocksStartAddr = 0;
   assert(theCodeRanges.size() <= 1);
   if (theCodeRanges.size() > 0) {
      assert(theCodeRanges[0].first > entryChunk.startAddr);
      outlinedBlocksStartAddr = theCodeRanges[0].first;
   }
}

static bool compare_code(const insnVec_t &v1,
                         const insnVec_t &v2) {
   if (v1.numInsnBytes() != v2.numInsnBytes())
      return false;
   
   for (unsigned lcv=0; lcv < v1.numInsns(); ++lcv) {
      const instr_t *i1 = v1.get_by_ndx(lcv);
      const instr_t *i2 = v2.get_by_ndx(lcv);

      if (! (*(const sparc_instr*)i1 == *(const sparc_instr*)i2))
         return false;
   }

   return true;
}

static void disassemble(const insnVec_t &i1,
                        const insnVec_t &i2,
                        kptr_t startAddr) {
   unsigned numinsns = std::max(i1.numInsns(), i2.numInsns());
   kptr_t insnAddr = startAddr;
   kptr_t lastPrintedInsnAddr = 0;

   for (unsigned lcv=0; lcv < numinsns; ++lcv, insnAddr += 4) {
      pdstring s1 = (lcv >= i1.numInsns() ? pdstring("") : i1.get_by_ndx(lcv)->disassemble(insnAddr));
      pdstring s2 = (lcv >= i2.numInsns() ? pdstring("") : i2.get_by_ndx(lcv)->disassemble(insnAddr));

      if (s1 != s2) {
         // if a gap is detected, print "...\n"
         if (lastPrintedInsnAddr != 0 &&
             insnAddr > lastPrintedInsnAddr + 4)
            cout << "..." << endl;

         // we have a mismatch
         cout << addr2hex(insnAddr) << " [*] ";

         cout << s1;
         const unsigned padding = s1.length() >= 40 ? 0 : 40 - s1.length();
         for (unsigned slcv=0; slcv < padding; ++slcv)
            cout << ' ';
         cout << s2;
         cout << endl;

         lastPrintedInsnAddr = insnAddr;
      }
   }
}

static void
chunkwise_compare(const char *chunkName,
                  const insnVec_t &origCode,
                  const insnVec_t &emittedCode,
                  kptr_t chunkStartAddr,
                  const pdstring &modName,
                  const function_t &fn) {
   // a static method

   if (!compare_code(origCode, emittedCode)) {
      cout << "code comparison failed for " << modName << "/"
           << fn.getPrimaryName().c_str() << ": " << chunkName << endl;
      cout << "Here are the differences (old code on the left; new code on the right)"
           << endl;
      disassemble(origCode, emittedCode, chunkStartAddr);
   }
}

class ignorantResolver {
 private:
   dictionary_hash<pdstring, kptr_t> knownDownloadedCodeAddrs;
      // This is very useful when outlining multiple functions at a time, and when
      // one outlined piece of code has to make an interprocedural branch to, or a call
      // to, other outlined code.  Such branches/calls are emitted via symname, not
      // not destAddr (since destAddr isn't known until the last moment), and thus
      // require some smarts from us to resolve.

 public:
   ignorantResolver(const dictionary_hash<pdstring,kptr_t> &iknownDownloadedCodeAddrs) :
      knownDownloadedCodeAddrs(iknownDownloadedCodeAddrs) {
   }

   pair<bool, kptr_t> obtainFunctionEntryAddr(const pdstring &fnName) const {
      // "fnName" is of the form "modName/fnName", right?
      kptr_t result = 0;
      if (knownDownloadedCodeAddrs.find(fnName, result))
         return std::make_pair(true, result);
      
      return std::make_pair(false, 0);
   }

   pair<bool, kptr_t> obtainImmAddr(const pdstring &/*objectName*/) const {
      assert(false);
   }
};

void
testOutlineCoupledNoChanges(unsigned groupUniqueId,
                            const pdstring &modName,
                            const function_t &fn,
                            const fnCodeObjects &theFnCodeObjects,
                            const pdvector<bbid_t> &blocksToInline,
                            const pdvector<bbid_t> &blocksToOutline,
                            // The next 3 params allow emitted test code to be
                            // disjoint...necessary for properly testing code that
                            // was already disjoint (e.g. already outlined)
                            kptr_t emitPreFnDataStartAddr,
                            kptr_t emitInlinedBlocksStartAddr,
                               // should equal fn.getEntryAddr(), right?
                            kptr_t emitOutlinedBlocksStartAddr,
                            const dictionary_hash<pdstring, kptr_t> &
                               knownDownloadedCodeAddrsDict
                            // although this routine just outlines a single fn in
                            // isolation, the code objects have already been parsed
                            // with a group mentality, so we may need to resolve some
                            // calls-to-name; that's why this param needs to be passed
                            // in.
                            ) {
   // a static method
   // First, assert that "blocksToInline" and "blocksToOutline" are sensible:
   // no duplicates and together make up the full set of basic block id's
   pdvector<bbid_t> temp_ids = blocksToInline;
   temp_ids += blocksToOutline;
   assert(temp_ids.size() == fn.numBBs());
   std::sort(temp_ids.begin(), temp_ids.end()); // STL's sort
   assert(std::adjacent_find(temp_ids.begin(), temp_ids.end()) == temp_ids.end());

   // This routine fudges bbCounts so that everything in "blocksToInline" has a count
   // of 1 and everything in "blocksToOutline" has a count of 0.
   pdvector<unsigned> bbCounts(fn.numBBs(), 0);
   pdvector<bbid_t>::const_iterator blocksToInlineIter = blocksToInline.begin();
   pdvector<bbid_t>::const_iterator blocksToInlineFinish = blocksToInline.end();
   for (; blocksToInlineIter != blocksToInlineFinish; ++blocksToInlineIter) {
      const bbid_t bbid = *blocksToInlineIter;
      
      assert(bbCounts[bbid] == 0);
      bbCounts[bbid] = 1;
   }
   
   //const unsigned coldBlockThreshold = 0; // <= this amt --> a cold block

   hotBlockCalcOracle isBlockInlined(fn, bbCounts, hotBlockCalcOracle::AnyNonZeroBlock);

   if (isBlockInlined.getNumFudgedEdges() > 0)
      cout << "Sorry, but "
           << isBlockInlined.getNumFudgedEdges() << " of "
           << isBlockInlined.getTotalNumEdges() << " edges had to be fudged...";

   // We'll call pickBlockOrderingOrigSeq() strictly for assertion purposes, since
   // it should set theInlinedBlockIds to blocksToInline, and it should set
   // theOutlinedBlockIds to blocksToOutline.  We'll check this.
   pdvector<bbid_t> theInlinedBlockIds;
   pdvector<bbid_t> theOutlinedBlockIds;
   pickBlockOrderingOrigSeq(isBlockInlined, fn,
                            theInlinedBlockIds,
                            theOutlinedBlockIds);
      // a static method
      // We go with original sequencing here to maximize the chances that
      // the emitted code will be the same.
   assert(theInlinedBlockIds.size() == blocksToInline.size() &&
          std::equal(theInlinedBlockIds.begin(), theInlinedBlockIds.end(),
                     blocksToInline.begin()));
   assert(theOutlinedBlockIds.size() == blocksToOutline.size() &&
          std::equal(theOutlinedBlockIds.begin(), theOutlinedBlockIds.end(),
                     blocksToOutline.begin()));

   // Unlike for our "ignorantResolver" variable below, our "outliningLocations"
   // object should *not* be told that every routine in the group is being
   // inlined/outlined, because it might then report a false positive when queried
   // can fn A (in the group) reach fn B (also in the group) with an interprocedural
   // branch?  (Unfortunately, our solution may be too harsh, leading to false
   // *negatives*....)
   pdvector< pair<pdstring, const function_t*> >
      funcsInlinedAndOutlined(1, std::make_pair(modName, &fn));

   outliningLocations whereBlocksNowReside(groupUniqueId,
                                           outliningLocations::afterInlinedBlocks,
                                              // where outlined blocks reside
                                           funcsInlinedAndOutlined,
                                           true, // just testing -- nothing's moving
                                           theInlinedBlockIds,
                                           theOutlinedBlockIds);

   extern instrumenter *theGlobalInstrumenter;

//     const unsigned exactNumBytesForPreFnData =
//        exactNumBytesForPreFunctionData(fn, theFnCodeObjects);
//     const kptr_t preFnDataEmitAddr = fn.getEntryAddr() - exactNumBytesForPreFnData;

   sparc_tempBufferEmitter em(theGlobalInstrumenter->getKernelABI());
      // will contain 3 chunks: pre-fn data (often empty), inlined blocks,
      // and (if any) outlined blocks.
      // Each chunk is fully relocatable and independent of placement viz. the other
      // two chunks.
      // When done emitting, we can assert
      // that the first chunk is exactly "exactNumBytesForPreFnData" bytes.

   // "em" presently contains a single chunk, which we'll use for pre-fn data (if any)
   
   emitAGroupOfBlocksInNewChunk(true, // try to keep original code (since testing)
                                em,
                                whereBlocksNowReside,
                                fn, theFnCodeObjects,
                                theInlinedBlockIds);
   assert(em.numChunks() == 2);

   emitAGroupOfBlocksInNewChunk(true, // try to keep original code (since testing)
                                em,
                                whereBlocksNowReside,
                                fn, theFnCodeObjects,
                                theOutlinedBlockIds);
   em.complete();

   assert(em.getChunk(0)->numInsnBytes() ==
          exactNumBytesForPreFunctionData(fn, theFnCodeObjects));
      // 0 --> the pre-fn data chunk (jump table stuff)

   const sparc_relocatableCode em_rc(em);

   // Let's think about resolving calls by name.  You'll recall that the code objects
   // have already been parsed, with a mindset of a *group* of fns being co-outlined;
   // any calls to such functions will have code object type "call by name" instead
   // of the usual "call by addr", and it's too late to change that, even though it
   // causes us a headache in this "testing" routine where in actuality only a single
   // routine is being outlined.  Following me?  Good.  Anyway, all it means is that
   // we must fill "knownDownloadedCodeAddrsDict[]" with more than just the single
   // function that is being test-outlined here; we must fill it with the entire group
   // of functions.  This group is available in "this->allFuncsInGroup[]", but since
   // this is a static method (and we don't want to change that), we can't access it.
   // So instead we pass that as a parameter to this routine.

   ignorantResolver theResolver(knownDownloadedCodeAddrsDict);

   pdvector<kptr_t> chunkStartAddrs;
   chunkStartAddrs += emitPreFnDataStartAddr;
   chunkStartAddrs += emitInlinedBlocksStartAddr;
   assert(emitInlinedBlocksStartAddr == fn.getEntryAddr());
   chunkStartAddrs += emitOutlinedBlocksStartAddr;
   assert(chunkStartAddrs.size() == em.numChunks());

   const unsigned exactNumBytesForPreFnData =
      exactNumBytesForPreFunctionData(fn, theFnCodeObjects);
   
   const pdvector<insnVec_t*> &resolvedCode =
      em_rc.obtainFullyResolvedCode(chunkStartAddrs, theResolver);

   const insnVec_t *emittedPreFnData = resolvedCode[0]; // could be empty
   const insnVec_t *emittedHotCodePortion = resolvedCode[1];
   const insnVec_t *emittedColdCodePortion = resolvedCode[2]; // could be empty

   // Now let's do a chunkwise comparison against a peek of the kernel's code.
   // We probably have to peek at the kernel's code rather than look at the function's
   // origInsns, at least for comparing pre-function data, since it's not stored
   // in the function's "fnCode" structure.
   if (emittedPreFnData->numInsnBytes() > 0 ||
       exactNumBytesForPreFunctionData(fn, theFnCodeObjects)) {
      // we need to check pre-fn data:
      const pdvector<uint32_t> origData =
         theGlobalInstrumenter->peek_kernel_contig(emitPreFnDataStartAddr,
                                                   exactNumBytesForPreFnData);
      
      const insnVec_t *origPreFnData = insnVec_t::getInsnVec(&origData[0], exactNumBytesForPreFnData);

      chunkwise_compare("pre-fn data", *origPreFnData, *emittedPreFnData,
                        emitPreFnDataStartAddr,
                        modName, fn);
   }

   // Next, check the hot chunk:
   fnCode_t::const_iterator entryChunkIter = // entry chunk --> hot chunk
      fn.getOrigCode().findChunk(fn.getEntryAddr(), false);
      // false --> not definitely start of chunk (actually it will be)
   assert(entryChunkIter != fn.getOrigCode().end());
   const fnCode_t::codeChunk &entryChunk = *entryChunkIter;
   chunkwise_compare("hot blocks",
                     *entryChunk.theCode, *emittedHotCodePortion,
                     fn.getEntryAddr(), // chunk start addr
                     modName, fn);

   // Next, if it exists, check the cold chunk:
   fnCode_t::const_iterator chunkAfterEntryChunkIter = entryChunkIter + 1;
   const bool coldChunkExistedInOrigCode = (chunkAfterEntryChunkIter != fn.getOrigCode().end());
   if (emittedColdCodePortion->numInsnBytes() > 0 || coldChunkExistedInOrigCode) {
      if (coldChunkExistedInOrigCode)
         chunkwise_compare("cold blocks",
                           *chunkAfterEntryChunkIter->theCode,
                           *emittedColdCodePortion,
                           chunkAfterEntryChunkIter->startAddr,
                           modName, fn);
      else {
         insnVec_t *emptyInsnVec = insnVec_t::getInsnVec();
         chunkwise_compare("cold blocks", *emptyInsnVec, 
			   *emittedColdCodePortion,
                           chunkStartAddrs[2],
                           modName, fn);
      }
   }
}

extern kerninstdClientUser *connection_to_kerninstd;

int testOutliningOneFnInIsolation(
    const pdstring &modName, const function_t &fn,
    const dictionary_hash<pdstring,kptr_t> &knownDownloadedCodeAddrsDict)
{
   cout << modName << '/' << fn.getPrimaryName().c_str() << "...";
         
   if (fn.isUnparsed())
      cout << "(not parsed, never mind)" << endl;
   // (Note that we can (and do) test outlining a fn that is parsed but un-analyzed,
   // so we don't have a check for that)
   else {
      cout << "get codeObjects..." << flush;

      pdvector<fnAddrBeingRelocated> fnAddrsBeingRelocated; // empty, on purpose
      fnCodeObjects theFnCodeObjects =
         connection_to_kerninstd->syncParseFnIntoCodeObjects(fn.getEntryAddr(),
                                                             fnAddrsBeingRelocated);
      if (theFnCodeObjects.isUnparsed())
         cout << "(kerninstd couldn't parse code objects, never mind)"
              << endl;

      else {
         cout << "test outlining..." << flush;

         pdvector<bbid_t> blocksPresentlyInlined; // filled in
         pdvector<bbid_t> blocksPresentlyOutlined; // filled in
         kptr_t emitPreFnDataStartAddr = 0; // filled in
         kptr_t emitInlinedBlocksStartAddr = 0; // filled in
         kptr_t emitOutlinedBlocksStartAddr = 0; // filled in
         getInlinedAndOutlinedInfoAboutFn(fn, theFnCodeObjects,
                                          blocksPresentlyInlined,
                                          blocksPresentlyOutlined,
                                          emitPreFnDataStartAddr,
                                          emitInlinedBlocksStartAddr,
                                          emitOutlinedBlocksStartAddr);
            // in outliningTest.[hC]
         
         testOutlineCoupledNoChanges(0, // dummy group unique id
                                     modName, fn,
                                     theFnCodeObjects,
                                     blocksPresentlyInlined,
                                     blocksPresentlyOutlined,
                                     emitPreFnDataStartAddr,
                                     emitInlinedBlocksStartAddr,
                                     emitOutlinedBlocksStartAddr,
                                     knownDownloadedCodeAddrsDict);
            // in outliningTest.[hC]
            
         cout << "OK" << endl;
      }
   }

   return 0;
}

