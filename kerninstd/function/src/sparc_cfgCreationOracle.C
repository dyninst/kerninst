// sparc_cfgCreationOracle.C

#include "sparc_cfgCreationOracle.h"
#include "simplePath.h"
#include "util/h/minmax.h"
#include <algorithm>

//------------------------------------------------------------------

static bool
handleConditionalBranchInstrTakenCase(bbid_t bb_id,
                                      kptr_t destAddr,
				      const simplePath &pathFromEntryBB,
				         // doesn't include this
                                      const kmem */*km*/,
                                      parseOracle *theParseOracle) {
   // Returns true iff we split ourselves, in which case the caller should process
   // the fall-through case, if any, on behalf of getsucc(0) instead of this.

   // If destAddr isn't in any bb, then recursively create it.
   // Else, if destAddr is the start of a bb, then use it.
   // Else, if it's in the middle of an existing bb, then split it (being careful when
   // splitting ourselves).

   // Note: we can be assured that branch is intraprocedural; whoever called this
   // routine was certain of this before calling.  Therefore, the following asserts
   // may be applied:
   function_t *fn = theParseOracle->getMutableFn();
   assert(fn->getOrigCode().enclosesAddr(destAddr));

   assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() == 0);

   bbid_t succ_bb_id = fn->searchForBB(destAddr);
   if (succ_bb_id == (bbid_t)-1) {
      // destAddr doesn't lie within an existing basic block.  Recurse.
      basicblock_t *newbb = basicblock_t::getBasicBlock(destAddr, bb_id);
      succ_bb_id = fn->addBlock_new(newbb);

      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id);
         // won't be a dup since it was just created

      fn->parse(succ_bb_id,
		simplePath(pathFromEntryBB, bb_id), // pathFromEntryBB + bb_id
		theParseOracle);

      return false;
   }

   if (fn->getBasicBlockFromId(succ_bb_id)->getStartAddr() == destAddr) {
      // destAddr is the beginning of an existing bb; use it.
      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id); 
         // won't be a dup since numSuccessors() was 0
      if (!fn->addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
	 assert(false);

      return false;
   }

   assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() == 0);

   // destAddr lies in the middle of an existing bb.  Split that basic block
   // in two, set left as appropriate.  Be careful when splitting ourselves; it's
   // a tricky case to handle.
   bbid_t bb_to_split_id = succ_bb_id;

   const bool splitSelf = (bb_to_split_id == bb_id);

   succ_bb_id = fn->splitBasicBlock(bb_to_split_id, destAddr);

   // NOTE: we don't both to update pathFromEntryBB since it's only needed if we're
   // gonna recurse...which doesn't happen here.

   // Finally, do what we came here to do:
   if (!splitSelf) {
      // If we didn't split ourselves, then we should have no successors so far.
      assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() == 0);
      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id);
         // won't be a dup since numSuccessors() was 0
      if (!fn->addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
	 assert(false);
   }
   else {
      // If we split ourselves, then we should now have 1 successor: the lower
      // half of the split block:
      const basicblock_t *bb = fn->getBasicBlockFromId(bb_id);
      assert(bb->numSuccessors() == 1);

      const bbid_t succ0bb_id = bb->getsucc(0);
      const basicblock_t *succ0bb = fn->getBasicBlockFromId(succ0bb_id);
      
      assert(succ0bb->numPredecessors() == 1);
      assert(succ0bb->getpred(0) == bb_id);

      // Finally, do what we came here to do: add a successor representing
      // the taken branch.  Since we have split ourselves, the branch should
      // be added to the successors of getsucc(0);
      if (fn->addSuccessorToBlockByIdUnlessDup(succ0bb_id, succ0bb_id))
         if (!fn->addPredecessorToBlockByIdUnlessDup(succ0bb_id, succ0bb_id))
            assert(false);
   }

   // NOTE: If splitSelf, then we want to process the fall-through case on behalf of
   // 'succ', instead of curr bb.
   return splitSelf;
}

// ----------------------------------------------------------------------

static void
handleConditionalBranchInstrFallThroughCase(bbid_t bb_id,
                                            kptr_t fallThroughInsnAddr,
					    const simplePath &pathFromEntryBB,
					       // curr bb has not yet been added
                                            const kmem */*km*/,
                                            parseOracle *theParseOracle,
                                            bbParsingStats &theBBParsingStats) {
   // If the dest block is nextKnownFnStartAddr, then do nothing (much).
   // Else, if the dest block lies at the start of an existing bb, then use it.
   // Else, if it lies in the middle on an existing bb, then assert fail (never
   // happens with the fall-through case).

   function_t *fn = theParseOracle->getMutableFn();
   assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() < 2);

   if (!fn->containsAddr(fallThroughInsnAddr)) {
      // This check is new   03/2000 --ari
      // The fall-through case stumbles onto another function, noted by adding
      // "ExitBBId" as a successor instead of parsing the fall-thru block.
      //  if (verbose_fnParse) {
//           cout << "NOTE: Function " << fn->getPrimaryName().c_str()
//                << " (" << addr2hex(fn->getEntryAddr()) << ")"
//                << " has a conditional branch that falls through to "
//                << addr2hex(fallThroughInsnAddr) << ", the start of another function"
//                << endl;
//        }

      fn->addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);
         // stronger assertion!  12/2000

      fn->markBlockAsHavingInterProcFallThru(bb_id);

      theBBParsingStats.numCondBranchFallThrusThatDoInterProceduralStumble++;
      
      return;
   }
   
   bbid_t succ_id = fn->searchForBB(fallThroughInsnAddr);

   if (succ_id == (bbid_t)-1) {
      // fall-thru addr isn't within any existing basic block, so recursively
      // create it.
      basicblock_t *newbb = basicblock_t::getBasicBlock(fallThroughInsnAddr, bb_id);
      succ_id = fn->addBlock_new(newbb);

      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_id);
         // won't be a duplicate if it didn't exist anywhere until now

      fn->parse(succ_id,
		simplePath(pathFromEntryBB, bb_id), // pathFromEntryBB + bb_id
		theParseOracle);
   }
   else {
      const basicblock_t *fallThruBlock = fn->getBasicBlockFromId(succ_id);
   
      if (fallThruBlock->getStartAddr() == fallThroughInsnAddr) {
         // Dest block lies at the start of an existing bb; use it.
         // Don't allow duplicates in 'successors':
         if (fn->getBasicBlockFromId(bb_id)->numSuccessors() == 0 ||
             fn->getBasicBlockFromId(bb_id)->getsucc(0) != succ_id) { // disallow duplicates
            fn->addSuccessorToBlockByIdNoDup(bb_id, succ_id);
            if (!fn->addPredecessorToBlockByIdUnlessDup(succ_id, bb_id))
               assert(false);
         }
      }
      else {
         // dest block lies within an existing bb without being the start of a bb; can't
         // happen with the fall-through case.  Proof: Assume fall through address is
         // in the middle of an existing basic block.  This means that there are insns
         // before it in that basic block, because one does not fall through
         // without a branch instruction preceding it.  But that's strange, since we've
         // just been working on the instructions before the fall-through case; and
         // they're in the current bb, which won't yet show up in a .searchForBB() yet.
         cout << "Sorry, fall thru to " << addr2hex(fallThroughInsnAddr)
              << " lies within, but not at the beginning of, a basic block "
              << " whose startAddr is " << addr2hex(fallThruBlock->getStartAddr())
              << endl;
         assert(false);
      }
   }

   assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() > 0);
}

//------------------------------------------------------------------

void sparc_cfgCreationOracle::
processCallRecursion(bbid_t bb_id,
                     kptr_t calleeAddr,
                     instr_t *callInstr,
                     kptr_t instrAddr,
                     bool delaySlotInSameBlock,
                     instr_t *dsInsn,
                     const simplePath &pathFromEntryBB
                     ) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   // Recursive parse the destination of the call (will probably do nothing, since
   // presumably, the destination is the start of the fn, an already-parsed block).
   const bool splitSelf =
      handleConditionalBranchInstrTakenCase(bb_id,
                                            calleeAddr,
                                            pathFromEntryBB,
                                               // doesn't include this block yet
                                            km,
                                            this);

   // Recursively parse the fall-thru of the call
   
   bbid_t fallThruOnBehalfOf = splitSelf ?
      fn.getBasicBlockFromId(bb_id)->getsucc(0) :
      bb_id;
   
   handleConditionalBranchInstrFallThroughCase(fallThruOnBehalfOf,
                                               instrAddr + 8, // fallthru addr
                                               splitSelf ?
                                                  simplePath(pathFromEntryBB, bb_id) :
                                                     // pathFromEntryBB + bb_id
                                                  pathFromEntryBB,
                                               km,
                                               this,
                                               theBBParsingStats);

   theBBParsingStats.numCallRecursion++;

   delete callInstr;
   delete dsInsn;
}

// ----------------------------------------------------------------------

void sparc_cfgCreationOracle::
processV9Return(bbid_t bb_id, instr_t *insn,
                kptr_t /*insnAddr*/,
                bool delaySlotInSameBlock,
                instr_t *delayInsn) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, insn);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delayInsn);

   // any different from handling ret/retl?
   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   theBBParsingStats.numV9Returns++;

   delete insn;
   delete delayInsn;
}

// ----------------------------------------------------------------------

void sparc_cfgCreationOracle::
processInterproceduralBranch(bbid_t bb_id, kptr_t destAddr,
                             instr_t */*i*/,
                             const controlFlowInfo_t &cfi,
                             bool branchAlways,
                             kptr_t /*instrAddr*/,
                             bool /*delaySlotInSameBlock*/,
                             instr_t *dsInsn) {
   //  if (verbose_fnParse) {
//        cout << "NOTE: inter-procedural branch detected" << endl;
//        cout << "(dest addr is " << addr2hex(destAddr) << endl;
//        cout << "startAddr=" << addr2hex(fn.getEntryAddr()) << endl;
//        cout << "The branch insn is at address " << addr2hex(instrAddr) << ")"
//             << endl;
//     }

   // Add "interProcBranchExitBBId" as a successor to this basic block,
   // and add to interProcJumpsAndCalls[].

   // Note that, like a normal "intra-procedural" branch, it's possible that the
   // fall-thru case could be an inter-procedural stumble.  But that's not our
   // concern in this method.

   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::interProcBranchExitBBId);
      
   assert(instr_t::instructionHasValidAlignment(destAddr));
   interProcJumpsAndCalls[destAddr] = true; // val is dummy; key is what matters

   if (branchAlways)
      theBBParsingStats.numInterProcBranchAlways++;
   else {
      theBBParsingStats.numInterProcCondBranches++;
      if (((const sparc_instr::sparc_cfi&)cfi).sparc_fields.annul) {
         theBBParsingStats.numInterProcCondBranchesWithAnnulBit++;
         if (dsInsn->isSave() || dsInsn->isRestore())
            theBBParsingStats.numInterProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay++;
      }
   }
}

// ----------------------------------------------------------------------

void sparc_cfgCreationOracle::
processCondBranch(bbid_t bb_id,
                  bool branchAlways, bool branchNever,
                  instr_t *i,
                  controlFlowInfo_t *cfi,
                  kptr_t instrAddr,
                  bool hasDelaySlotInsn,
                  bool delaySlotInThisBlock,
                  instr_t *delaySlotInsn,
                  kptr_t fallThroughAddr,
                  kptr_t destAddr,
                  const simplePath &pathFromEntryBB) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, i);

   // Normally, there's no need to put the delay slot insn in its own, 1-instruction,
   // basic block.  The exception is if that instruction is a save or restore.
   const bool putDelayInsnInOwnBasicBlock =
      hasDelaySlotInsn && delaySlotInThisBlock &&
      cfi->delaySlot == sparc_instr::dsWhenTaken &&
      (delaySlotInsn->isSave() || delaySlotInsn->isRestore());
   if (putDelayInsnInOwnBasicBlock) {
      assert(delaySlotInThisBlock);
      delaySlotInThisBlock = false;
   }

   bbid_t parentIdOfIfTakenBlock = bb_id; // for now...
   bool fallThruBlockSpecial = false; // for now...

   if (hasDelaySlotInsn && delaySlotInThisBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delaySlotInsn);
   else if (hasDelaySlotInsn && putDelayInsnInOwnBasicBlock) {
      basicblock_t *newbb = basicblock_t::getBasicBlock(instrAddr + 4, bb_id);
      const bbid_t delaySlotBBId = fn.addBlock_new(newbb);
         // bb_id: parent basic block
      fn.bumpUpBlockSizeBy1Insn(delaySlotBBId, delaySlotInsn);
      
      fn.addSuccessorToBlockByIdNoDup(bb_id, delaySlotBBId);

      fallThruBlockSpecial = true;
      parentIdOfIfTakenBlock = delaySlotBBId;
   }

   if (!fallThruBlockSpecial)
      assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0); // for now...
      // can't make the assert if parentIdOfIfTakenBlock has been changed

   if (!branchNever) {
      // Handle the taken case.  It may or may not be interprocedural.
      if (!fn.containsAddr(destAddr)) {
         // unusual: an inter-procedural branch!
         this->processInterproceduralBranch(parentIdOfIfTakenBlock,
                                               // used to be bb_id; seems wrong to me.
                                            destAddr, i,
                                            *cfi, branchAlways,
                                            instrAddr,
                                            delaySlotInThisBlock,
                                            delaySlotInsn);
            // No need to pass around "path" information, since all this routine
            // does is add the appropriate exit edge.
      }
      else {
         if (branchAlways)
            theBBParsingStats.numIntraProcBranchAlways++;
         else if (branchNever)
            theBBParsingStats.numIntraProcBranchNevers++;
         else {
            theBBParsingStats.numIntraProcCondBranches++;
            if (((sparc_instr::sparc_cfi*)cfi)->sparc_fields.annul) {
               theBBParsingStats.numIntraProcCondBranchesWithAnnulBit++;
               if (delaySlotInsn->isSave() || delaySlotInsn->isRestore())
                  theBBParsingStats.numIntraProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay++;
            }
         }
         
         (void)handleConditionalBranchInstrTakenCase(parentIdOfIfTakenBlock,
                                                     destAddr,
                                                     parentIdOfIfTakenBlock!=bb_id ?
                                                     simplePath(pathFromEntryBB,bb_id) :
                                                        pathFromEntryBB,
                                                     km,
                                                     this);
      }
   }
   
   // Recurse on the fall-through case, if any.  If curr bb has been split anytime
   // during processing of the taken case, then the fall-through case must be processed
   // on behalf of this->left, not this!

   // Determining when we have been split is tricky.
   // As a first guess, we've been split if there's just 1 successor, and its
   // startAddr is our endAddr+1.
   // But there are cases that prove this test is insufficient.
   // In particular, IO_default_xsgetn does this tricky sequence:
   // 0x7117c bgu,a 0x71184
   // 0x71180 <some delay slot insn>
   // 0x71184 <start of new basic block>
   // Now, because the branch target is explicit, we should not count it as
   // a fall-through case, and thus as a split block case.
   // How do we determine the difference?  By looking at the control flow info.
   // If it was a control flow instruction (it was, if this routine was called)
   // and if the destination address is equal to the fall-through address
   // (which is equal to the address of this->left), then we weren't split;
   // else, say we were split.  (But does this cover all the cases?)

   if (!branchAlways) {
      // The parent of the fall-thru block is not always the same as
      // "handleTakenCaseOnBehalfOf" or its successor (when split).
      // In particular, if we put a save/restore
      // delay slot in its own 1-insn, then the parent of the fall-thru block
      // should be the block containing the branch instruction, not the delay
      // slot instruction.
      //
      // Rules to go by: If conditionally annulled delay slot then go with
      // whatever basic block contained the branch instruction.  Otherwise,
      // go with whatever basic block contained the delay slot instruction.

      // Calculate "updatedParentIdOfIfTakenBlock", to take into account possible
      // split(s) (yes, plural) of what used to be the bb containing the branch.
      const bbid_t updatedBBIdContainingBranch = fn.searchForBB(instrAddr);
      assert(updatedBBIdContainingBranch != (bbid_t)-1);
      const bbid_t updatedBBIdContainingDelay = fn.searchForBB(instrAddr+4);
         // OK because a conditional branch always has a delay slot
      if (updatedBBIdContainingDelay == (bbid_t)-1)
         // I'll allow this *if* the delay slot stumbles onto another fn:
         assert(!delaySlotInThisBlock && !fn.containsAddr(instrAddr+4));

      // Now, which of the two has a successor leading to the if-taken case?
      // That one will be designated "updatedParentIdOfIfTakenBlock"
      const basicblock_t *updatedBlockContainingBranch = 
         fn.getBasicBlockFromId(updatedBBIdContainingBranch);
      assert(updatedBlockContainingBranch->containsAddr(instrAddr));

      const bool blockContainingBranchIsParentOfIfTaken =
         (updatedBlockContainingBranch->numSuccessors() == 1 &&
          (updatedBlockContainingBranch->getsucc(0) == basicblock_t::interProcBranchExitBBId && !fn.containsAddr(destAddr) ||
           fn.getBasicBlockFromId(updatedBlockContainingBranch->getsucc(0))->getStartAddr() == destAddr));

      bool blockContainingDelayIsParentOfIfTaken = false;
      
      if (updatedBBIdContainingDelay != (bbid_t)-1) {
         // delay slot exists and is in this function.
         const basicblock_t *updatedBlockContainingDelay = 
            fn.getBasicBlockFromId(updatedBBIdContainingDelay);
         assert(updatedBlockContainingDelay->containsAddr(instrAddr+4));

         assert(updatedBlockContainingDelay->getStartAddr() >=
                updatedBlockContainingBranch->getStartAddr());
      
         blockContainingDelayIsParentOfIfTaken =
            (updatedBlockContainingDelay->numSuccessors() == 1 &&
             (updatedBlockContainingDelay->getsucc(0) == basicblock_t::interProcBranchExitBBId && !fn.containsAddr(destAddr) ||
              fn.getBasicBlockFromId(updatedBlockContainingDelay->getsucc(0))->getStartAddr() == destAddr));
      }
      
      bbid_t updatedParentIdOfIfTakenBlock = (bbid_t)-1;

      if (!blockContainingBranchIsParentOfIfTaken &&
          !blockContainingDelayIsParentOfIfTaken)
         assert(false && "must be one or the other");
      else if (blockContainingBranchIsParentOfIfTaken &&
               !blockContainingDelayIsParentOfIfTaken) {
         updatedParentIdOfIfTakenBlock = updatedBBIdContainingBranch;
      }
      else if (!blockContainingBranchIsParentOfIfTaken &&
               blockContainingDelayIsParentOfIfTaken) {
         updatedParentIdOfIfTakenBlock = updatedBBIdContainingDelay;
      }
      else if (blockContainingBranchIsParentOfIfTaken &&
               blockContainingDelayIsParentOfIfTaken) {
         assert(updatedBBIdContainingBranch == updatedBBIdContainingDelay);
         updatedParentIdOfIfTakenBlock = updatedBBIdContainingBranch;
      }
      else
         abort();
      
      assert(updatedParentIdOfIfTakenBlock != (bbid_t)-1);

      bbid_t parentIdOfFallThruBlock = (bbid_t)-1;
      if (!fallThruBlockSpecial) {
         // The parent block of the fall-thru case is gonna be the same as the
         // parent block of the if-taken case.
         assert(!(hasDelaySlotInsn && putDelayInsnInOwnBasicBlock));

         parentIdOfFallThruBlock = updatedParentIdOfIfTakenBlock;
      }
      else {
         assert(hasDelaySlotInsn && putDelayInsnInOwnBasicBlock);
         
         // The parent block of the fall-thru case is the block containing
         // the branch instruction, not the block containing the delay slot insn
         // (and they *will* be different, in this case)
         assert(updatedBBIdContainingBranch != updatedBBIdContainingDelay);
         parentIdOfFallThruBlock = updatedBBIdContainingBranch;
      }

      assert(parentIdOfFallThruBlock != (bbid_t)-1);
         
      handleConditionalBranchInstrFallThroughCase
         (parentIdOfFallThruBlock,
          fallThroughAddr,
          parentIdOfFallThruBlock != bb_id ?
             simplePath(pathFromEntryBB, bb_id) :
             pathFromEntryBB,
          km,
          this,
          theBBParsingStats);
   }
   delete i;
   delete cfi;
   delete delaySlotInsn;
}

void sparc_cfgCreationOracle::
processIntraproceduralJmpToFixedAddr(bbid_t bb_id,
                                     instr_t *instr,
                                     kptr_t /*jmpInsnAddr*/,
                                     bool delaySlotInSameBlock,
                                     instr_t *dsInsn,
                                     kptr_t destAddr,
                                     const simplePathSlice_t &/*jmpRegSlice*/,
                                     const simplePath &pathFromEntryBB) {
   //  if (verbose_fnParse)
//        cout << "I see a jump to a constant address: " << addr2hex(destAddr) << endl;

   fn.bumpUpBlockSizeBy1Insn(bb_id, instr);
      // formerly appendToBBContents()
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
      
   (void)handleConditionalBranchInstrTakenCase(bb_id, destAddr,
                                               pathFromEntryBB,
                                               km,
                                               this);
      // void the result: we don't care if we split ourself (this basic block),
      // since we're not going to process any fall-through case.

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 1);

   theBBParsingStats.numJumpToRegFixedAddr_IntraProc++;

   delete instr;
   delete dsInsn;
}


void sparc_cfgCreationOracle::
processInterproceduralJmpToFixedAddr(bbid_t bb_id,
                                     instr_t *instr,
                                     bool delaySlotInSameBlock,
                                     instr_t *dsInsn,
                                     kptr_t destAddr,
                                     const simplePath &/*pathFromEntryBB*/) {
   //  if (verbose_fnParse)
//        cout << "I see a jump to a constant address: " << addr2hex(destAddr) << endl;

   fn.bumpUpBlockSizeBy1Insn(bb_id, instr);
      // formerly appendToBBContents()
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   assert(instr_t::instructionHasValidAlignment(destAddr));
   interProcJumpsAndCalls[destAddr] = true; // value is a dummy; key is what matters

   theBBParsingStats.numJumpToRegFixedAddr_InterProc++;

   delete instr;
   delete dsInsn;
}

void sparc_cfgCreationOracle::
processTrueCallThatWritesToO7InDelay(bbid_t bb_id,
                                     instr_t *callInstr,
                                     kptr_t /*instrAddr*/,
                                     kptr_t destAddr,
                                     bool delaySlotInSameBlock,
                                     instr_t *dsInsn) {
   //  if (verbose_fnParse) {
//        cout << "found tail-call sequence (delay instruc writes to o7) @"
//             << addr2hex(instrAddr) << endl;
//        sparc_reg dummy=sparc_reg::g0; // filled in; we ignore
//        const bool isMove = dsInsn.isMove(sparc_reg::o7, dummy);

//        cout << "tail-calling routine at " << addr2hex(destAddr) << endl;
//        cout << "That was case 3" << endl;
//        if (!isMove)
//           cout << "However, an unusual case 3 since it wasn't a move!" << endl;
//     }

   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   assert(instr_t::instructionHasValidAlignment(destAddr));
   interProcJumpsAndCalls[destAddr] = true;

   theBBParsingStats.numTailCalls_trueCallThatWritesToO7InDelay++;

   delete callInstr;
   delete dsInsn;
}


void sparc_cfgCreationOracle::
processJmplThatWritesToO7InDelay(bbid_t bb_id, instr_t *callInstr,
                                 kptr_t /*instrAddr*/,
                                 bool delaySlotInSameBlock,
                                 instr_t *dsInsn) {
   //  if (verbose_fnParse) {
//        cout << "found tail-call sequence (delay instruc writes to o7) @"
//             << addr2hex(instrAddr) << endl;
//        cout << "That was case 6 -- unexpected (dumb compiler?)" << endl;

//        sparc_reg dummy=sparc_reg::g0; // filled in; we ignore
//        const bool isMove = dsInsn.isMove(sparc_reg::o7, dummy);
//        if (!isMove)
//           cout << "And an unusual case 6 besides, since it wasn't a move!" << endl;
//     }

   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   theBBParsingStats.numTailCalls_jmplThatWritesToO7InDelay++;

   delete callInstr;
   delete dsInsn;
}


void sparc_cfgCreationOracle::
processNonTailOptimizedTrueCall(bbid_t bb_id, instr_t *callInstr,
                                kptr_t /*instrAddr*/,
                                kptr_t destAddr,
                                bool delaySlotInSameBlock,
                                instr_t *delaySlotInsn
                                ) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delaySlotInsn);

   assert(instr_t::instructionHasValidAlignment(destAddr));
   interProcJumpsAndCalls[destAddr] = true;

   theBBParsingStats.numTrueCalls_notTail++;

   delete callInstr;
   delete delaySlotInsn;
}
   

void sparc_cfgCreationOracle::
processNonTailOptimizedJmplCall(bbid_t bb_id, instr_t *callInstr,
				kptr_t /*instrAddr*/,
				bool delaySlotInSameBlock,
				instr_t *delaySlotInsn
				) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delaySlotInsn);

   theBBParsingStats.numJmplCalls_notTail++;

   delete callInstr;
   delete delaySlotInsn;
}


void sparc_cfgCreationOracle::
processIndirectTailCall(bbid_t bb_id, instr_t *jmpInsn,
			kptr_t /*insnAddr*/,
			bool delaySlotInSameBlock,
			instr_t *dsInsn) const
{
   fn.bumpUpBlockSizeBy1Insn(bb_id, jmpInsn);
   if (delaySlotInSameBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, dsInsn);

   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   delete jmpInsn;
   delete dsInsn;
}


// ---------- Non-Loading Backwards Jump Table: ----------

void sparc_cfgCreationOracle::
processNonLoadingBackwardsJumpTable(bbid_t bb_id,
                                    kptr_t jumpTableStartAddr,
                                    kptr_t jumpTableEndAddr,
                                    instr_t *jmpInsn,
                                    bool delaySlotInThisBlock,
                                    instr_t *delaySlotInsn,
                                    const simplePathSlice_t &/*settingJustPastEndOfJumpTableAddrSlice*/,
                                       // a sethi & bset/add slice
                                    const simplePath &pathFromEntryBB) {
   fn.bumpUpBlockSizeBy1Insn(bb_id, jmpInsn);
   if (delaySlotInThisBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delaySlotInsn);

   for (kptr_t addr = jumpTableStartAddr; addr < jumpTableEndAddr; addr += 4) {
      bbid_t succbb_id = fn.searchForBB(addr);
      if (succbb_id == (bbid_t)-1) {
         // This is an entirely new basic block.  Create it & parse it.
	 basicblock_t *newbb = basicblock_t::getBasicBlock(addr, succbb_id);
         succbb_id = fn.addBlock_new(newbb);
         
         fn.addSuccessorToBlockByIdNoDup(bb_id, succbb_id);
         
         fn.parse(succbb_id,
		   simplePath(pathFromEntryBB, bb_id),
		   this);
         
         assert(fn.searchForBB(addr) == succbb_id);
      }
      else if (fn.getBasicBlockFromId(succbb_id)->getStartAddr() == addr) {
         // A basic block with that start address already exists.  Make it a
         // successor, unless it already is.
         if (fn.addSuccessorToBlockByIdUnlessDup(bb_id, succbb_id)) {
            if (!fn.addPredecessorToBlockByIdUnlessDup(succbb_id, bb_id))
               assert(false);
         }
         
         else
            assert(fn.getBasicBlockFromId(bb_id)->hasAsASuccessor(succbb_id));
      }
      else {
         // Need to split an existing basic block
         
         assert(succbb_id != bb_id);
         succbb_id = fn.splitBasicBlock(succbb_id, addr);

         // No worries about duplicates here since succbb didn't exist 'till just now.
         if (!fn.addSuccessorToBlockByIdUnlessDup(bb_id, succbb_id))
            assert(false);
         if (!fn.addPredecessorToBlockByIdUnlessDup(succbb_id, bb_id))
            assert(false);
      }
   }

   theBBParsingStats.numNonLoadingBackwardsJumpTables++;
   theBBParsingStats.numNonLoadingBackwardsJumpTableTotalNonDupEntries += fn.getBasicBlockFromId(bb_id)->numSuccessors();

   delete jmpInsn;
   delete delaySlotInsn;
}

// --------------------

void sparc_cfgCreationOracle::
processOffsetJumpTable32WithTentativeBounds(bbid_t bb_id,
                                            instr_t *instr,
                                            kptr_t /*jmpInsnAddr*/,
                                            bool delaySlotInThisBlock,
                                            instr_t *delaySlotInsn,
                                            kptr_t jumpTableStartAddr,
                                            const pdvector<kptr_t> &ijumpTableBlockAddrs,
                                               // tentative!! May be too many!!!
                                            const simplePathSlice_t &, // sethi/bset
                                            const simplePath &pathFromEntryBB) {
   assert(ijumpTableBlockAddrs.size() > 0);
   
   fn.bumpUpBlockSizeBy1Insn(bb_id, instr);
   if (delaySlotInThisBlock)
      fn.bumpUpBlockSizeBy1Insn(bb_id, delaySlotInsn);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0); // for now, at least

   // We are *certain* of the start addr of this jump table, so "register" it with
   // the oracle, so that later-created jump tables that begin before
   // jumpTableStartAddr can limit their bounds to just before jumpTableStartAddr.
   // Furthermore, already-created jump tables may get trimmed.  The problem is that
   // already-created jump tables, that may have parsed too far, may have created
   // too many basic blocks due to splitting, which is an unfortunate thing that
   // cannot be undone.  So it would be best for routines like this one to NOT
   // recursively parse any basic blocks...do that after fully unwinding.  The oracle
   // can keep track of all this stuff. XXX this is TO DO!!!  By the way, a good
   // test for this functionality, at least on Solaris7/32-bit/UltraSPARC is the
   // routine prioctl.  Big and lots of switch statements that go too far at first.
   adjustForNewJumpTableStartAddr(jumpTableStartAddr);

   pdvector<kptr_t> jumpTableBlockAddrs(ijumpTableBlockAddrs);
   
   // --------------------
   // Perhaps shrink jumpTableBlockAddrs[], based on already-parsed
   // jump tables.
   const unsigned jumpTableEntryNumBytes=4;
   const unsigned origJumpTableTentativeNBytes =
      jumpTableBlockAddrs.size() * jumpTableEntryNumBytes;
   unsigned jumpTableTentativeNBytes = origJumpTableTentativeNBytes;
   
   ipmin(jumpTableTentativeNBytes,
         getMaxJumpTableNumBytesBasedOnOtherJumpTableStartAddrs(jumpTableStartAddr));
   
   if (origJumpTableTentativeNBytes != jumpTableTentativeNBytes) {
      jumpTableBlockAddrs.shrink(jumpTableTentativeNBytes / jumpTableEntryNumBytes);

      //  if (verbose_fnParse)
//           cout << "cfgCreationOracle for fn "
//                << addr2hex(fn.getEntryAddr())
//                << " NOTE: shrunk jumpTable in offsetJumpTable32 from "
//                << origJumpTableTentativeNBytes << " to "
//                << jumpTableTentativeNBytes
//                << " due to detected clash with another jump table in this fn" << endl;
   }
   // --------------------

   // So we're now a little more confident on the correct size of the
   // jump table, but not one hundred percent so.  It could still be
   // too large.

   // Process all of the jump table entries.  Note that we may process too many.
   pdvector<kptr_t>::const_iterator bbiter = jumpTableBlockAddrs.begin();
   pdvector<kptr_t>::const_iterator bbfinish = jumpTableBlockAddrs.end();
   for (; bbiter != bbfinish; ++bbiter) {
      const kptr_t destAddr = *bbiter;
      bbid_t succ_bb_id = fn.searchForBB(destAddr);
      if (succ_bb_id == (bbid_t)-1) {
         // A new basic block
	 basicblock_t *newbb = basicblock_t::getBasicBlock(destAddr, bb_id);
         succ_bb_id = fn.addBlock_new(newbb);

         fn.addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id);
            // won't be a duplicate since it didn't exist anywhere til now.

         fn.parse(succ_bb_id,
		   simplePath(pathFromEntryBB, bb_id),
		   this);

         assert(succ_bb_id == fn.searchForBB(destAddr));
      }
      else if (fn.getBasicBlockFromId(succ_bb_id)->getStartAddr() == destAddr) {
         // A basic block with that start address already exists.  It's likely that
         // it's already a child; avoid duplicate successors.
         if (fn.addSuccessorToBlockByIdUnlessDup(bb_id, succ_bb_id)) {
            if (!fn.addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
               assert(false);
         }
         else
            assert(fn.getBasicBlockFromId(bb_id)->hasAsASuccessor(succ_bb_id));
      }
      else {
         // Need to split an existing basic block

         // assert that we are not splitting ourselves
         assert(succ_bb_id != bb_id);

         succ_bb_id = fn.splitBasicBlock(succ_bb_id, destAddr);
                                                        
         // the new succ_bb_id won't be a dup since it didn't exist until the split
         if (!fn.addSuccessorToBlockByIdUnlessDup(bb_id, succ_bb_id))
            assert(false);
         if (!fn.addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
            assert(false);
      }
   }

   // Reminder: we may have processed too many entries, and thus we may have too
   // many successors.  We'll have a chance to undo such damage later on.

   // Sanity check: at least 1 successor
   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() > 0);

   // Sanity check: all successor basic blocks should be found if we search
   // for them by their start addresses (since none will be ExitBB/UnanalyzableBB)
   const basicblock_t::ConstSuccIter start = fn.getBasicBlockFromId(bb_id)->beginSuccIter();
   const basicblock_t::ConstSuccIter finish = fn.getBasicBlockFromId(bb_id)->endSuccIter();
   for (basicblock_t::ConstSuccIter succiter=start; succiter != finish; ++succiter) {
      bbid_t succ_bb_id = *succiter;
      const basicblock_t *succbb = fn.getBasicBlockFromId(succ_bb_id);
      const kptr_t succbbStartAddr = succbb->getStartAddr();
         
      bbid_t searched_succbb_id = fn.searchForBB(succbbStartAddr);
      assert(succ_bb_id == searched_succbb_id);
   }

   //  if (verbose_fnParse) {
//        cout << "Done processing offset-32 jump table starting at "
//             << addr2hex(jumpTableStartAddr) << endl;
//        cout << "containing " << fn.getBasicBlockFromId(bb_id)->numSuccessors()
//             << " non-duplicate entries (tentative; may shrink later)" << endl;
//     }

   theBBParsingStats.numOffsetJumpTables++;
   theBBParsingStats.numOffsetJumpTableTotalNonDupEntries += fn.getBasicBlockFromId(bb_id)->numSuccessors();

   delete instr;
   delete delaySlotInsn;
}

