#include "sparc_codeObjectCreationOracle.h"

#include "interProcCallCodeObject.h"
#include "interProcTailCallCodeObject.h"
#include "intraProcJmpToFixedAddrCodeObject.h"
#include "offsetJumpTable32CodeObject.h"
#include "pcIndepNoFallThruCodeObject.h"
#include "recursiveCallCodeObject.h"
#include "simpleJumpTable32CodeObject.h"
#include "fnCodeObjects.h"

#include "out_streams.h"
#include "simplePath.h"
#include "util/h/minmax.h"

typedef parseOracle::bbid_t bbid_t;

void sparc_codeObjectCreationOracle::
processCallToRoutineReturningOnCallersBehalf(bbid_t bb_id,
                                             instr_t */*callInstr*/,
                                             kptr_t callInsnAddr,
                                             kptr_t destAddr,
                                             bool delaySlotInSameBlock,
                                             instr_t *delaySlotInsn) {
   regset_t *availRegsAfterCodeObject =
      theModuleMgr.getDeadRegsAfterInsnAddr(fn, callInsnAddr + 4, false);
      // +4 to account for the delay slot, which always exists.
      // false --> not verbose

   codeObject *co = NULL;
   if (fnsBeingRelocated.defines(destAddr))
      co = new interProcCallToNameCodeObject(fnsBeingRelocated.get(destAddr),
                                             delaySlotInSameBlock,
                                             delaySlotInsn,
                                             availRegsAfterCodeObject,
                                             true);
      // true --> the call won't return
   else
      co = new interProcCallToAddrCodeObject(destAddr, delaySlotInSameBlock,
                                             delaySlotInsn,
                                             availRegsAfterCodeObject,
                                             true);
         // true --> the call won't return
   assert(co);

   appendCodeObjectToBasicBlock(bb_id,
                                callInsnAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);

   endBasicBlock(bb_id);
}

void sparc_codeObjectCreationOracle::
processCallRecursion(bbid_t bb_id,
                     kptr_t /*calleeAddr*/,
                     instr_t */*callInstr*/,
                     kptr_t instrAddr,
                     bool delaySlotInSameBlock,
                     instr_t *dsInsn,
                     const simplePath &pathFromEntryBB
                     ) {
   regset_t *availRegsAfterCodeObject =
      theModuleMgr.getDeadRegsAfterInsnAddr(fn, instrAddr + 4, false);
      // +4 to account for the delay slot, which always exists.
      // false --> not verbose
   
   codeObject *co = new recursiveCallCodeObject(fn.xgetEntryBB(),
                                                delaySlotInSameBlock, dsInsn,
                                                availRegsAfterCodeObject);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                instrAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);

   endBasicBlock(bb_id);
   
   // here is where we would recursively parse the callee, but I don't see the
   // need; the entry basic block will surely get parsed without our help.

   // Now process the fall-thru of the recursion.  After all, all recursion
   // ends some time, so there is a fall-through case.

   const kptr_t fallThroughAddr = instrAddr + 8;
   if (!fn.containsAddr(fallThroughAddr)) {
      // nothing to do.  Perhaps falls thru to the next fn?
      return;
   }

   bbid_t succ_id = fn.searchForBB(fallThroughAddr);
   assert(succ_id != (bbid_t)-1 && "codeObjectCreationOracle: could not find a block");
   
   if (fn.getBasicBlockFromId(succ_id)->getStartAddr() == fallThroughAddr) {
      // fall-through is at the *start* of an already-existing basic block.
      // Recursively parse it, if needed.
      parseBlockIfNeeded(succ_id, pathFromEntryBB);
   }
   else {
      // fall-through is in the middle of an existing bb.  Can't happen will fall-thru.
      // (See cfg creation oracle for a proof.)  Even if it could, we'd assert fail,
      // since it doubly shouldn't happen in a codeObject.
      assert(false);
   }
}

void sparc_codeObjectCreationOracle::
processTrueCallThatWritesToO7InDelay(bbid_t bb_id,
                                     instr_t */*callInstr*/,
                                     kptr_t instrAddr,
                                     kptr_t destAddr,
                                     bool delaySlotInSameBlock,
                                     instr_t *dsInsn) const {
   // The delay slot instruction isn't a restore, but something close enough, in that
   // it fries %o7 and thus makes the call/dsInsn combo an optimized tail call.
   // The call instruction itself is pretty normal.  For codeObject purposes, we handle
   // it just like call;restore  (not like any ole' call, because here, we're sure
   // that the call won't return, and thus there's no need to fiddle with a fall-thru
   // block).

   codeObject *co = NULL;
   if (fnsBeingRelocated.defines(destAddr))
      co = new interProcTailCallToNameCodeObject(fnsBeingRelocated.get(destAddr),
                                                 delaySlotInSameBlock, dsInsn);
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
}

void sparc_codeObjectCreationOracle::
processJmplThatWritesToO7InDelay(bbid_t bb_id, instr_t *callInstr,
                                 kptr_t instrAddr,
                                 bool delaySlotInSameBlock,
                                 instr_t *dsInsn) const {
   // the delay slot insn isn't a restore, but something close enough, making
   // this jmpl/dsInsn combo an optimized tail call.
   // As usual with jmpl, this is all trivially relocatable, so no special
   // codeObject is required:

   insnVec_t *theInsnVec = insnVec_t::getInsnVec();
   theInsnVec->appendAnInsn(callInstr);
   if (delaySlotInSameBlock)
      theInsnVec->appendAnInsn(dsInsn);

   codeObject *co = new pcIndepNoFallThruCodeObject(theInsnVec);
   assert(co);
   
   appendCodeObjectToBasicBlock(bb_id,
                                instrAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   
   endBasicBlock(bb_id);
}

void sparc_codeObjectCreationOracle::
processBranchDelayInDifferentBlock(kptr_t branchInstrAddr,
                                   bbid_t bb_id,
                                   bbid_t iftaken_bb_id,
                                      // (bbid_t)-1 if branch is inter-procedural
                                      // (who cares, this is just used for an assert
                                      // in this routine anyway)
                                   const simplePath &pathFromEntryBB
                                   ) {
   // odd cases:
   // delay_slot_bb_id could be (bbid_t)-1 --> inter-proc fall-thru
   // iftaken_bb_id could be (bbid_t)-1 --> inter-proc branch

   assert(bb_id != (bbid_t)-1);

   const bbid_t delay_slot_bb_id = fn.searchForBB(branchInstrAddr + 4);
   assert(delay_slot_bb_id != bb_id);

   if (delay_slot_bb_id != (bbid_t)-1)
      assert(delay_slot_bb_id != iftaken_bb_id);

   // Can't do the following asserts because the ds insn could be the dest of
   // a branch (though rare), so the ds insn isn't necessarily in its own 1-insn
   // basic block.
//     assert(fn.getBasicBlockFromId(delay_slot_bb_id).getNumInsnBytes() == 4);
//     assert(dsInsn.isSave() || dsInsn.isRestoreInstr());

   // Recursively parse delay_slot_bb_id, unless it's in another function.
   if (delay_slot_bb_id != (bbid_t)-1) {
      const simplePath newPath(pathFromEntryBB, bb_id); // pathFromEntryBB + bb_id
      parseBlockIfNeeded(delay_slot_bb_id, newPath);
   }
}

void sparc_codeObjectCreationOracle::
processIntraproceduralJmpToFixedAddr(bbid_t bb_id,
                                     instr_t *jmp_insn,
                                     kptr_t jmpInsnAddr,
                                     bool delaySlotInSameBlock,
                                     instr_t *dsInsn,
                                     kptr_t destAddr,
                                     const simplePathSlice_t &jmpRegSlice,
                                        // should include just 2 insns; a sethi
                                        // and a bset/add
                                     const simplePath &pathFromEntryBB
                                     ) {
   // It's kind of tricky to handle this.  Let me show you why.
   // we'll have something like sethi; bset; jmp; delay;
   // The sethi and bset have already been put into some other code object
   // (presumably pcIndepWithFallThru)
   // We need to ruthlessly yank those instructions out of said code object because
   // they must logically be put into this code object, because otherwise the fixed
   // address of the original code won't be changed as needed to reflect relocation.
   // So we need the infrastructure to trim a pcIndep* code object, which shouldn't
   // be too hard on its face.  But consider: (1) the pcIndep code object may now be
   // empty and need to be fried entirely, and (2) due to compiler instruction
   // scheduling, the sethi/bset combo might not even be in this basic block.
   // (That's not as hard as it sounds; we can selectively slice and dice code objects
   // that have long since been processed, since we have access to them via
   // "theFnCodeObjects").
   //
   // Well, never mind all of the above for now.  What we're going to do is keep those
   // sethi/bset instructions in their pcIndep* code objects, and backpatch them.
   // This is modeled after what offsetJumpTable32CodeObject does.

   // So the first thing that we need to do is to obtain a [bbid, codeobject ndx,
   // insn ndx within code object] triple for both the sethi and bset/add
   // instructions.  Only then do we have the needed info to construct an
   // intraProcJmpToFixedAddrCodeObject object.  Obtaining said info is modeled after
   // similar action taken in offset32 jump table parsing.

   jmpRegSlice.beginBackwardsIteration();
   assert(!jmpRegSlice.isEndOfIteration());
   
   const std::pair<bbid_t, unsigned> bsetInsnBlockIdAndInsnNdx =
      jmpRegSlice.getCurrIterationBlockIdAndOffset();
   
   jmpRegSlice.advanceIteration();
   
   const std::pair<bbid_t, unsigned> setHiInsnBlockIdAndInsnNdx =
      jmpRegSlice.getCurrIterationBlockIdAndOffset();
   
   jmpRegSlice.advanceIteration();
   assert(jmpRegSlice.isEndOfIteration());
   
   // We need [bbid, codeobject ndx, insn ndx w/in codeobject] for both
   // the sethi and the bset instruction.  We have [bbid, insn ndx w/in bb] and
   // need to make the conversion.
   const std::pair<unsigned, unsigned>
   codeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj_sethi =
      getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(setHiInsnBlockIdAndInsnNdx.first,
                                                          4*setHiInsnBlockIdAndInsnNdx.second);

   const std::pair<unsigned, unsigned>
   codeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj_bsetOrAdd =
      getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(bsetInsnBlockIdAndInsnNdx.first,
                                                          4*bsetInsnBlockIdAndInsnNdx.second);

   const bbid_t dest_bb_id = fn.searchForBB(destAddr);
   assert(dest_bb_id != (bbid_t)-1);

   codeObject *co = 
      new intraProcJmpToFixedAddrCodeObject(jmp_insn, dest_bb_id, destAddr,
                                            delaySlotInSameBlock, dsInsn,
                                            setHiInsnBlockIdAndInsnNdx.first,
                                               // the bb
                                            codeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj_sethi.first,
                                               // ndx of code object within that bb
                                            codeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj_sethi.second,
                                               // byte offset w/in that code obj of insn
                                            bsetInsnBlockIdAndInsnNdx.first,
                                               // the bb
                                            codeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj_bsetOrAdd.first,
                                               // ndx of code object w/in bb
                                            codeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj_bsetOrAdd.second
                                               // byte offset w/in that code obj of insn
                                            );

   appendCodeObjectToBasicBlock(bb_id,
                                jmpInsnAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);

   endBasicBlock(bb_id);
   
   // Now recursively parse the if-taken destination address
   const bbid_t iftaken_bb_id = fn.searchForBB(destAddr);
   assert(iftaken_bb_id != (bbid_t)-1);
   
   const simplePath newPath(pathFromEntryBB, bb_id); // the '+' constructor
   parseBlockIfNeeded(iftaken_bb_id, newPath);
}


void sparc_codeObjectCreationOracle::
processV9Return(bbid_t bb_id, instr_t *insn,
		kptr_t insnAddr,
		bool delaySlotInSameBlock,
		instr_t *delayInsn) {
   // for the codeObjectCreationOracle, a v9 return is processed
   // the same as any other ret or retl
   processReturn(bb_id, insn, insnAddr, delaySlotInSameBlock, delayInsn);
}

void sparc_codeObjectCreationOracle::
prepareSimpleJumpTable32(bbid_t bb_id,
                         instr_t *jmpInsn,
                         kptr_t jmpInsnAddr,
                         bool delaySlotInThisBlock,
                         instr_t *dsInsn,
                         kptr_t jumpTableDataStartAddr,
                         const simplePathSlice_t &settingJumpTableStartAddrSlice
                         ) {
   // register the jump table's start addr with the oracle, so the previous and
   // perhaps future jump tables can be properly truncated if they stumble onto
   // our data.
   adjustForNewJumpTableStartAddr(jumpTableDataStartAddr);

   const std::pair< triple<bbid_t,unsigned,unsigned>, 
                    triple<bbid_t,unsigned,unsigned> >
     setHiAndBSet_relocatableInfo =
     getSetHiAndBSetOrAddCodeObjRelativeInfo(settingJumpTableStartAddrSlice);

   jumpTableCodeObject *co =
      new simpleJumpTable32CodeObject(jumpTableDataStartAddr,
                                      jmpInsn,
                                      delaySlotInThisBlock, dsInsn,
                                      setHiAndBSet_relocatableInfo.first.first,
                                         // block id
                                      setHiAndBSet_relocatableInfo.first.second,
                                         // code obj ndx w/in bb
                                      setHiAndBSet_relocatableInfo.first.third,
                                         // insn byte offset w/in that code obj
                                      setHiAndBSet_relocatableInfo.second.first,
                                         // the bb
                                      setHiAndBSet_relocatableInfo.second.second,
                                         // ndx of code object w/in bb
                                      setHiAndBSet_relocatableInfo.second.third
                                         // byte offset w/in that code obj of insn
                                      );
   assert(co);

   // Store this jump table code object (keep track of all jump tables, so we can
   // later trim this guy's length, should that prove necessary due to a conflict with
   // another jump table).
   allJumpTableCodeObjects += co; // don't forget this line!
   
   appendCodeObjectToBasicBlock(bb_id,
                                jmpInsnAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   endBasicBlock(bb_id);
}

void sparc_codeObjectCreationOracle::
processNonLoadingBackwardsJumpTable(bbid_t /*bb_id*/,
                                    kptr_t /*lowestDestAddr*/,
                                    kptr_t /*finishDestAddr*/,
                                       // just past, actually (STL iterator style)
                                    instr_t */*jmpInsn*/,
                                    bool /*delaySlotInThisBlock*/,
                                    instr_t */*delaySlotInsn*/,
                                    const simplePathSlice_t &/*finishDestAddrSlice*/,
                                       // actually, this sethi & bset/add slice
                                       // sets a register to the address just past
                                       // the *end* of the range being jumped to
                                    const simplePath &/*pathFromEntryBB*/) {
   // The difficulty with non-loading backwards jump tables: the code completely
   // depends on the destination blocks being 4 bytes apart and *contiguous*.
   // Unfortunately, outlining will reorder blocks so that they're no longer
   // contiguous!  In theory we can adapt by emitting a different kind of jump
   // table (offset32?), but that requires us having complete control over
   // the sethi/bset and the (non-existant in this case) load instruction.
   // An alternative is to be sure that we never reorder certain blocks.
   // Whatever the case, this kind of jump table cannot be implemented yet.

   dout << "processNonLoadingBackwardsJumpTable(): this kind of code object will be hard to implement" << endl;
   theFnCodeObjects.makeUnparsed();
   throw fnCodeObjects::RelocationForThisCodeObjNotYetImpl();

#ifdef notyet   
   // We are certain about both the start and end address of the jump table.
   // "jumpTableEndAddr" is the key here; it's the start of the instruction just
   // past the end of the jump table.  Unfortunately, we cannot be certain that
   // this address is at the start of the basic block (and indeed, in practice,
   // where this is used, blkclr() and hwblkclr(), it's never at the start of a basic
   // block).

   // examine finishDestAddrSlice; it'll be a sethi & bset/add combination
   finishDestAddrSlice.beginBackwardsIteration();
   assert(!finishDestAddrSlice.isEndOfIteration());
   const std::pair<bbid_t, unsigned> bsetOrAddInsnBlockIdAndInsnOffset =
      finishDestAddrSlice.getCurrIterationBlockIdAndOffset();
   finishDestAddrSlice.advanceIteration();
   const std::pair<bbid_t, unsigned> setHiInsnBlockIdAndInsnOffset =
      finishDestAddrSlice.getCurrIterationBlockIdAndOffset();
   finishDestAddrSlice.advanceIteration();
   assert(finishDestAddrSlice.isEndOfIteration());
   
   // Now convert the above (bbid_t, insn offset) pairs to
   // (bbid_t, code object ndx w/in bb, insn byte offset w/in code object)
   // (The bbid_t won't change, so we only bother to calculate the last two:)
   const std::pair<unsigned, unsigned>
      setHiInsn_codeObjNdx_InsnByteOffset =
      getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(setHiInsnBlockIdAndInsnOffset.first,
                                                          4*setHiInsnBlockIdAndInsnOffset.second);
   
   const std::pair<unsigned, unsigned>
      bsetOrAddInsn_codeObjNdx_InsnByteOffset =
      getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(bsetOrAddInsnBlockIdAndInsnOffset.first,
                                                          4*bsetOrAddInsnBlockIdAndInsnOffset.second);

   const bbid_t highestDestAddrBlockId = fn.searchForBB(finishDestAddr);
   assert(highestDestAddrBlockId != (bbid_t)-1);
   const unsigned highestDestAddrByteOffsetWithinBlock =
      finishDestAddr - fn.getBasicBlockFromId(highestDestAddrBlockId).getStartAddr();
   assert(highestDestAddrByteOffsetWithinBlock <
          fn.getBasicBlockFromId(highestDestAddrBlockId).getNumInsnBytes());
   
   codeObject *co =
      new nonLoadingBackwardsJumpTableCodeObject(jmpInsn, delaySlotInThisBlock,
                                                 delaySlotInsn,
                                                 // information about the just-past
                                                 // end of the highest dest addr:
                                                 highestDestAddrBlockId,
                                                 highestDestAddrCodeObjNdxWithinBlock,
                                                 highestDestAddrByteOffsetWithinCodeObj,
                                                 // Information about the sethi/bsetAdd
                                                 // combo that sets a register to the
                                                 // "just-past end of the highest
                                                 // dest addr":
                                                 setHiInsnBlockIdAndInsnOffset.first,
                                                    // bb id
                                                 setHiInsn_codeObjNdx_InsnByteOffset.first,
                                                    // ndx of code obj w/in that bb
                                                 setHiInsn_codeObjNdx_InsnByteOffset.second,
                                                    // byte offset w/in that code obj
                                                 bsetOrAddBlockIdAndInsnOffset.first,
                                                 bsetOrAddInsn_codeObjNdx_InsnByteOffset.first,
                                                    // ndx of code obj w/in that bb
                                                 bsetOrAddInsn_codeObjNdx_InsnByteOffset.second,
                                                    // byte offset w/in that code obj
                                                 );
#endif /* notyet */
   
   assert(false && "nyi");
}

std::pair< triple<bbid_t,unsigned,unsigned>, triple<bbid_t,unsigned,unsigned> >
sparc_codeObjectCreationOracle::
getSetHiAndBSetOrAddCodeObjRelativeInfo(const simplePathSlice_t &theSlice) const {
   theSlice.beginBackwardsIteration();
   assert(!theSlice.isEndOfIteration());
   const std::pair<bbid_t, unsigned> bsetInsnBlockIdAndInsnOffset =
      theSlice.getCurrIterationBlockIdAndOffset();
   theSlice.advanceIteration();
   assert(!theSlice.isEndOfIteration());
   const std::pair<bbid_t, unsigned> setHiInsnBlockIdAndInsnOffset =
      theSlice.getCurrIterationBlockIdAndOffset();
   theSlice.advanceIteration();
   assert(theSlice.isEndOfIteration());
   
   // we have [bbid, insn ndx w/in bb]; convert the latter to a code object ndx
   // w/in bb and insn byte offset w/in the code object.
   const std::pair<unsigned, unsigned>
      coNdxAndByteOffset_sethi =
      getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(setHiInsnBlockIdAndInsnOffset.first,
                                                          4*setHiInsnBlockIdAndInsnOffset.second);

   const std::pair<unsigned, unsigned>
      coNdxAndByteOffset_bset =
      getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(bsetInsnBlockIdAndInsnOffset.first,
                                                          4*bsetInsnBlockIdAndInsnOffset.second);
   
   return std::make_pair(make_triple(setHiInsnBlockIdAndInsnOffset.first,
                                     coNdxAndByteOffset_sethi.first,
                                     coNdxAndByteOffset_sethi.second),
                         make_triple(bsetInsnBlockIdAndInsnOffset.first,
                                     coNdxAndByteOffset_bset.first,
                                     coNdxAndByteOffset_bset.second));
}

void sparc_codeObjectCreationOracle::
processOffsetJumpTable32WithTentativeBounds(bbid_t bb_id,
                                            instr_t *instr,
                                            kptr_t jmpInsnAddr,
                                            bool delaySlotInThisBlock,
                                            instr_t *delaySlotInsn,
                                            kptr_t jumpTableStartAddr,
                                            const pdvector<kptr_t> &ijumpTableBlockAddrs,
                                               // may be too many!!!
                                               // We'll possibly adjust this
                                               // later!!!
                                            const simplePathSlice_t &settingJumpTableStartAddrSlice,
                                            const simplePath &pathFromEntryBB
                                            ) {
   // First things's first: we are certain of the start address of this jump
   // table.  That means that we should "register" this with the oracle, so that
   // later-created jump tables (yes, there can be two or more jump tables in a single
   // function) that begin before jumpTableStartAddr can be sure to stop adding
   // entries just before jumpTableStartAddr, if it wasn't going to already.
   // Furthermore, jump tables that have already been created which begin before
   // jumpTableStartAddr and last until >= jumpTableStartAddr now need to be
   // trimmed...a sort of "oops...I guess we overestimated the size of that last
   // jump table we parsed."
   adjustForNewJumpTableStartAddr(jumpTableStartAddr);

   pdvector<kptr_t> jumpTableBlockAddrs(ijumpTableBlockAddrs);

   pdvector<bbid_t> jumpTableBlockIds;
   for (pdvector<kptr_t>::const_iterator iter = jumpTableBlockAddrs.begin();
        iter != jumpTableBlockAddrs.end(); ++iter) {
      const kptr_t oneBlockAddr = *iter;
      const bbid_t oneBlockId = fn.searchForBB(oneBlockAddr);
      assert(oneBlockId != (bbid_t)-1);
      // don't assert that block's actual startAddr equals oneBlockAddr just
      // yet...give us a chance to shrink jumpTableBlockIds, below.
      
      jumpTableBlockIds += oneBlockId;
   }
   
   // --------------------
   // Now let us perhaps shrink jumpTableBlockIds, based on what we already
   // know from other jump table entries:
   const unsigned jumpTableEntryNumBytes = 4;
   const unsigned origJumpTableTentativeNBytes =
      jumpTableBlockIds.size()*jumpTableEntryNumBytes;
   unsigned jumpTableTentativeNBytes = origJumpTableTentativeNBytes;
   
   ipmin(jumpTableTentativeNBytes,
         getMaxJumpTableNumBytesBasedOnOtherJumpTableStartAddrs(jumpTableStartAddr));

   if (origJumpTableTentativeNBytes != jumpTableTentativeNBytes) {
      jumpTableBlockIds.shrink(jumpTableTentativeNBytes / jumpTableEntryNumBytes);
      jumpTableBlockAddrs.shrink(jumpTableBlockIds.size());
      
      dout << "NOTE: shrunk jumpTableTentativeNBytes in offsetJumpTable32 from "
           << origJumpTableTentativeNBytes << " to "
           << jumpTableTentativeNBytes << " due to detected clash with another jump table in this fn" << endl;
   }
   // --------------------

   // Some assertions that had to be postponed until the above "shrink" can now
   // be done.  (Actually, it might still be too early)
   for (pdvector<kptr_t>::const_iterator iter = jumpTableBlockAddrs.begin();
        iter != jumpTableBlockAddrs.end(); ++iter) {
      const kptr_t oneBlockAddr = *iter;
      const bbid_t oneBlockId = fn.searchForBB(oneBlockAddr);
      assert(oneBlockId != (bbid_t)-1);
      assert(fn.getBasicBlockFromId(oneBlockId)->getStartAddr() == oneBlockAddr);
   }

   // OK, we're a little more confident on the size of jumpTableBlockIds,
   // but not one hundred percent so.  It could still be too large, and we may have to
   // shrink it later.  For now we tentatively proceed with creating a codeObject,
   // which will need to store the usual jump/delay, and also a (tentative, perhaps
   // too large) copy of the contents of the jump table.  An offset32 jump table stores
   // offsets from the jump instruction.  We'll instead store bb_id's, making it
   // easier to re-create later on when some blocks have been outlined.

   const std::pair< triple<bbid_t, unsigned, unsigned>, triple<bbid_t, unsigned, unsigned> >
      setHiAndBSet_relocatableInfo =
      getSetHiAndBSetOrAddCodeObjRelativeInfo(settingJumpTableStartAddrSlice);
   
   jumpTableCodeObject *co =
      new offsetJumpTable32CodeObject(jumpTableStartAddr,
                                      instr, delaySlotInThisBlock,
                                      delaySlotInsn,
                                      jumpTableBlockIds,
                                      setHiAndBSet_relocatableInfo.first.first,
                                         // block id
                                      setHiAndBSet_relocatableInfo.first.second,
                                         // code obj ndx w/in bb
                                      setHiAndBSet_relocatableInfo.first.third,
                                         // insn byte offset w/in that code obj
                                      setHiAndBSet_relocatableInfo.second.first,
                                         // the bb
                                      setHiAndBSet_relocatableInfo.second.second,
                                         // ndx of code object w/in bb
                                      setHiAndBSet_relocatableInfo.second.third
                                         // byte offset w/in that code obj of insn
                                      );
   
      // note that the codeObject does not store any absolute addresses (such as
      // the jump table entries, which are instead passed in as block ids, nor
      // the address of the jump table entries, since that'll certainly change
      // as we outline)
   assert(co);

   // Store this offsetJumpTable32CodeObject (keep track of all jump tables, so we can
   // later trim this guy's length, should that prove necessary due to a conflict with
   // another jump table).
   allJumpTableCodeObjects += co; // don't forget this line!

   appendCodeObjectToBasicBlock(bb_id,
                                jmpInsnAddr -
                                   fn.getBasicBlockFromId(bb_id)->getStartAddr(),
                                co);
   endBasicBlock(bb_id);
   
   // Now recursively parse the jump table contents bytes.  Note that even if our
   // present idea of the jump table bounds is too large, we'll still only parse
   // legitimate blocks, so that's no big deal.
   pdvector<bbid_t>::const_iterator iter = jumpTableBlockIds.begin();
   pdvector<bbid_t>::const_iterator finish = jumpTableBlockIds.end();
   for (; iter != finish; ++iter) {
      const simplePath newPath(pathFromEntryBB, bb_id); // pathFromEntryBB + bb_id
      parseBlockIfNeeded(*iter, newPath);
   }
}
