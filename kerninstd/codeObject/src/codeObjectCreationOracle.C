// codeObjectCreationOracle.C

#include "codeObjectCreationOracle.h"
#include "fnCodeObjects.h"
#include <algorithm> // STL find()
#include "util/h/minmax.h"
#include "util/h/hashFns.h"
#include "util/h/out_streams.h"
#include "simplePath.h"

#ifdef sparc_sun_solaris2_7
#include "sparc_codeObjectCreationOracle.h"
#include "pcIndepWithFallThruCodeObject.h"
#include "pcIndepNoFallThruCodeObject.h"
#include "interProcCallCodeObject.h"
#include "interProcTailCallCodeObject.h"
#include "callToObtainPCCodeObject.h"
#include "recursiveCallCodeObject.h"
#include "condBranchCodeObject.h"
#include "branchAlwaysCodeObject.h"
#include "interProcCondBranchCodeObject.h"
#include "interProcBranchAlwaysCodeObject.h"
#include "offsetJumpTable32CodeObject.h"
#include "simpleJumpTable32CodeObject.h"
#include "intraProcJmpToFixedAddrCodeObject.h"
#endif

typedef parseOracle::bbid_t bbid_t;

codeObjectCreationOracle::
codeObjectCreationOracle(const moduleMgr &imoduleMgr,
                         function_t &ifn,
                         fnCodeObjects &ifnCodeObjects,
                         const pdvector<fnAddrBeingRelocated> &ifnAddrsBeingRelocated,
                         bool iverbose_fnParse) : parseOracle(ifn), 
   theModuleMgr(imoduleMgr),
   theFnCodeObjects(ifnCodeObjects),
   fnsBeingRelocated(addrHash4),
   verbose_fnParse(iverbose_fnParse) {

   pdvector<fnAddrBeingRelocated>::const_iterator iter = ifnAddrsBeingRelocated.begin();
   pdvector<fnAddrBeingRelocated>::const_iterator finish = ifnAddrsBeingRelocated.end();
   for (; iter != finish; ++iter) {
      const fnAddrBeingRelocated &fa = *iter;
      fnsBeingRelocated.set(fa.first, fa.second);
   }
}

codeObjectCreationOracle::~codeObjectCreationOracle() {
   pdvector<jumpTableCodeObject*>::iterator iter = allJumpTableCodeObjects.begin();
   pdvector<jumpTableCodeObject*>::iterator finish = allJumpTableCodeObjects.end();
   for (; iter != finish; ++iter) {
      jumpTableCodeObject *co = *iter;
      delete co;
   }
}

codeObjectCreationOracle * 
codeObjectCreationOracle::getCodeObjectCreationOracle(const moduleMgr &imoduleMgr,
						      function_t &ifn,
						      fnCodeObjects &ifnCodeObjects,
						      const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated,
						      bool verbose_fnParse) {
  codeObjectCreationOracle *co = NULL;
#ifdef sparc_sun_solaris2_7
  co = new sparc_codeObjectCreationOracle(imoduleMgr, ifn, ifnCodeObjects,
					  fnAddrsBeingRelocated, 
					  verbose_fnParse);
#endif
  return co;
}

void codeObjectCreationOracle::
ensureBlockStartInsnIsAlsoBlockOnlyInsn(kptr_t addr,
                                        instr_t *i) const {
   // unlike the cfg parsing oracle, this version simply asserts, it doesn't
   // make it so.  That's because this class cannot in good conscience
   // muck with the structure of an already-parsed function.
   bbid_t bbid = fn.searchForBB(addr);
   assert(bbid != (bbid_t)-1);
   
   const basicblock_t *bb = fn.getBasicBlockFromId(bbid);
   
   assert(bb->getStartAddr() == addr);
   assert(bb->getNumInsnBytes() == i->getNumBytes());
}

std::pair<unsigned, unsigned>
codeObjectCreationOracle::
getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(bbid_t bbid,
                                                    unsigned insnByteOffsetWithinBB) const {
   return theFnCodeObjects.
      getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(&fn, bbid,
                                                          insnByteOffsetWithinBB);
}

void codeObjectCreationOracle::
appendCodeObjectToBasicBlock(bbid_t bb_id,
                             unsigned byteOffsetWithinBB,
                             codeObject *co) const {
   assert(byteOffsetWithinBB < fn.getBasicBlockFromId(bb_id)->getNumInsnBytes());
   
   theFnCodeObjects.appendToBasicBlock(bb_id, byteOffsetWithinBB, co);
}

void codeObjectCreationOracle::endBasicBlock(bbid_t bb_id) const {
   theFnCodeObjects.endBasicBlock(bb_id);
}

void codeObjectCreationOracle::parseBlockIfNeeded(bbid_t bb_id,
                                                  const simplePath &thePath) {
   theFnCodeObjects.parseBlockIfNeeded(bb_id, thePath, this);
}


void codeObjectCreationOracle::
processSeveralNonControlFlowInsnsAsInsnVecSubset(bbid_t bb_id,
                                                 kptr_t startAddr,
                                                 unsigned numInsns) const {
#ifdef sparc_sun_solaris2_7
   insnVec_t *theInsnVecSubset = insnVec_t::getInsnVec(); // initially empty

   kptr_t myAddr = startAddr;
   while (numInsns--) {
      instr_t *i = instr_t::getInstr(*(fn.get1OrigInsn(myAddr)));
         // TODO: optimize: since this sequence of insns won't cross a bb
         // boundary, it won't cross a function codeChunk boundary, so we
         // can be a bit quicker than fn.get1OrigInsn()
      theInsnVecSubset->appendAnInsn(i);

      myAddr += i->getNumBytes();
   }

   kptr_t startAddrOfLastInsnInObject = startAddr;
   if (theInsnVecSubset->numInsns() > 0)
      startAddrOfLastInsnInObject += theInsnVecSubset->insnNdx2ByteOffset(theInsnVecSubset->numInsns()-1);

   regset_t *availRegsAfterCodeObject = 
      theModuleMgr.getDeadRegsAfterInsnAddr(fn, startAddrOfLastInsnInObject, false);
      // false --> not verbose

   codeObject *co = new pcIndepWithFallThruCodeObject(theInsnVecSubset,
                                                      availRegsAfterCodeObject);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                startAddr-fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
#endif
}

void codeObjectCreationOracle::
processInterproceduralFallThru(bbid_t bb_id) {
   endBasicBlock(bb_id);
}

void codeObjectCreationOracle::
processStumbleOntoExistingBlock(bbid_t bb_id,
                                bbid_t stumble_bb_id,
                                const simplePath &thePath) {
   // End the present basic block, and then parse the stumblee if needed.
   // Just because it's known that we stumble onto this next block does NOT mean
   // that we have already processed it here, only that cfg parsing long long ago
   // already processed it.  We still need to process it!
   endBasicBlock(bb_id);

   const simplePath newPath(thePath, bb_id); // thePath + bb_id
   parseBlockIfNeeded(stumble_bb_id, newPath);
}

void codeObjectCreationOracle::
processCallToObtainPC(bbid_t bb_id,
                      instr_t *callInsn,
                      kptr_t callInsnAddr,
                      bool delaySlotInSameBlock,
                      instr_t *dsInsn) {
#ifdef sparc_sun_solaris2_7
   // the delay instruction will be processed as we continue to parse this
   // basic block.
   codeObject *co = new callToObtainPCCodeObject(delaySlotInSameBlock, dsInsn);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                callInsnAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   // do NOT call endBasicBlock; there will be more codeObjects for it, since we're
   // gonna continue to parse that block.

   delete callInsn;
#endif
}

void codeObjectCreationOracle::
noteHiddenIntraProceduralFn(kptr_t /*addr*/) {
   // nothing needed.  The actual call will still be processed as such.
}

void codeObjectCreationOracle::
processTrueCallRestore(bbid_t bb_id,
                       instr_t *callInstr,
                       kptr_t instrAddr,
                       kptr_t destAddr,
                       bool /*nonTrivialRestore*/,
                       bool /*dangerRestore*/,
                          // dangerous if rd isn't %o0 thru %o5 or %g; we won't be
                          // able to unwind it.
                       const reg_t */*regWrittenByRestore*/,
                       bool delaySlotInSameBlock,
                       instr_t *dsInsn) {
#ifdef sparc_sun_solaris2_7
   // note that call; restore isn't treated quite the same as any old call; delay
   // because for call;restore, we're sure that it won't fall thru.  Hence a distinct
   // codeObject.

   assert(dsInsn->isRestore());

   codeObject *co = NULL;
   if (fnsBeingRelocated.defines(destAddr))
      co = new interProcTailCallToNameCodeObject(fnsBeingRelocated.get(destAddr),
                                                 delaySlotInSameBlock,
                                                 dsInsn);
   else
      co = new interProcTailCallToAddrCodeObject(destAddr,
                                                 delaySlotInSameBlock,
                                                 dsInsn);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                instrAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);

   delete callInstr;
#endif
}

void codeObjectCreationOracle::
processJmplCallRestore(bbid_t bb_id, instr_t *callInstr,
		      kptr_t instrAddr,
		      bool /*nonTrivialRestore*/,
		      bool /*dangerousRestore*/,
		      const reg_t */*rdOfRestore*/,
		      bool delaySlotInSameBlock,
		      instr_t *dsInsn) {
#ifdef sparc_sun_solaris2_7
   insnVec_t *theInsnVec = insnVec_t::getInsnVec();
   theInsnVec->appendAnInsn(callInstr);
   if (delaySlotInSameBlock)
      theInsnVec->appendAnInsn(dsInsn);
   else
      delete dsInsn;

   codeObject *co = new pcIndepNoFallThruCodeObject(theInsnVec);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                instrAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);
#endif
}

void codeObjectCreationOracle::
processNonTailOptimizedTrueCall(bbid_t bb_id,
                                instr_t *callInstr,
                                kptr_t instrAddr,
                                kptr_t destAddr,
                                bool delaySlotInSameBlock,
                                instr_t *dsInsn) {
#ifdef sparc_sun_solaris2_7
   assert(delaySlotInSameBlock); // messy to outline otherwise
   regset_t *availRegsAfterCodeObject = 
      theModuleMgr.getDeadRegsAfterInsnAddr(fn, instrAddr+callInstr->getNumBytes(), false);
      // after the delay slot, too
      // false --> not verbose

   codeObject *co = NULL;
   if (fnsBeingRelocated.defines(destAddr))
      co = new interProcCallToNameCodeObject(fnsBeingRelocated.get(destAddr),
                                             delaySlotInSameBlock,
                                             dsInsn,
                                             availRegsAfterCodeObject,
                                             false);
         // false --> the call will return (as usual)
   else
      co = new interProcCallToAddrCodeObject(destAddr,
                                             delaySlotInSameBlock,
                                             dsInsn,
                                             availRegsAfterCodeObject,
                                             false);
         // false --> the call will return (as usual)
   assert(co);

   appendCodeObjectToBasicBlock(bb_id,
                                instrAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   // do NOT call endBasicBlock(bb_id)

   delete callInstr;
#endif
}
   
void codeObjectCreationOracle::
processNonTailOptimizedJmplCall(bbid_t bb_id,
			       instr_t *callInstr,
			       kptr_t instrAddr,
			       bool delaySlotInSameBlock,
			       instr_t *dsInsn) {
#ifdef sparc_sun_solaris2_7
   // as usual with jmpl, it's trivially relocatable:

   insnVec_t *theInsnVec = insnVec_t::getInsnVec();
   theInsnVec->appendAnInsn(callInstr);
   if (delaySlotInSameBlock)
      theInsnVec->appendAnInsn(dsInsn);
   else
      delete dsInsn;

   assert(delaySlotInSameBlock); // outlining would be messy otherwise
   regset_t *availRegsAfterCodeObject = 
      theModuleMgr.getDeadRegsAfterInsnAddr(fn, instrAddr+callInstr->getNumBytes(), false);
      // after the delay slot, too
      // false --> not verbose

   codeObject *co = new pcIndepWithFallThruCodeObject(theInsnVec,
                                                      availRegsAfterCodeObject);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                instrAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);

   // do NOT call endBasicBlock(bb_id)
#endif
}

void codeObjectCreationOracle::
processReturn(bbid_t bb_id, instr_t *insn, kptr_t insnAddr,
	      bool delaySlotInSameBlock, instr_t *dsInsn) {
#ifdef sparc_sun_solaris2_7   
   insnVec_t *theInsnVec = insnVec_t::getInsnVec();
   theInsnVec->appendAnInsn(insn);
   if (delaySlotInSameBlock)
      theInsnVec->appendAnInsn(dsInsn);
   else
      delete dsInsn;

   codeObject *co = new pcIndepNoFallThruCodeObject(theInsnVec);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                insnAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);
#endif
}

void codeObjectCreationOracle::
processJmpBehavingAsRet(bbid_t bb_id, instr_t *insn,
			kptr_t insnAddr,
			bool delaySlotInSameBlock,
			instr_t *dsInsn) {
#ifdef sparc_sun_solaris2_7
   // jmp insn is trivially relocatable:

   insnVec_t *theInsnVec = insnVec_t::getInsnVec();
   theInsnVec->appendAnInsn(insn);
   if (delaySlotInSameBlock)
      theInsnVec->appendAnInsn(dsInsn);
   else
      delete dsInsn;

   codeObject *co = new pcIndepNoFallThruCodeObject(theInsnVec);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                insnAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);
#endif
}

void codeObjectCreationOracle::
processDoneOrRetry(bbid_t bb_id, instr_t *insn, kptr_t insnAddr) {
#ifdef sparc_sun_solaris2_7
   // trivially relocatable:

   insnVec_t *theInsnVec = insnVec_t::getInsnVec();
   theInsnVec->appendAnInsn(insn);

   codeObject *co = new pcIndepNoFallThruCodeObject(theInsnVec);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                insnAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);
#endif
}

void codeObjectCreationOracle::
processCondBranch(bbid_t bb_id,
                  bool branchAlways, bool branchNever,
                  instr_t *i,
                  instr_t::controlFlowInfo *cfi,
                  kptr_t instrAddr,
                  bool hasDelaySlotInsn,
                  bool delaySlotInThisBlock,
                  instr_t *dsInsn,
                  kptr_t fallThroughAddr,
                  kptr_t destAddr,
                  const simplePath &pathFromEntryBB) {
   // NOTE: The name of this method is misleading; it should be "processBranch"
   // without "Cond" in the name, because to my mind, "cond" means a truly
   // conditional branch, without the possibility of "always" or "never".
   if (!branchAlways && !branchNever)
      assert(hasDelaySlotInsn && "cond branches always have delay slot instructions");
   
   // This routine handles both intra-procedural (the usual case) and
   // interprocedural branches.  Why aren't there two separate methods for
   // these cases?  (Probably because the parsing code that calls us isn't always
   // certain when a branch is interprocedural, and even if it could make a darn
   // good guess, this class can do better, since it always knows precise function
   // bounds...compare to an oracle for creating cfg's, which isn't so sure)

//     const bool delaySlotInItsOwnBlock =
//        hasDelaySlotInsn && !delaySlotInThisBlock &&
//        cfi.delaySlot == sparc_instr::dsWhenTaken &&
//        fn.getBasicBlockFromId(fn->searchForBB(instrAddr+4)).getNumBytes() == 4 &&
//        (dsInsn.isSave() || dsInsn.isRestore());
   
   if (!fn.getOrigCode().enclosesAddr(destAddr))
      processInterProcCondBranch(bb_id, branchAlways, branchNever,
                                 i, cfi, instrAddr,
                                 hasDelaySlotInsn,
                                 delaySlotInThisBlock,
                                 dsInsn, fallThroughAddr,
                                 destAddr, pathFromEntryBB);
   else
      processIntraProcCondBranch(bb_id, branchAlways, branchNever, i, cfi,
                                 instrAddr,
                                 hasDelaySlotInsn,
                                 delaySlotInThisBlock,
                                 dsInsn, fallThroughAddr, destAddr, pathFromEntryBB);
}

void codeObjectCreationOracle::
processIntraProcCondBranch(bbid_t bb_id,
                           bool branchAlways, bool branchNever,
                           instr_t *i,
                           instr_t::controlFlowInfo *cfi,
                           kptr_t instrAddr,
                           bool hasDelaySlotInsn,
                           bool delaySlotInThisBlock,
                           instr_t *dsInsn,
                           kptr_t fallThroughAddr,
                           kptr_t destAddr,
                           const simplePath &pathFromEntryBB) {
#ifdef sparc_sun_solaris2_7
   // NOTE: The name of this method is misleading; it should not contain "Cond"
   // in the name, because to my mind, "cond" means a truly
   // conditional branch, without the possibility of "always" or "never".
   if (!branchAlways && !branchNever)
      assert(hasDelaySlotInsn && "cond branches always have a delay slot insn");

   // intra-procedural branches are the usual case
   assert(fn.containsAddr(destAddr));
   
   const bbid_t iftaken_bb_id = fn.searchForBB(destAddr);
   assert(iftaken_bb_id != (bbid_t)-1 &&
          "codeObjectCreationOracle::processCondBranch() -- couldn't find dest block!");
   
   assert(fn.getBasicBlockFromId(iftaken_bb_id)->getStartAddr() == destAddr &&
          "codeObjectCreationOracle::processCondBranch() -- dest address isn't at the start of a basic block; should be.");

   regset_t *availRegsBefore =
      theModuleMgr.getDeadRegsBeforeInsnAddr(fn, instrAddr, false); // not verbose
   regset_t *availRegsAfter =
      theModuleMgr.getDeadRegsAfterInsnAddr(fn, hasDelaySlotInsn ? instrAddr + i->getNumBytes() : instrAddr, false); // not verbose
   
   codeObject *co = NULL;
   if (branchAlways) {
      co = new branchAlwaysCodeObject(iftaken_bb_id,
                                      i,
                                      hasDelaySlotInsn,
                                      delaySlotInThisBlock,
                                      dsInsn,
                                      availRegsBefore,
                                      availRegsAfter);
      delete cfi; 
   }
   else if (branchNever)
      assert(false && "codeObjectCreationOracle::processCondBranch() -- branchNever nyi");
   else
      co = new condBranchCodeObject(iftaken_bb_id,
                                    i, cfi,
                                    delaySlotInThisBlock,
                                    dsInsn,
                                    availRegsBefore,
                                    availRegsAfter);
   assert(co);
 
   appendCodeObjectToBasicBlock(bb_id,
                                instrAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);

   // If the delay slot insn is in a different basic block, then recursively
   // parse that basic block, lest we get tripped up later on with some kind
   // of "that basic block has not been complete()'d" check.  There are 2 cases:
   // 1) The delay slot is in another block because it's a save or restore.
   //    For reg analysis reasons, putting in its own block is necessary.
   //    Note that in this case, when we emit code, it's quite possible that the
   //    "parent" block will emit its own copy of the save/restore, because
   //    if that other block was re-ordered, it wouldn't be in the right place
   //    anymore.  That'll probably lead to the save/restore being emitted twice,
   //    but harmlessly.  Note that in the future, it's possible that we'll put
   //    all conditionally annulled delay slot instructions in their own basic
   //    block...the only reason we don't do this right now is fear of mem explosion.
   // 2) The delay slot is in another basic block because, yes indeed, it's reachable
   //    by other code.  I believe that in Solaris7/32-bit, this happens just 4
   //    times, plus a few more "times" due to quirks in jump table parsing when the
   //    bounds are unknown ahead of time.  In this case, duplicating the delay slot
   //    when we emit code is absolutely desirable.

   if (hasDelaySlotInsn && !delaySlotInThisBlock)
      processBranchDelayInDifferentBlock(instrAddr, bb_id, iftaken_bb_id,
                                         pathFromEntryBB);

   // Now we need to recursively parse the if-taken and if-not-taken cases,
   // if needed.

   // if-taken basic block:
   if (!branchNever) {
      const simplePath newPath(pathFromEntryBB, bb_id); // pathFromEntryBB + bb_id
      parseBlockIfNeeded(iftaken_bb_id, newPath);
   }

   if (!branchAlways) {
      // There is a fall-thru; perhaps intraprocedural, perhaps interprocedural.
      if (fn.containsAddr(fallThroughAddr)) {
         const bbid_t fallee_bb_id = fn.searchForBB(fallThroughAddr);
         assert(fallee_bb_id != (bbid_t)-1);
         assert(fn.getBasicBlockFromId(fallee_bb_id)->getStartAddr()==fallThroughAddr);
            // should be the start of the basic block

         const simplePath newPath(pathFromEntryBB, bb_id); // pathFromEntryBB + bb_id
         parseBlockIfNeeded(fallee_bb_id, newPath);
      }
      else {
         // interprocedural.  Don't parse the fall-thru.
         assert(fn.searchForBB(fallThroughAddr) == (bbid_t)-1);
      }
   }
#endif
}

void codeObjectCreationOracle::
processInterProcCondBranch(bbid_t bb_id,
                           bool branchAlways, bool branchNever,
                           instr_t *i,
                           instr_t::controlFlowInfo *cfi,
                           kptr_t instrAddr,
                           bool hasDelaySlotInsn,
                           bool delaySlotInThisBlock,
                           instr_t *dsInsn,
                           kptr_t fallThroughAddr,
                           kptr_t destAddr,
                           const simplePath &pathFromEntryBB) {
#ifdef sparc_sun_solaris2_7
   // NOTE: The name of this method is misleading; it should not contain "Cond"
   // in the name, because to my mind, "cond" means a truly
   // conditional branch, without the possibility of "always" or "never".
   if (!branchAlways && !branchNever)
      assert(hasDelaySlotInsn && "cond branches always have delay slot instructions");
   
   if (!delaySlotInThisBlock) {
      dout << "WARNING: codeObjectCreationOracle processInterProcCondBranch() found a delay slot in a different block (or different fn); untested." << endl;
//        assert(delaySlotInThisBlock &&
//               "codeObjectCreationOracle::processCondBranch() -- delay slot in a different block nyi");
   }

   regset_t *availRegsBefore =
      theModuleMgr.getDeadRegsBeforeInsnAddr(fn, instrAddr, false); // not verbose
   regset_t *availRegsAfterBranchInsn =
      theModuleMgr.getDeadRegsAfterInsnAddr(fn, instrAddr, false); // not verbose
   regset_t *availRegsAfter =
      theModuleMgr.getDeadRegsAfterInsnAddr(fn, hasDelaySlotInsn ? instrAddr + i->getNumBytes() : instrAddr, false); // not verbose
   
   codeObject *co = NULL;
   if (branchAlways) {
      if (fnsBeingRelocated.defines(destAddr))
         co = new interProcBranchAlwaysToNameCodeObject(false, // temp dummy param
                                                        fnsBeingRelocated.get(destAddr),
                                                        destAddr, // dest original addr
                                                        // this param is used in a call
                                                        // to outliningLocations::
                                                        // willInterProcBranchStillFit()
                                                        i, hasDelaySlotInsn,
                                                        dsInsn, 
                                                        availRegsBefore,
                                                        availRegsAfter);
      else
         co = new interProcBranchAlwaysToAddrCodeObject(destAddr, i, 
							hasDelaySlotInsn,
                                                        dsInsn,
                                                        availRegsBefore,
                                                        availRegsAfter);
   }
   else if (branchNever)
      assert(false && "codeObject parsing: interProc branchNever nyi");
   else {
      // a truly conditional branch (not branchAlways or branchNever)
      if (fnsBeingRelocated.defines(destAddr))
         co = new interProcCondBranchToNameCodeObject(fnsBeingRelocated.get(destAddr),
                                                      destAddr, // dest original addr
                                                      // this param is used in a call
                                                      // to outliningLocations::
                                                      // willInterProcBranchStillFit()
                                                      i, hasDelaySlotInsn,
                                                      dsInsn, 
                                                      availRegsBefore,
                                                      availRegsAfterBranchInsn,
                                                      availRegsAfter);
      else
         co = new interProcCondBranchToAddrCodeObject(destAddr, i, 
						      hasDelaySlotInsn,
                                                      dsInsn,
                                                      availRegsBefore,
                                                      availRegsAfterBranchInsn,
                                                      availRegsAfter);
   }
   
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                instrAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);
   
   // If the delay slot insn is in a different basic block, then recursively
   // parse that basic block, lest we get tripped up later on with some kind
   // of "that basic block has not been complete()'d" check.  There are 2 cases:
   // 1) The delay slot is in another block because it's a save or restore.
   //    For reg analysis reasons, putting in its own block is necessary.
   //    Note that in this case, when we emit code, it's quite possible that the
   //    "parent" block will emit its own copy of the save/restore, because
   //    if that other block was re-ordered, it wouldn't be in the right place
   //    anymore.  That'll probably lead to the save/restore being emitted twice,
   //    but harmlessly.  Note that in the future, it's possible that we'll put
   //    all conditionally annulled delay slot instructions in their own basic
   //    block...the only reason we don't do this right now is fear of mem explosion.
   // 2) The delay slot is in another basic block because, yes indeed, it's reachable
   //    by other code.  I believe that in Solaris7/32-bit, this happens just 4
   //    times, plus a few more "times" due to quirks in jump table parsing when the
   //    bounds are unknown ahead of time.  In this case, duplicating the delay slot
   //    when we emit code is absolutely desirable.

   if (hasDelaySlotInsn && !delaySlotInThisBlock)
      processBranchDelayInDifferentBlock(instrAddr, bb_id,
                                         (bbid_t)-1, // --> inter-procedural branch
                                         pathFromEntryBB);

   // If this were an intra-procedural branch, we'd follow the if-taken case here.
   // But it's not, so we don't.
   
   // ...and now handle the fall-thru case:
   // NOTE: this code is copied verbatim from processIntraProcCondBranch(); perhaps
   // we should combine code.
   // NOTE: if the fall-thru is interprocedural (stumbles onto another function),
   // then we (intentionally) don't parse the fallee.  Note that no info is lost by
   // doing this, nor do we need to pass "hey, this conditional branch has an
   // interprocedural fallee" to the above code object constructors, because the
   // fallee information is always passed later on, when emitting code (more specially,
   // in the case of an interprocedural fallee, the fallee block id will be (bbid_t)-1,
   // which is enough to identify it as an interproc fallee, and the fn object can then
   // be queried to find out the original fallee, which can then be translated to
   // the (perhaps new due to relocation) fallee by consulting outliningLocations
   // object.)
   if (!branchAlways) {
      // there is a fall-thru; perhaps intraprocedural, perhaps interprocedural.
      if (fn.containsAddr(fallThroughAddr)) {
         // intraprocedural, the usual case.
         const bbid_t fallee_bb_id = fn.searchForBB(fallThroughAddr);
         assert(fallee_bb_id != (bbid_t)-1);
         assert(fn.getBasicBlockFromId(fallee_bb_id)->getStartAddr()==fallThroughAddr);
            // should be the start of the basic block

         const simplePath newPath(pathFromEntryBB, bb_id); // pathFromEntryBB + bb_id
         parseBlockIfNeeded(fallee_bb_id, newPath);
      }
      else {
         // interprocedural.  don't parse fall-thru, as explained above.
         assert(fn.searchForBB(fallThroughAddr) == (bbid_t)-1);
      }
   }

   delete cfi;
#endif
}

void codeObjectCreationOracle::
processJmplRestoreOptimizedTailCall(bbid_t bb_id,
                                   instr_t *insn,
                                   kptr_t insnAddr,
                                   bool delaySlotInSameBlock,
                                   instr_t *dsInsn) const {
#ifdef sparc_sun_solaris2_7
   // jmpl insns are trivially relocatable:
   
   insnVec_t *theInsnVec = insnVec_t::getInsnVec();
   theInsnVec->appendAnInsn(insn);
   if (delaySlotInSameBlock)
      theInsnVec->appendAnInsn(dsInsn);
   else
      delete dsInsn;

   codeObject *co = new pcIndepNoFallThruCodeObject(theInsnVec);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                insnAddr - fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);
#endif
}

void codeObjectCreationOracle::
processIndirectTailCall(bbid_t bb_id, instr_t *insn,
			kptr_t insnAddr,
			bool delaySlotInSameBlock,
			instr_t *dsInsn) const {
#ifdef sparc_sun_solaris2_7
   // jmpl insns are trivially relocatable:
   
   insnVec_t *theInsnVec = insnVec_t::getInsnVec();
   theInsnVec->appendAnInsn(insn);
   if (delaySlotInSameBlock)
       theInsnVec->appendAnInsn(dsInsn);
   else 
       delete dsInsn;

   codeObject *co = new pcIndepNoFallThruCodeObject(theInsnVec);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                insnAddr - fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);
#endif
}

void codeObjectCreationOracle::
process1SimpleJumpTable32Entry(bbid_t our_block_id, // uniquely id's the jmp table
                               kptr_t nextJumpTableEntry,
                               const simplePath &pathFromEntryBB) {
#ifdef sparc_sun_solaris2_7
   bbid_t destBlockId = fn.searchForBB(nextJumpTableEntry);
   assert(destBlockId != (bbid_t)-1);
      // a junk "nexstJumpTableEntry" will fail this test, so the caller must be sure
      // to do some sanity checking and be willing to terminate the jump table.
   assert(fn.getBasicBlockFromId(destBlockId)->getStartAddr() == nextJumpTableEntry);
      // should be at the start of a basic block already.  Even if we parse too far
      // and include someone else's jump table stuff, this will still hold.

   // get the simpleJumpTable32CodeObject and add destBlockId if not a duplicate.

   // Get the code object: will be the last code object in bb "our_block_id"
   const basicBlockCodeObjects &ourBBCodeObjects =
      theFnCodeObjects.getCodeObjectsForBlockById(our_block_id);
   const std::pair<unsigned, codeObject *> &lastCodeObjInOurBBInfo = ourBBCodeObjects.back();
      // .first gives us the byte offset, though we don't have any use for it

   simpleJumpTable32CodeObject *co = dynamic_cast<simpleJumpTable32CodeObject*>(lastCodeObjInOurBBInfo.second);
   assert(co != NULL); // NULL if the dynamic_cast failed for any reason
   const bool duplicateEntry = co->addAnEntry(destBlockId);

   if (!duplicateEntry) {
      const simplePath newPath(pathFromEntryBB, our_block_id);
      parseBlockIfNeeded(destBlockId, newPath);
   }
#endif
}

void codeObjectCreationOracle::
completeSimpleJumpTable32(bbid_t /*bb_id*/, kptr_t /*jumpTableStartAddr*/) {
   // nothing needed
}

// ----------------------------------------------------------------------

void codeObjectCreationOracle::
prepareTaggedJumpTable32(bbid_t /*bb_id*/,
                         kptr_t /*jumpTableStartAddr*/,
                         instr_t */*jmpInsn*/,
                         bool /*delaySlotInThisBlock*/,
                         instr_t */*delaySlotInsn*/) const {
   assert(false && "nyi");
}

void codeObjectCreationOracle::
process1TaggedJumpTable32Entry(bbid_t /*bb_id*/,
                               uint32_t /*theTag*/,
                               kptr_t /*theDestAddr*/,
                               const simplePath &/*pathFromEntryBB*/) {
   assert(false && "nyi");
}

void codeObjectCreationOracle::
completeTaggedJumpTable32(bbid_t /*bb_id*/, 
			  kptr_t /*jumpTableStartAddr*/) {
   assert(false && "nyi");
}

// --------------------

void codeObjectCreationOracle::
adjustForNewJumpTableStartAddr(kptr_t jumpTableStartAddr) {
   // Assert that we haven't seen this jump table addr yet:
   // allJumpTableStartAddrs[] is kept sorted, so we can use binary_search()
   // instead of find():
   if (std::binary_search(allJumpTableStartAddrs.begin(), allJumpTableStartAddrs.end(),
                          jumpTableStartAddr))
      assert(false);

   // Add to allJumpTableStartAddrs[] and re-sort (this vec must be kept sorted).
   allJumpTableStartAddrs += jumpTableStartAddr;
   std::sort(allJumpTableStartAddrs.begin(), allJumpTableStartAddrs.end());

   // Now possibly adjust the bounds of other jump tables that we've already
   // tentatively processed:
   pdvector<jumpTableCodeObject*>::const_iterator iter = allJumpTableCodeObjects.begin();
   pdvector<jumpTableCodeObject*>::const_iterator finish = allJumpTableCodeObjects.end();
   for (; iter != finish; ++iter) {
      jumpTableCodeObject *co = *iter;
      co->maybeAdjustJumpTableSize(jumpTableStartAddr);
   }
}
