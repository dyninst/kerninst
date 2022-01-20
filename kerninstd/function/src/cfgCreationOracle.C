// cfgCreationOracle.C

#include "cfgCreationOracle.h"
#include "simplePath.h"
#include "util/h/minmax.h"
#include <algorithm>

#ifdef sparc_sun_solaris2_7
#include "sparc_cfgCreationOracle.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_cfgCreationOracle.h"
#elif defined(rs6000_ibm_aix5_1)
#include "power_cfgCreationOracle.h"
#endif

// ----------------------------------------------------------------------

cfgCreationOracle::
cfgCreationOracle(function_t &ifn,
		  dictionary_hash<kptr_t, bool> &iinterProcJumpsAndCalls,
		  pdvector<hiddenFn> &ihiddenFnsToParse,
		  const kmem *ikm,
		  bool iverbose, bool iverbose_hiddenFns,
		  bbParsingStats &iBBParsingStats) :
      parseOracle(ifn),
      interProcJumpsAndCalls(iinterProcJumpsAndCalls),
      hiddenFnsToParse(ihiddenFnsToParse),
      km(ikm),
      verbose(iverbose), verbose_hiddenFns(iverbose_hiddenFns),
      theBBParsingStats(iBBParsingStats) {}

cfgCreationOracle::~cfgCreationOracle() {
   pdvector<jumpTableMetaData*>::iterator iter = allJumpTablesMetaData.begin();
   pdvector<jumpTableMetaData*>::iterator finish = allJumpTablesMetaData.end();
   for (; iter != finish; ++iter) {
      jumpTableMetaData *md = *iter;
      delete md;
   }
   delete km;
}

cfgCreationOracle *
cfgCreationOracle::getCfgCreationOracle(
            function_t &ifn,
	    dictionary_hash<kptr_t, bool> &iinterProcJumpsAndCalls,
	    pdvector<hiddenFn> &ihiddenFnsToParse,
	    const kmem *ikm,
	    bool iverbose, bool iverbose_hiddenFns,
	    bbParsingStats &iBBParsingStats) {
   cfgCreationOracle *co;
#ifdef sparc_sun_solaris2_7
   co = new sparc_cfgCreationOracle(ifn, iinterProcJumpsAndCalls,
				    ihiddenFnsToParse, ikm, iverbose,
				    iverbose_hiddenFns, iBBParsingStats);
#elif defined(i386_unknown_linux2_4)
   co = new x86_cfgCreationOracle(ifn, iinterProcJumpsAndCalls,
				  ihiddenFnsToParse, ikm, iverbose,
				  iverbose_hiddenFns, iBBParsingStats);
#elif defined(rs6000_ibm_aix5_1)
   co = new power_cfgCreationOracle (ifn, iinterProcJumpsAndCalls,
                                    ihiddenFnsToParse, ikm, iverbose,
                                    iverbose_hiddenFns, iBBParsingStats);
#endif
   return co;
}
   
void cfgCreationOracle::
ensureBlockStartInsnIsAlsoBlockOnlyInsn(kptr_t addr,
					instr_t *insn) const {
   fn.ensureBlockStartInsnIsAlsoBlockOnlyInsn(addr, insn);
}

void cfgCreationOracle::
processSeveralNonControlFlowInsnsAsInsnVecSubset(
           bbid_t bb_id,
	   kptr_t currNonControlFlowCodeChunkStartAddr,
	   unsigned currNonControlFlowCodeChunkNumInsns) const {
   // note: currNonControlFlowCodeChunkNumInsns could be zero

   kptr_t myAddr = currNonControlFlowCodeChunkStartAddr;
   while (currNonControlFlowCodeChunkNumInsns--) {
      const instr_t *i = fn.get1OrigInsn(myAddr);
       // TODO: we can do better than fn.get1OrigInsn(): because a basic block
       // never crosses a fnCode chunk boundary, we can get a reference to
       // the fnCode_t::codeChunk before the while loop and call its
       // version of get1Insn().  Will be a bit faster.
      fn.bumpUpBlockSizeBy1Insn(bb_id, i);

      myAddr += i->getNumBytes();
   }
}

void cfgCreationOracle::
adjustForNewJumpTableStartAddr(kptr_t jumpTableStartAddr) {
   if (std::find(allJumpTableStartAddrs.begin(),
                 allJumpTableStartAddrs.end(),
                 jumpTableStartAddr) != allJumpTableStartAddrs.end()) {
      // already exists -- unexpected
      assert(false && "cfgCreationOracle adjustForNewJumpTableStartAddr: jumpTableStartAddr already exists (unexpected)");
   }

   allJumpTableStartAddrs += jumpTableStartAddr;
   std::sort(allJumpTableStartAddrs.begin(), allJumpTableStartAddrs.end());

   pdvector<jumpTableMetaData*>::const_iterator iter = allJumpTablesMetaData.begin();
   pdvector<jumpTableMetaData*>::const_iterator finish = allJumpTablesMetaData.end();
   for (; iter != finish; ++iter) {
      jumpTableMetaData *theJumpTableMetaData = *iter; // not const
      
      theJumpTableMetaData->maybeAdjustJumpTableSize(jumpTableStartAddr);
   }
}

void cfgCreationOracle::
processInterproceduralFallThru(bbid_t bb_id) {
   // add ExitBB as a successor to this basic block (consistent with how we treat
   // an interprocedural branch),
   // and set the flag fallsThroughToNextFunction to true.
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);
   fn.markBlockAsHavingInterProcFallThru(bb_id);

   if (verbose) {
      const kptr_t falleeFnStartAddr =
         fn.getBasicBlockFromId(bb_id)->getEndAddrPlus1();
      cout << "NOTE: Function " << fn.getPrimaryName().c_str() << " starting at "
           << addr2hex(fn.getEntryAddr()) << " falls through to the next function, at "
           << addr2hex(falleeFnStartAddr) << endl;
   }

   theBBParsingStats.numInterProceduralStumbles++;
      // except for interprocedural stumbles that happen
      // as a result of the fall-thru case of a conditional branch.
}


void cfgCreationOracle::
processStumbleOntoExistingBlock(bbid_t bb_id,
                                bbid_t stumble_bb_id,
                                const simplePath &/*thePath*/) {
   fn.addSuccessorToBlockByIdNoDup(bb_id, stumble_bb_id);
   if (!fn.addPredecessorToBlockByIdUnlessDup(stumble_bb_id, bb_id))
      assert(false);

   // No need for us to recursively parse stumble_bb_id, for it must have already
   // been parsed into cfg in order for us to know that we're stumbling onto it!
   // We can even assert this.

   theBBParsingStats.numStumblesOntoExistingBlock++;
}


void cfgCreationOracle::
processCallToRoutineReturningOnCallersBehalf(bbid_t bb_id,
                                             instr_t *i,
                                             kptr_t /*instrAddr*/,
                                             kptr_t calleeAddr,
                                             bool delaySlotInSameBlock,
                                             instr_t *delayInsn) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, i);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delayInsn);

   if (verbose) {
      cout << "NOTE: parse(): call to " << addr2hex(calleeAddr)
           << " appears to be to a function" << endl
           << "known to return on the caller's behalf.  Thus, assuming "
           << "there's no fall-through for this call" << endl;
   }

   // Add ExitBB as a successor to this block:
   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);
   
   // Add to interProcJumps[].  If it was already there...fine.
   assert(instr_t::instructionHasValidAlignment(calleeAddr));
   interProcJumpsAndCalls[calleeAddr] = true;

   delete i;
   delete delayInsn;
}

void cfgCreationOracle::
processCallToObtainPC(bbid_t bb_id, instr_t *callInsn,
                      kptr_t /*callInsnAddr*/,
                      bool delaySlotInSameBlock,
                      instr_t *delaySlotInsn) {
   // Add the insn, and the delay slot too, even though we'll continue to parse
   // the basic block.  This is consistent behavior with a "normal" call
   // (see processNonTailOptimizedTrueCall()).
   fn.bumpUpBlockSizeBy1Insn(bb_id, callInsn);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delaySlotInsn);
   
   theBBParsingStats.numCallsJustToObtainPC++;
   
   delete callInsn;
   delete delaySlotInsn;
}

void cfgCreationOracle::
noteHiddenIntraProceduralFn(kptr_t calleeAddr) {
   // let the name of the new function be the name of this function, plus
   // a dot, plus the hex address of the callee basic block.  That should
   // be sufficient for uniqueness

   pdstring dest_fn_name = fn.getPrimaryName().c_str();
   dest_fn_name += ".";
   dest_fn_name += addr2hex(calleeAddr);
   hiddenFnsToParse += hiddenFn(calleeAddr, dest_fn_name);

   if (verbose_hiddenFns)
      cout << "Found a call to " << addr2hex(calleeAddr)
           << ", which is NOT the entry of a function" << endl
           << "So, will mark " << addr2hex(calleeAddr)
           << " as a \"hidden\" function" << endl;

   theBBParsingStats.numHiddenIntraProcFnCalls++;
}


void cfgCreationOracle::
processReturn(bbid_t bb_id, instr_t *insn,
	      kptr_t /*insnAddr*/,
	      bool delaySlotInSameBlock,
	      instr_t *dsInsn) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, insn);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   theBBParsingStats.numRetOrRetl++;
   
   delete insn;
   delete dsInsn;
}


void cfgCreationOracle::
processJmpBehavingAsRet(bbid_t bb_id, instr_t *insn,
			kptr_t /*insnAddr*/,
			bool delaySlotInSameBlock,
			instr_t *dsInsn) {
   //  if (verbose_fnParse)
//        cout << "Found an effective retl instruction (saved link reg trick)"
//             << " at " << addr2hex(insnAddr) << endl;

   fn.bumpUpBlockSizeBy1Insn(bb_id, insn);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   theBBParsingStats.numJmpBehavingAsRetl++;

   delete insn;
   delete dsInsn;
}

void cfgCreationOracle::
processDoneOrRetry(bbid_t bb_id,
                   instr_t *insn,
                   kptr_t /*insnAddr*/) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, insn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   theBBParsingStats.numDoneOrRetry++;
   delete insn;
}

void cfgCreationOracle::
processTrueCallRestore(bbid_t bb_id,
                       instr_t *callInstr,
                       kptr_t /*instrAddr*/,
                       kptr_t destAddr,
                       bool /*nonTrivialRestore*/,
                       bool /*dangerRestore*/,
                          // dangerous if rd isn't %o0 thru %o5 or %g; we won't be
                          // able to unwind it.
                       const reg_t */*regWrittenByRestore*/,
                       bool delaySlotInSameBlock,
                       instr_t *dsInsn) {
   //  if (verbose_fnParse)
//        cout << "found tail call sequence (delay instruc uses restore) at "
//             << addr2hex(instrAddr) << endl;
//     if (verbose_fnParse || dangerRestore) {
//        cout << "tail-calling routine at " << addr2hex(destAddr) << endl;
//        cout << "That was case 1" << endl;
//        if (nonTrivialRestore)
//           cout << "p.s. the restore was non-trivial" << endl;
//        if (dangerRestore)
//           cout << "WARNING: cannot unwind this tail call sequence because restore writes to " << regWrittenByRestore->disass() << endl;
//     }

   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   assert(instr_t::instructionHasValidAlignment(destAddr));
   interProcJumpsAndCalls[destAddr] = true; // value is a dummy; key is what matters

   theBBParsingStats.numTailCalls_TrueCallRestore++;

   delete callInstr;
   delete dsInsn;
}


void cfgCreationOracle::
processJmplCallRestore(bbid_t bb_id, instr_t *callInstr,
		       kptr_t /*instrAddr*/,
		       bool /*nonTrivialRestore*/,
		       bool dangerousRestore,
		       // dangerous if rd isn't %o0 thru %o5 or %g; we won't be
		       // able to unwind it.
		       const reg_t *rdOfRestore,
		       bool delaySlotInSameBlock,
		       instr_t *dsInsn) {
   //  if (verbose_fnParse)
//        cout << "found tail call sequence (delay instruc uses restore) at "
//             << addr2hex(instrAddr) << endl;
//     if (verbose_fnParse || dangerousRestore)
//        cout << "That was case 2" << endl;
//     if (verbose_fnParse && nonTrivialRestore)
//        cout << "p.s. the restore was non-trivial" << endl;
   if (dangerousRestore)
      cout << "WARNING: cannot unwind this tail call sequence because restore writes to " << rdOfRestore->disass() << endl;

   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   theBBParsingStats.numTailCalls_JmplRestore++;
}

void cfgCreationOracle::
processJmplRestoreOptimizedTailCall(bbid_t bb_id,
				    instr_t *instr,
				    kptr_t /*insnAddr*/,
				    bool delaySlotInSameBlock,
				    instr_t *dsInsn) const {
   fn.bumpUpBlockSizeBy1Insn(bb_id, instr);
   // formerly appendToBBContents()
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);
  
   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);
   
   delete instr;
   delete dsInsn;
}

void cfgCreationOracle::
prepareSimpleJumpTable32(bbid_t bb_id,
                         instr_t *jmpInsn,
                         kptr_t /*jmpInsnAddr*/,
                         bool delaySlotInThisBlock,
                         instr_t *delaySlotInsn,
                         kptr_t jumpTableDataStartAddr,
                         const simplePathSlice_t &) {
   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   
   fn.bumpUpBlockSizeBy1Insn(bb_id, jmpInsn);
   if (delaySlotInThisBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delaySlotInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
      // for now, at least

   // We're certain of the start addr of the data of this jump table; inform
   // the oracle, so that later-created jump tables (or even those that have already
   // been created) can possibly trim their bounds as appropriate.
   adjustForNewJumpTableStartAddr(jumpTableDataStartAddr);

   delete jmpInsn;
   delete delaySlotInsn;
}


void cfgCreationOracle::
process1SimpleJumpTable32Entry(bbid_t bb_id, // uniquely id's the jmp table
                               kptr_t nextJumpTableEntry,
                               const simplePath &pathFromEntryBB) {
   // Sanity check: all successor basic blocks should be found if we search
   // for them by their start addresses (since none will be ExitBB/UnanalyzableBB)
#ifdef _DO_EVEN_EXPENSIVE_ASSERTS_
   const basicblock_t::ConstSuccIter start=fn.getBasicBlockFromId(bb_id)->beginSuccIter();
   const basicblock_t::ConstSuccIter finish=fn.getBasicBlockFromId(bb_id)->endSuccIter();
   for (basicblock_t::ConstSuccIter succiter=start; succiter != finish; ++succiter) {
      bbid_t succ_bb_id = *succiter;
      const basicblock_t *succbb = fn.getBasicBlockFromId(succ_bb_id);
      const kptr_t succbbStartAddr = succbb->getStartAddr();
         
      bbid_t searched_succbb_id = fn.searchForBB(succbbStartAddr);
      assert(succ_bb_id == searched_succbb_id);
   }
#endif

   bbid_t succ_bb_id = fn.searchForBB(nextJumpTableEntry);
   if (succ_bb_id == (bbid_t)-1) {
      // nextJumpTableEntry doesn't lie w/in an existing bb; recurse.
      basicblock_t *newbb = basicblock_t::getBasicBlock(nextJumpTableEntry, 
							bb_id);
      succ_bb_id = fn.addBlock_new(newbb);

      fn.addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id);
      // won't be a duplicate since it didn't exist anywhere til now.

      fn.parse(succ_bb_id,
		simplePath(pathFromEntryBB, bb_id), // new path
		this);

      assert(succ_bb_id == fn.searchForBB(nextJumpTableEntry));
   }
   else if (fn.getBasicBlockFromId(succ_bb_id)->getStartAddr()==nextJumpTableEntry) {
      // nextJumpTableEntry lies at the start of an existing bb; use it.
      // There's a very real chance of this bb already being a child; must weed
      // out duplicate successors.
      if (fn.addSuccessorToBlockByIdUnlessDup(bb_id, succ_bb_id)) {
         if (!fn.addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
            assert(false);
      }
      else
         assert(fn.getBasicBlockFromId(bb_id)->hasAsASuccessor(succ_bb_id));
   }
   else {
      // nextJumpTableEntry lies in the middle of an existing bb; split it.
      //	 cout << "jump table entry " << addr2hex(nextJumpTableEntry) << " being added via splitting its existing bb" << endl;

      // assert that we are not splitting ourselves
      assert(succ_bb_id != bb_id);

      succ_bb_id = fn.splitBasicBlock(succ_bb_id, nextJumpTableEntry);
                                                        
      // the new succ_bb_id won't be a dup since it didn't exist until the split
      if (!fn.addSuccessorToBlockByIdUnlessDup(bb_id, succ_bb_id))
         assert(false);
      if (!fn.addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
         assert(false);
   }
}


void cfgCreationOracle::
completeSimpleJumpTable32(bbid_t bb_id, kptr_t /*jumpTableStartAddr*/) {
   //  if (verbose_fnParse)
//        cout << "Done processing simple-32 jump table starting at "
//             << addr2hex(jumpTableStartAddr) << "; it contains "
//             << fn.getBasicBlockFromId(bb_id)->numSuccessors()
//             << " non-duplicate entries" << endl;

   theBBParsingStats.numSimpleJumpTables++;
   theBBParsingStats.numSimpleJumpTableTotalNonDupEntries += fn.getBasicBlockFromId(bb_id)->numSuccessors();
}


void cfgCreationOracle::
prepareTaggedJumpTable32(bbid_t bb_id,
                         kptr_t /*jumpTableStartAddr*/,
                         instr_t *jmpInsn,
                         bool delaySlotInThisBlock,
                         instr_t *delaySlotInsn) const {
   //  if (verbose_fnParse)
//        cout << "jump table (tagged-32) address is: "
//             << addr2hex(jumpTableStartAddr) << endl;

   fn.bumpUpBlockSizeBy1Insn(bb_id, jmpInsn);
   if (delaySlotInThisBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delaySlotInsn);

   delete jmpInsn;
   delete delaySlotInsn;
}


void cfgCreationOracle::
process1TaggedJumpTable32Entry(bbid_t bb_id,
                               uint32_t /*theTag*/,
                               kptr_t theDestAddr,
                               const simplePath &pathFromEntryBB) {
   // Sanity check: all successor basic blocks should be found if we search
   // for them by their start addresses (since none will be ExitBB/UnanalyzableBB)
   const basicblock_t::ConstSuccIter start=fn.getBasicBlockFromId(bb_id)->beginSuccIter();
   const basicblock_t::ConstSuccIter finish=fn.getBasicBlockFromId(bb_id)->endSuccIter();
   for (basicblock_t::ConstSuccIter succiter=start; succiter != finish; ++succiter) {
      bbid_t succbb_id = *succiter;
      const basicblock_t *succbb = fn.getBasicBlockFromId(succbb_id);
      const kptr_t succbbStartAddr = succbb->getStartAddr();

      bbid_t searched_succbb_id = fn.searchForBB(succbbStartAddr);
      assert(searched_succbb_id == succbb_id);
   }

   bbid_t succ_bb_id = fn.searchForBB(theDestAddr);
   if (succ_bb_id == (bbid_t)-1) {
      // theDestAddr doesn't lie w/in an existing bb; recurse.
      basicblock_t *newbb = basicblock_t::getBasicBlock(theDestAddr, bb_id);
      succ_bb_id = fn.addBlock_new(newbb);

      fn.addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id);
      // won't be a duplicate since it didn't exist anywhere til now.

      fn.parse(succ_bb_id,
		simplePath(pathFromEntryBB, bb_id), // new path
		this);

      assert(fn.searchForBB(theDestAddr) == succ_bb_id);
   }
   else if (fn.getBasicBlockFromId(succ_bb_id)->getStartAddr()==theDestAddr) {
      // theDestAddr lies at the start of an existing bb; use it.
      // There's a very real chance of this bb already being a child; must weed
      // out duplicate successors.
      if (fn.addSuccessorToBlockByIdUnlessDup(bb_id, succ_bb_id)) {
         if (!fn.addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
            assert(false);
      }
      else
         assert(fn.getBasicBlockFromId(bb_id)->hasAsASuccessor(succ_bb_id));
   }
   else {
      // theDestAddr lies in the middle of an existing bb; split it.
      //	 cout << "jump table entry " << addr2hex(theDestAddr) << " being added via splitting its existing bb" << endl;

      // assert that we are not splitting ourselves
      assert(succ_bb_id != bb_id);

      succ_bb_id = fn.splitBasicBlock(succ_bb_id, theDestAddr);

      // won't be a dup since this bb didn't exist until the split
      if (!fn.addSuccessorToBlockByIdUnlessDup(bb_id, succ_bb_id))
         assert(false);
      if (!fn.addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
         assert(false);
   }

}


void cfgCreationOracle::
completeTaggedJumpTable32(bbid_t bb_id, 
			  kptr_t /*jumpTableStartAddr*/) {
   //  if (verbose_fnParse)
//        cout << "Done processing tagged-32 jump table starting at "
//             << addr2hex(jumpTableStartAddr) << "; it contains "
//             << fn.getBasicBlockFromId(bb_id)->numSuccessors()
//             << " non-duplicate entries" << endl;


   theBBParsingStats.numTaggedJumpTables++;
   theBBParsingStats.numTaggedJumpTableTotalNonDupEntries += fn.getBasicBlockFromId(bb_id)->numSuccessors();
}
