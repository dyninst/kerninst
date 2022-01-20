// branchAlwaysCodeObject.C

#include "branchAlwaysCodeObject.h"
#include "xdr_send_recv_common.h"
#include "util/h/xdr_send_recv.h"
#include "util/h/out_streams.h"
#include "abi.h"

branchAlwaysCodeObject::
branchAlwaysCodeObject(bbid_t iiftaken_bb_id,
                       instr_t *ioriginsn,
                       bool idelayInsnToo,
                       bool idelaySlotInThisBlock,
                       instr_t *idsInsn,
                       regset_t *iavailRegsBeforeCodeObject,
                       regset_t *iavailRegsAfterCodeObject)  :
      iftaken_bb_id(iiftaken_bb_id),
      originsn(ioriginsn),
      hasDelaySlot(idelayInsnToo),
      delaySlotInThisBlock(idelaySlotInThisBlock),
      dsInsn(idsInsn),
      availRegsBeforeCodeObject(iavailRegsBeforeCodeObject),
      availRegsAfterCodeObject(iavailRegsAfterCodeObject)
{
   sparc_instr::sparc_cfi cfi;
   originsn->getControlFlowInfo(&cfi);

   // This class is for branch-always's, not never's or conditionals.
   // Assert this:
   assert(cfi.fields.controlFlowInstruction);
   if (cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc)
      assert(cfi.condition == sparc_instr::iccAlways);
   else if (cfi.sparc_fields.isBPr)
      assert(false); // never always
   else if (cfi.sparc_fields.isFBfcc || cfi.sparc_fields.isFBPfcc)
      assert(cfi.condition == sparc_instr::fccAlways);
   else
      assert(false);
}

branchAlwaysCodeObject::branchAlwaysCodeObject(XDR *xdr) :
      iftaken_bb_id(read1_bb_id(xdr))
{
   originsn = (instr_t*) malloc(sizeof(sparc_instr));
   dsInsn = (instr_t*) malloc(sizeof(sparc_instr));
   trick_regset *tr1 = (trick_regset*) malloc(sizeof(trick_regset));
   trick_regset *tr2 = (trick_regset*) malloc(sizeof(trick_regset));

   if (!P_xdr_recv(xdr, *originsn) ||
       !P_xdr_recv(xdr, hasDelaySlot) ||
       !P_xdr_recv(xdr, delaySlotInThisBlock) ||
       !P_xdr_recv(xdr, *dsInsn) ||
       !P_xdr_recv(xdr, *tr1) ||
       !P_xdr_recv(xdr, *tr2))
      throw xdr_recv_fail();

   availRegsBeforeCodeObject = const_cast<regset_t*>(tr1->get());
   availRegsAfterCodeObject = const_cast<regset_t*>(tr2->get());
   free(tr1);
   free(tr2);
}

branchAlwaysCodeObject::branchAlwaysCodeObject(const branchAlwaysCodeObject &src) :
   codeObject(src),
   iftaken_bb_id(src.iftaken_bb_id),
   hasDelaySlot(src.hasDelaySlot),
   delaySlotInThisBlock(src.delaySlotInThisBlock) {
   
   originsn = instr_t::getInstr(*(src.originsn));
   dsInsn = instr_t::getInstr(*(src.dsInsn));
   availRegsBeforeCodeObject = regset_t::getRegSet((regset_t &)*(src.availRegsBeforeCodeObject));
   availRegsAfterCodeObject = regset_t::getRegSet((regset_t &)*(src.availRegsAfterCodeObject));

}

bool branchAlwaysCodeObject::send(bool, XDR *xdr) const {
   return (P_xdr_send(xdr, iftaken_bb_id) &&
           P_xdr_send(xdr, originsn) &&
           P_xdr_send(xdr, hasDelaySlot) &&
           P_xdr_send(xdr, delaySlotInThisBlock) &&
           P_xdr_send(xdr, dsInsn) &&
           availRegsBeforeCodeObject->send(xdr) &&
           availRegsAfterCodeObject->send(xdr));
}

void branchAlwaysCodeObject::
emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                    const function_t &/*parentFn*/,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &whereBlocksNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t /*fallThruBlockId*/
                       // NOT nec. same as block that'll be emitted next.
                       // Instead, it complements iftaken_bb_id.
                    ) const {
   assert(thisIsLastCodeObjectInBlock);

   // A nifty optimized special case is if the if-taken block is going to emitted
   // immediately after the present block.  If so, and if thisIsLastCodeObjectInBlock
   // (which it must be for a branchAlways), then we don't need to emit a branch
   // at all...just the delay slot (if any)...and let us fall thru to the next
   // block!  Cool!
   if (!tryToKeepOriginalCode && // disable this optimization if testing
       whereBlocksNowReside.whoIfAnyoneWillBeEmittedAfter(owning_bb_id) ==
       iftaken_bb_id) {
      assert(thisIsLastCodeObjectInBlock); // always true for branch-always
      dout << "outlining good news: got rid of an unconditional branch" << endl;

      // If it exists, emit the delay slot instruction now...
      // we aren't checking the status of delaySlotInSameBlock...I think that
      // could lead to a duplication of the delay insn, but nothing harmful beyond
      // that...correct?
      if (hasDelaySlot) {
	instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
	em.emit(dsInsn_copy);
      }
      return;
   }

   if (whereBlocksNowReside.getWhereOutlinedBlocksReside()==
       outliningLocations::afterInlinedBlocks ||
          // in range because everything's within range
       whereBlocksNowReside.isBlockInlined(owning_bb_id)==
       whereBlocksNowReside.isBlockInlined(iftaken_bb_id)
          // in range because in same group (inlined vs. outlined)
      ) {
      // IfTaken address is within range; perhaps known, perhaps not.

      const pdstring ifTakenLabelName=outliningLocations::getBlockLabel(iftaken_bb_id);

      // As ever, it's best to avoid annulled predicted-taken-branches that might
      // reside at the end of an icache line, since the delay slot will always
      // need to get fetched, even if it'll be annulled. (UltraSparc-IIi User's
      // Guide, section 21.2.2.3).  So if we're going to annul, try to un-annul
      // and swap the ordering of this instruction and the previous one.
      // NOT YET IMPLEMENTED

      // Since bicc is deprecated, we always emit bpcc, unless tryToKeepOriginalCode
      // is set:
      if (tryToKeepOriginalCode) {
         em.emitCondBranchToLabel(originsn, ifTakenLabelName);
         // originsn will do; the offset will get patched up later
      }
      else {
         const bool annulBit = !hasDelaySlot;
         instr_t *i = new sparc_instr(sparc_instr::bpcc,
				      sparc_instr::iccAlways,
				      annulBit,
				      true, // predict taken
				      true, // xcc (doesn't matter)
				      0);
         em.emitCondBranchToLabel(i, ifTakenLabelName);
      }
         
      if (hasDelaySlot) {
         // we aren't checking the status of delaySlotInSameBlock...I think that
         // could lead to a duplication of the delay insn, but nothing harmful beyond
         // that...correct?
	instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
	em.emit(dsInsn_copy);
      }
   }
   else {
      // The if-taken address isn't definitely within range of a single branch insn,
      // and its addr is unknown (hasn't yet been emitted).

      const pdstring ifTakenLabelName=outliningLocations::getBlockLabel(iftaken_bb_id);

      // Strategy:
      // A) If a free reg to play with before the codeObject:
      //    emitLabelAddr(ifTakenSymName); jmp R; ds/nop
      // B) Else if a free reg to play with after the codeObject:
      //    ds (if any); emitLabelAddr; jmp R; nop;
      // C) Else (no free regs before or after the codeObject)
      //    ds (if any); save; emitLabelAddr; jmp R; restore

      if (availRegsBeforeCodeObject->countIntRegs() > 0) {
         // case A above
         reg_t &R = *availRegsBeforeCodeObject->begin();
         em.emitLabelAddr(ifTakenLabelName, R);
	 instr_t *i = new sparc_instr(sparc_instr::jump, (sparc_reg&)R);
         em.emit(i);
         if (hasDelaySlot) {
            // we aren't checking the status of delaySlotInSameBlock...I think that
            // could lead to a duplication of the delay insn, but nothing harmful beyond
            // that...correct?
            instr_t *dsInsn_copy=instr_t::getInstr(*dsInsn);
	    em.emit(dsInsn_copy);
         }
         else {
	    instr_t *nop = new sparc_instr(sparc_instr::nop);
            em.emit(nop);
	 }
      }
      else if (availRegsAfterCodeObject->countIntRegs() > 0) {
         // case B above
	 if (hasDelaySlot) {
            // we aren't checking the status of delaySlotInSameBlock...I think that
            // could lead to a duplication of the delay insn, but nothing harmful beyond
            // that...correct?
            instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
	    em.emit(dsInsn_copy);
	 }
         reg_t &R = *availRegsAfterCodeObject->begin();
         em.emitLabelAddr(ifTakenLabelName, R);
	 instr_t *i = new sparc_instr(sparc_instr::jump, (sparc_reg&)R);
         em.emit(i);
         instr_t *nop = new sparc_instr(sparc_instr::nop);
	 em.emit(nop);
      }
      else {
         // case C above
	 if (hasDelaySlot) {
            // we aren't checking the status of delaySlotInSameBlock...I think that
            // could lead to a duplication of the delay insn, but nothing harmful beyond
            // that...correct?
            instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
	    em.emit(dsInsn_copy);
	 }

	 instr_t *i = new sparc_instr(sparc_instr::save,
				      -em.getABI().getMinFrameAlignedNumBytes());
         em.emit(i);
         em.emitLabelAddr(ifTakenLabelName, sparc_reg::l0);
	 i = new sparc_instr(sparc_instr::jump, sparc_reg::l0);
         em.emit(i);
	 i = new sparc_instr(sparc_instr::restore);
         em.emit(i);
      }
   }
}


 
