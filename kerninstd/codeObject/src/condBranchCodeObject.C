// condBranchCodeObject.C
// NOTE: only for those branches that are truly conditional.  branchAlways's and
// branchNever's need not apply.

#include "condBranchCodeObject.h"
#include "usualJump.h"
#include "util/h/rope-utils.h"
#include "xdr_send_recv_common.h"
#include "util/h/xdr_send_recv.h"

#include "sparc_instr.h" 
typedef sparc_instr::sparc_cfi sparc_cfi;


condBranchCodeObject::condBranchCodeObject(bbid_t iiftaken_bb_id,
                                           instr_t *iinsn,
                                           instr_t::controlFlowInfo *icfi,
                                           bool idelaySlotInThisBlock,
                                           instr_t *idsInsn,
                                           regset_t *iavailRegsBefore,
                                           regset_t *iavailRegsAfter)  :
      iftaken_bb_id(iiftaken_bb_id),
      insn(iinsn),
      cfi(icfi),
      delaySlotInThisBlock(idelaySlotInThisBlock),
      dsInsn(idsInsn),
      availRegsBeforeCodeObject(iavailRegsBefore),
      availRegsAfterCodeObject(iavailRegsAfter)  {
    insn->getControlFlowInfo(cfi);

   // This class is for truly conditional branches, not branch-always or branch-never.
   // Assert this:
   assert(cfi->fields.controlFlowInstruction);
   if (((sparc_cfi*)cfi)->sparc_fields.isBicc || ((sparc_cfi*)cfi)->sparc_fields.isBPcc)
      assert(cfi->condition != sparc_instr::iccAlways &&
             cfi->condition != sparc_instr::iccNever);
   else if (((sparc_cfi*)cfi)->sparc_fields.isBPr)
      ; // OK; never unconditional
   else if (((sparc_cfi*)cfi)->sparc_fields.isFBfcc || ((sparc_cfi*)cfi)->sparc_fields.isFBPfcc)
      assert(cfi->condition != sparc_instr::fccAlways &&
             cfi->condition != sparc_instr::fccNever);
   else
      assert(false);
}

condBranchCodeObject::
condBranchCodeObject(const condBranchCodeObject &src) :
   codeObject(src),
   iftaken_bb_id(src.iftaken_bb_id),
   // cfi uninitialized for now! (no copy ctor)
   delaySlotInThisBlock(src.delaySlotInThisBlock) 
{
   insn = instr_t::getInstr(*(src.insn));   
   dsInsn = instr_t::getInstr(*(src.dsInsn));
   availRegsBeforeCodeObject = regset_t::getRegSet((regset_t &)*(src.availRegsBeforeCodeObject));
   availRegsAfterCodeObject = regset_t::getRegSet((regset_t &)*(src.availRegsAfterCodeObject));
   cfi = new sparc_cfi(); 
   insn->getControlFlowInfo(cfi);
}

condBranchCodeObject::condBranchCodeObject(XDR *xdr) :
   iftaken_bb_id(read1_bb_id(xdr))
{
   insn = (instr_t*) malloc(sizeof(sparc_instr));
   dsInsn = (instr_t*) malloc(sizeof(sparc_instr));
   // Note that send(), below, doesn't send the controlFlowInfo (this can be
   // derived, and besides, I didn't want to write a P_xdr_send() routine for
   // it).

   trick_regset *tr1 = (trick_regset*) malloc(sizeof(trick_regset));
   trick_regset *tr2 = (trick_regset*) malloc(sizeof(trick_regset));
   
   if (!P_xdr_recv(xdr, *insn) ||
       !P_xdr_recv(xdr, delaySlotInThisBlock) ||
       !P_xdr_recv(xdr, *dsInsn) ||
       !P_xdr_recv(xdr, *tr1) ||
       !P_xdr_recv(xdr, *tr2))
      throw xdr_recv_fail();
   
   availRegsBeforeCodeObject = const_cast<regset_t*>(tr1->get());
   availRegsAfterCodeObject = const_cast<regset_t*>(tr2->get());
   insn->getControlFlowInfo(cfi);
   free(tr1);
   free(tr2);
}

bool condBranchCodeObject::send(bool, XDR *xdr) const {
   return (P_xdr_send(xdr, iftaken_bb_id) &&
           P_xdr_send(xdr, *insn) &&
           P_xdr_send(xdr, delaySlotInThisBlock) &&
           P_xdr_send(xdr, *dsInsn) &&
           availRegsBeforeCodeObject->send(xdr) &&
           availRegsAfterCodeObject->send(xdr));
}

void condBranchCodeObject::
emitWithInRangeTakenAddress(bool /*tryToKeepOriginalCode*/,
                            const function_t &parentFn,
                            bbid_t owning_bb_id,
                            tempBufferEmitter &em,
                            const outliningLocations &whereBlocksNowReside,
                            bbid_t fallThruBlockId
                               // NOT nec. same as block that'll be emitted next.
                               // Instead, it complements iftaken_bb_id.
                               // NOTE: if (bbid_t)-1, then this basic block probably
                               // has an interprocedural fall-thru to some function,
                               // which can be determined by examining the parent fn.
                            ) const {
   // The isTaken address is within range.  Emit same branch (adjusted range) to ifTaken
   // Then get to fall-thru block, via emitGettingToFallee().
   //
   // Now for the optimization(s) to that rule:
   // 1) if original sequence was branch(+ds) to XXX/YYY/XXX, and if outlining
   //    order has changed things around so that YYY has been moved far away,
   //    while XXX stays put, then reverse the polarity of the branch, yielding
   //    branch[reversed](+ds if was always executed else nop) to YYY/
   //    ds (if was executed if-taken originally)/XXX
   //    
   //    This would be an improvement on the naive:
   //    branch(+ds) to XXX/ba,a to YYY/XXX
   // 
   //    So the check is: if_taken_block is scheduled to be emitted after this one,
   //    and fallThruBlockId is *not* scheduled to be emitted after this one.
   //    This would simplify to "if_taken_block is scheduled to be emitted next"
   //    except that the if-taken and fall-thru blocks can actually be the same, as in:
   //         bne,a foo
   //         inc %o0
   //    foo: ...
   //    We don't gain anything by optimizing such a case.
   //
   // 2) If the original sequence was
   //    b<cond>,a to HOTBLOCK
   //    <delay>
   //    <COLDBLOCK(S)>
   //    ...
   //    <delay, same as that in the branch above>
   //    <HOTBLOCK>
   //    Then we have a tricky case.  Note that the above optimization (case 1)
   //    will work poorly, because of the delay instruction located just before
   //    HOTBLOCK, which will probably still be emitted just before HOTBLOCK, thus
   //    preventing HOTBLOCK from being moved up to just after the branch.
   //    So what we do here is:
   //    b<reversecond> to COLDBLOCK(s)
   //    nop
   //    <delay>
   //    <HOTBLOCK>
   //    And we further optimize this by putting <delay> in the delay slot of the
   //    branch, should it be harmless to the results of COLDBLOCK(s).
   //
   //    How to detect this case:
   //    In the original code, the conditional branch was to a hot block, and has
   //    a delay slot instruction that is emitted immediately prior to HOTBLOCK
   //    in the original code (in its own basic blocks *that falls thru* to HOTBLOCK),
   //    and if the block to be emitted immediately after this branch is the
   //    1-codeobject basic block containing the delayinsn, then we can do the
   //    optimization:
   //    
   //    b<revcond> to COLDBLOCK(S)
   //    nop
   //    with assurances that the correct stuff will be emitted next.
   //
   //    But like the above case, also throw in a check that the if-taken and
   //    fall-thru blocks of the original code aren't the same; if so, don't
   //    bother with the optimization.
   //
   //    THIS OPTIMIZATION IS NOT YET IMPLEMENTED

   if (whereBlocksNowReside.whoIfAnyoneWillBeEmittedAfter(owning_bb_id) == iftaken_bb_id &&
       iftaken_bb_id != fallThruBlockId &&
       fallThruBlockId != (bbid_t)-1 // this avoids messing w/ interproc fall-thru case
       ) {
      // reverse the polarity of the branch instruction, and probably reverse
      // the predict bit.

      sparc_instr newInsn(*((sparc_instr*)insn));

      // Note that cfi has been stored (since ctor time); we use it now:
      newInsn.reverseBranchPolarity(*((sparc_cfi*)cfi));
      newInsn.changeBranchOffsetAndAnnulBit(*((sparc_cfi*)cfi), 0, false);
         // dummy offset for now.  And annul is ALWAYS set to false

      newInsn.changeBranchPredictBitIfApplicable(*((sparc_cfi*)cfi), false);
         // note: this is rather bold: we are changing the predict
         // bit to what we think is best, not necessarily to
         // the negation of the original value!!!!!!!!!  Neat!
         // May help branch prediction in the future, when the branch
         // prediction hardware tables (which are stored in the i-cache) become empty
         // (due to the block being flushed from the i-cache) and have to rely
         // on the bit in the instruction.
      
      const pdstring fallThruLabelName = outliningLocations::getBlockLabel(fallThruBlockId);
      em.emitCondBranchToLabel(&newInsn, // undefined offset so far
                               fallThruLabelName);
      
      // note that "cfi" is the *original* control flow info value, as we want

      switch (cfi->delaySlot) {
         case instr_t::dsAlways: {
            // Note that we're not checking the status of delaySlotInThisBlock
            // (correct?)
	    instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
            em.emit(dsInsn_copy);
            }
            break;
         case instr_t::dsNone:
            assert(false && "dsNone shouldn't arise with a truly conditional branch");
            break;
         case instr_t::dsNever:
            assert(false && "dsNever shouldn't arise with a truly conditional branch");
            break;
         case instr_t::dsWhenTaken: {
            // Note that we're not checking the status of delaySlotInThisBlock
            // (correct?)

            // TO DO: if dsInsn doesn't affect the code of "fallThruBlockId" then
            // emit just the dsInsn, skipping the nop.
            const bool dsInsnDefinitelyDoesNotAffectFallThruBlock = false;
               // TODO: instead of false, actually make an effort to set it to true

            if (dsInsnDefinitelyDoesNotAffectFallThruBlock) {
	       instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
               em.emit(dsInsn_copy);
            }
            else {
               instr_t *i = (instr_t*) new sparc_instr(sparc_instr::nop);
	       em.emit(i);
	       delete(i);
               
               // we're now in a new basic block that'll be executed only if taken.
               // So, we've got to emit the original delay slot instruction.
               // As an optimization, we'll skip this step if that delay slot
               // instruction was a nop.  But that's hardly likely; I do not know
               // of any code generator who would be dumb enough to make a nop
               // instruction conditionally annulled.  But the check is harmless,
               // so we have it here.
               if (! dsInsn->isNop() ) {
	          instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
                  em.emit(dsInsn_copy);
	       }
            }

            break;
         }
         default:
            assert(false);
      }
      
      // Next we'll be emitting code for block iftaken_bb_id.  No need for us to
      // do that; it'll happen naturally
      assert(whereBlocksNowReside.whoIfAnyoneWillBeEmittedAfter(owning_bb_id) ==
             iftaken_bb_id);

      return;
   }
   
   // All done handling the exceptions to the usual rule; time for the usual rule:
   
   sparc_instr newInsn(*((sparc_instr*)insn));
   newInsn.changeBranchOffset(0); // a dummy, may help debugging

   pdstring ifTakenLabelName = outliningLocations::getBlockLabel(iftaken_bb_id);

   em.emitCondBranchToLabel(&newInsn, // undefined offset so far, of course
                            ifTakenLabelName);
         
   // delay slot, when applicable
   switch (cfi->delaySlot) {
      case instr_t::dsNone:
         // never happens with a truly conditional branch, right?
         assert(false);
         break;
      case instr_t::dsNever:
         // never happens with a truly conditional branch, right?
         assert(false);
         break;
      case instr_t::dsAlways:
      case instr_t::dsWhenTaken: {
         // Note that aren't checking the status of delaySlotInThisBlock; if it was
         // false, then I think the worst that can happen is an unnecessary second
         // copy of the delay slot insn can be emitted.
	 instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
         em.emit(dsInsn_copy);
      }
      break;
      default:
         assert(0);
   }

   // Now get to the fallee block.  Don't forget to check for interprocedural fall-thru
   if (fallThruBlockId == (bbid_t)-1)
      // inter proc fall thru
      emitGettingToInterProcFallee(em, whereBlocksNowReside,
                                   false, // don't leave delay slot open
                                   *availRegsAfterCodeObject,
                                   parentFn, owning_bb_id);
   else
      emitGettingToIntraProcFallee(em,
                                   whereBlocksNowReside,
                                   availRegsAfterCodeObject,
                                   owning_bb_id,
                                   fallThruBlockId);
}

void condBranchCodeObject::
emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                    const function_t &parentFn,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &whereBlocksNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t fallThruBlockId
                       // NOT nec. same as block that'll be emitted next.
                       // Instead, it complements iftaken_bb_id.
                    ) const {
   assert(thisIsLastCodeObjectInBlock &&
          "condBranchCodeObject isn't the last code object in a block?!");
   
   if (whereBlocksNowReside.getWhereOutlinedBlocksReside()==
       outliningLocations::afterInlinedBlocks ||
          // ifTaken will be in range because everything's within range
       whereBlocksNowReside.isBlockInlined(owning_bb_id)==
       whereBlocksNowReside.isBlockInlined(iftaken_bb_id)
          // ifTaken will be in range because same category (inlined vs. outlined)
      ) {
      emitWithInRangeTakenAddress(tryToKeepOriginalCode,
                                  parentFn, owning_bb_id,
                                  em, whereBlocksNowReside,
                                  fallThruBlockId);
   }
   else
      assert(false && "condBranchCodeObject emit: not in range");
}


