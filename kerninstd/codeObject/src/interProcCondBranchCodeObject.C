// interProcCondBranchCodeObject.C

#include "interProcCondBranchCodeObject.h"
#include "usualJump.h"
#include "util/h/rope-utils.h"
#include "xdr_send_recv_common.h"
#include "util/h/xdr_send_recv.h"
#include "abi.h"

interProcCondBranchCodeObjectBase::
interProcCondBranchCodeObjectBase(instr_t *iinsn,
                                  bool idelayInsnToo, // always true (truly cond branch)
                                  instr_t *idsInsn,
                                  regset_t *iavailRegsBefore,
                                  regset_t *iavailRegsAfterBranchInsn,
                                  regset_t *iavailRegsAfter)  :
      origBranchInsn(iinsn),
      delayInsnToo(idelayInsnToo), // always true (for a truly conditional branch)
      dsInsn(idsInsn) {

   availIntRegsBeforeCodeObject = (regset_t*) new sparc_reg_set(*((sparc_reg_set*)iavailRegsBefore) & 
								sparc_reg_set::allIntRegs);
   availIntRegsAfterBranchInsnBeforeDelayInsn = (regset_t*) new 
     sparc_reg_set(*((sparc_reg_set*)iavailRegsAfterBranchInsn) & sparc_reg_set::allIntRegs);
   availIntRegsAfterCodeObject = (regset_t*) new sparc_reg_set(*((sparc_reg_set*)iavailRegsAfter) & 
							       sparc_reg_set::allIntRegs);

   delete iavailRegsAfter;
   delete iavailRegsBefore;
   delete iavailRegsAfterBranchInsn;

   sparc_instr::sparc_cfi cfi;
   origBranchInsn->getControlFlowInfo(&cfi);

   // This class is for truly con&ditional branches, not branch-always or branch-never.
   // Assert this:
   assert(cfi.fields.controlFlowInstruction);
   if (cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc)
      assert(cfi.condition != sparc_instr::iccAlways &&
             cfi.condition != sparc_instr::iccNever);
   else if (cfi.sparc_fields.isBPr)
      ; // OK; never unconditional
   else if (cfi.sparc_fields.isFBfcc || cfi.sparc_fields.isFBPfcc)
      assert(cfi.condition != sparc_instr::fccAlways &&
             cfi.condition != sparc_instr::fccNever);
   else
      assert(false);

   // We don't assert that delayInsnToo==true, because there's one case where it's OK
   // for it to be false here: if a bb was split on the delay slot instruction.
}

interProcCondBranchCodeObjectBase::
interProcCondBranchCodeObjectBase(const interProcCondBranchCodeObjectBase &src) :
   codeObject(src),
   delayInsnToo(src.delayInsnToo) {
   
   origBranchInsn = instr_t::getInstr(*(src.origBranchInsn));
   dsInsn = instr_t::getInstr(*(src.dsInsn));
   availIntRegsBeforeCodeObject = regset_t::getRegSet((regset_t &)*(src.availIntRegsBeforeCodeObject));
   availIntRegsAfterCodeObject = regset_t::getRegSet((regset_t &)*(src.availIntRegsAfterCodeObject));
   availIntRegsAfterBranchInsnBeforeDelayInsn = regset_t::getRegSet((regset_t &)*(src.availIntRegsAfterBranchInsnBeforeDelayInsn));

}

interProcCondBranchCodeObjectBase::interProcCondBranchCodeObjectBase(XDR *xdr) {
   origBranchInsn = (instr_t*) malloc(sizeof(sparc_instr));
   dsInsn = (instr_t*) malloc(sizeof(sparc_instr));

   trick_regset *tr1 = (trick_regset*) malloc(sizeof(trick_regset));
   trick_regset *tr2 = (trick_regset*) malloc(sizeof(trick_regset));
   trick_regset *tr3 = (trick_regset*) malloc(sizeof(trick_regset));

   if (!P_xdr_recv(xdr, *origBranchInsn) ||
       !P_xdr_recv(xdr, delayInsnToo) ||
       !P_xdr_recv(xdr, *dsInsn) ||
       !P_xdr_recv(xdr, *tr1) ||
       !P_xdr_recv(xdr, *tr2) ||
       !P_xdr_recv(xdr, *tr3))
      throw xdr_recv_fail();

   availIntRegsBeforeCodeObject = const_cast<regset_t*>(tr1->get());
   availIntRegsAfterBranchInsnBeforeDelayInsn = const_cast<regset_t*>(tr2->get());
   availIntRegsAfterCodeObject = const_cast<regset_t*>(tr3->get());
   free(tr1);
   free(tr3);
   free(tr2);
}

bool interProcCondBranchCodeObjectBase::send(bool, XDR *xdr) const {
   return (P_xdr_send(xdr, *origBranchInsn) &&
           P_xdr_send(xdr, delayInsnToo) &&
           P_xdr_send(xdr, *dsInsn) &&
           availIntRegsBeforeCodeObject->send(xdr) &&
           availIntRegsAfterBranchInsnBeforeDelayInsn->send(xdr) &&
           availIntRegsAfterCodeObject->send(xdr));
}

void interProcCondBranchCodeObjectBase::
emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                    const function_t &parentFn,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &whereThingsNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t fallThruBlockId
                       // NOT nec. same as block that'll be emitted next.
                       // Instead, it complements iftaken_bb_id.
   ) const {
   assert(thisIsLastCodeObjectInBlock &&
          "condBranchCodeObject isn't the last code object in a block?!");

   sparc_instr::sparc_cfi cfi;
   origBranchInsn->getControlFlowInfo(&cfi);
   
   switch (cfi.delaySlot) {
      case instr_t::dsNone:
      case instr_t::dsNever:
         // never happens with a truly conditional branch
         assert(false);
         break;
      case instr_t::dsAlways:
      case instr_t::dsWhenTaken:
         assert(delayInsnToo && "splitting on delay slot not supported");
         break;
      default:
         assert(0);
   }

   assert(delayInsnToo && "splitting on delay slot not supported");
   
   // will a branch instruction have enough range?
   if (willBranchDefinitelyStillFit(whereThingsNowReside)) {
      emitBranchInsnAssumeWillFit(em, tryToKeepOriginalCode);
      instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
      em.emit(dsInsn_copy);

      if (fallThruBlockId == (bbid_t)-1)
         // probably interprocedural fall-thru
         emitGettingToInterProcFallee(em, whereThingsNowReside,
                                      false, // don't leave delay slot open
                                      *availIntRegsAfterCodeObject,
                                      parentFn, owning_bb_id);
      else
         emitGettingToIntraProcFallee(em,
                                      whereThingsNowReside,
                                      availIntRegsAfterCodeObject,
                                      owning_bb_id,
                                      fallThruBlockId);
   }
   else {
      // not 100% sure that the branch still has enough range.  Need a long jump.
      // Notice the shocking similarities to LRAndR's for conditional branches.
      // SHARE CODE DAMMIT!

      if (availIntRegsBeforeCodeObject->countIntRegs() >= 2 &&
          cfi.delaySlot == instr_t::dsAlways) {
         regset_t::const_iterator rs_iter = availIntRegsBeforeCodeObject->begin();
         assert(rs_iter != availIntRegsBeforeCodeObject->end());
         sparc_reg r1 = (sparc_reg&) *rs_iter;
         ++rs_iter;
         assert(rs_iter != availIntRegsBeforeCodeObject->end());
         sparc_reg r2 = (sparc_reg&) *rs_iter;

         // set r2 to iftaken addr
         // set r1 to fallthru addr (not a simple thing to figure out where this
         //                          now resides!)
         // movr <cond, testreg> r2 onto r1.....or movcc <xcc/fcc> r2 onto r1
         // jmp r1
         // delayInsn

         // set r2 to iftaken:
         emitAssignIfTakenAddrToReg(em, r2); // for the future: can use r1 as scratch

         // set r1 to iffallthru:
         if (fallThruBlockId == (bbid_t)-1) {
            // interproc fall-thru must be handled specially
	    sparc_reg_set availScratchRegs(*((sparc_reg_set*)availIntRegsBeforeCodeObject) - r1 - r2);
            emitSetRegToInterProcFalleeAddr(em, whereThingsNowReside,
                                            (reg_t&)r1, // dest reg
                                            availScratchRegs,
                                               // avail scratch regs
                                            parentFn,
                                            owning_bb_id);
         } else
            em.emitLabelAddr(outliningLocations::getBlockLabel(fallThruBlockId), r1);

	 instr_t *i = (instr_t*) new sparc_instr(sparc_instr::cfi2ConditionalMove(cfi,
										  r2, // move this
										  r1 // onto this
										  ));
         em.emit(i);
	 i = (instr_t*) new sparc_instr(sparc_instr::jump, r1);
         em.emit(i);
         instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
         em.emit(dsInsn_copy);
      }
      else if (availIntRegsAfterBranchInsnBeforeDelayInsn->countIntRegs() >= 1) {
         // our strategy: see condBranch_1FreeAfterInsn_LRAndR, and for goodness
         // sakes already, SHARE MORE CODE!
         //   branch to "ifTaken" (use a label to be safest about the offset...curr
         //      bbid+"branch_at_end" should be unique enough of a label name)
         //   nop
         // [notTaken:]
         //   unconditionally ba to getBlockLabel(fallThruBlockId)
         //   (If cfi.delaySlot == dsAlways then leave ds open & emit delay insn,
         //    else make it a ba,a,pt)
         // [taken:]
         //   emitAssignIfTakenAddrToReg(em, r)
         //   jump r
         //   emit delay insn (assert cfi.delaySlot == dsAlways or dsWhenTaken)

         const pdstring ifTakenLabelName = num2string(owning_bb_id) + pdstring("interProcCondBranch_iftaken");
         sparc_instr newBranchInsn(*((sparc_instr*)origBranchInsn));
         newBranchInsn.changeBranchOffsetAndAnnulBit(0, false);
         em.emitCondBranchToForwardLabel(&newBranchInsn, ifTakenLabelName);
         instr_t *nop = (instr_t*) new sparc_instr(sparc_instr::nop);
	 em.emit(nop); // delay slot
	 
         // notTaken:
         const bool doDelayInsnForNotTakenToo = (cfi.delaySlot==instr_t::dsAlways);
         const bool leaveDelaySlotOpenForNotTaken = doDelayInsnForNotTakenToo;
         
         if (fallThruBlockId == (bbid_t)-1)
            // handle interproc fall-thru specially
            emitGettingToInterProcFallee(em, whereThingsNowReside,
                                         leaveDelaySlotOpenForNotTaken,
                                         *availIntRegsAfterBranchInsnBeforeDelayInsn,
                                         parentFn,
                                         owning_bb_id);
         else {
            const bool annul = !doDelayInsnForNotTakenToo;
	    instr_t *i = (instr_t*) new sparc_instr(sparc_instr::bpcc,
						    sparc_instr::iccAlways,
						    annul,
						    true, // predict taken
						    true, // xcc (doesn't matter)
						    0);
            em.emitCondBranchToLabel(i, outliningLocations::getBlockLabel(fallThruBlockId));
	 }
         
         if (leaveDelaySlotOpenForNotTaken) {
            instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
            em.emit(dsInsn_copy);
	 }
         
         // ifTaken:
         em.emitLabel(ifTakenLabelName);
         sparc_reg r = (sparc_reg&) *availIntRegsAfterBranchInsnBeforeDelayInsn->begin();
         assert(r.isIntReg() && (r != sparc_reg::g0));
         emitAssignIfTakenAddrToReg(em, r);
            // NOTE: may require an additional scratch reg in the future
         instr_t *i = (instr_t*) new sparc_instr(sparc_instr::jump, r);
	 em.emit(i);
         assert(cfi.delaySlot == instr_t::dsAlways ||
                cfi.delaySlot == instr_t::dsWhenTaken);
         instr_t *dsInsn_copy =  instr_t::getInstr(*dsInsn);
         em.emit(dsInsn_copy);
      }
      else {
         // No available regs
         if (cfi.delaySlot == instr_t::dsAlways && fallThruBlockId != (bbid_t)-1) {
            // Delay slot is always executed (both if-taken and not), and
            // the fall-thru basic block isn't interprocedural.  We can therefore
            // reverse the condition being tested, branching directly to the former
            // fall-thru block.
            // Keep delay slot as before.
            // The fall-thru of this branch, which is logically what used to be
            // the if-taken case, will then contain a long jump to the
            // inter-procedural destination.

            // branch [not annulled] <reverse-cond> to FALL-THRU-BLOCK-ID
            //    (OK if we know not interproc fall-thru)
            // delay slot, as before
            // long jump to dest of inter proc branch

            sparc_instr newBranchInsn(*((sparc_instr*)origBranchInsn));
            newBranchInsn.reverseBranchPolarity(cfi);
            newBranchInsn.changeBranchOffset(0);
            
            em.emitCondBranchToLabel(&newBranchInsn,
                                     outliningLocations::getBlockLabel(fallThruBlockId));
            instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
            em.emit(dsInsn_copy);

	    instr_t *i = (instr_t*) new sparc_instr(sparc_instr::save,
						    -em.getABI().getMinFrameAlignedNumBytes());
            em.emit(i);
            emitAssignIfTakenAddrToReg(em, sparc_reg::l0);
	    i = (instr_t*) new sparc_instr(sparc_instr::jump, sparc_reg::l0);
            em.emit(i);
	    i = (instr_t*) new sparc_instr(sparc_instr::restore);
            em.emit(i); // delay slot of jump
         }
         else if (cfi.delaySlot == instr_t::dsAlways &&
                  fallThruBlockId == (bbid_t)-1) {
            // delay slot is always executed, but the fall-thru block is
            // inter-procedural, the latter making it hard to reverse the branch
            // polarity so that it goes directly to the fall-thru block (unless
            // we know that the branch is w/in range, but I don't feel like
            // checking that right now).  The only good news is that we can keep
            // the delay slot as it was.

            // b to "ifTaken"
            // delay slot, as usual
            // long jump to inter-proc fallee
            // [ifTaken:]
            // long jump to destination of original branch

            const pdstring ifTakenLabelName = num2string(owning_bb_id) + pdstring("interProcCondBranch_iftaken");
            sparc_instr newBranchInsn(*((sparc_instr*)origBranchInsn));
            newBranchInsn.changeBranchOffset(0);
            em.emitCondBranchToForwardLabel(&newBranchInsn, ifTakenLabelName);
           
	    instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
            em.emit(dsInsn_copy);

            // notTaken: jump to inter-proc fallee
            emitGettingToInterProcFallee(em, whereThingsNowReside,
                                         false, // don't leave ds open
                                            // (can't anyway since no free regs)
                                         regset_t::getEmptySet(),
                                            // no avail regs.  Since we've moved to
                                            // delay insn before us, things are so
                                            // weirded out anyway that I might
                                            // not trust any free regs anyway.
                                         parentFn,
                                         owning_bb_id);

            // ifTaken: jump to dest of the original branch
            em.emitLabel(ifTakenLabelName);
	    instr_t *i = (instr_t*) new sparc_instr(sparc_instr::save,
						    -em.getABI().getMinFrameAlignedNumBytes());
            em.emit(i);
            emitAssignIfTakenAddrToReg(em, sparc_reg::l0);
            i = (instr_t*) new sparc_instr(sparc_instr::jump, sparc_reg::l0);
	    em.emit(i);
	    i = (instr_t*) new sparc_instr(sparc_instr::restore);
            em.emit(i); // delay slot of jump
         }
         else {
            assert(cfi.sparc_fields.annul);
            assert(cfi.delaySlot == instr_t::dsWhenTaken);

            // Have to conditionally execute the delay slot instruction
            // Also, the fall-thru block may or may not be inter-procedural

            // b<cond>, a to "ifTaken"
            // delay slot as before
            // ba,a,pt to fallThruBlockId, or a long jump (if interprocedural fallee)
            // [ifTaken:]
            // long jump to original destination of the branch

            const pdstring ifTakenLabelName = num2string(owning_bb_id) + pdstring("interProcCondBranch_iftaken");
            sparc_instr newBranchInsn(*((sparc_instr*)origBranchInsn));
            newBranchInsn.changeBranchOffset(0);
            em.emitCondBranchToForwardLabel(&newBranchInsn, ifTakenLabelName);
            
	    instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
            em.emit(dsInsn_copy);

            // notTaken:
            if (fallThruBlockId == (bbid_t)-1) {
               // interproc fall-thru must be handled specially
               emitGettingToInterProcFallee(em, whereThingsNowReside,
                                            false, // don't leave ds open
                                            // (can't anyway since no free regs)
                                            regset_t::getEmptySet(),
                                            // no avail regs.
                                            parentFn,
                                            owning_bb_id);
            }
            else {
	       instr_t *i = (instr_t*) new sparc_instr(sparc_instr::bpcc,
						       sparc_instr::iccAlways,
						       true, // annulled
						       true, // predict taken
						       true, // xcc (doesn't matter)
						       0);
               em.emitCondBranchToLabel(i, outliningLocations::getBlockLabel(fallThruBlockId));
	    }
         
            // ifTaken:
            em.emitLabel(ifTakenLabelName);
	    instr_t *i = (instr_t*) new sparc_instr(sparc_instr::save,
						    -em.getABI().getMinFrameAlignedNumBytes());
            em.emit(i);
            emitAssignIfTakenAddrToReg(em, sparc_reg::l0);
	    i = (instr_t*) new sparc_instr(sparc_instr::jump, sparc_reg::l0);
            em.emit(i);
	    i = (instr_t*) new sparc_instr(sparc_instr::restore); 
            em.emit(i); // delay slot of jump
         }
      } // end of 0 free regs case
   }
}

// ----------------------------------------------------------------------

interProcCondBranchToAddrCodeObject::interProcCondBranchToAddrCodeObject(XDR *xdr) :
      base_class(xdr) {
   if (!P_xdr_recv(xdr, destAddr))
      throw xdr_recv_fail();
}

bool interProcCondBranchToAddrCodeObject::send(bool, XDR *xdr) const {
   return (base_class::send(false, xdr) &&
           P_xdr_send(xdr, destAddr));
}

bool interProcCondBranchToAddrCodeObject::
willBranchDefinitelyStillFit(const outliningLocations &whereThingsNowReside) const {
   return whereThingsNowReside.willInterprocBranchStillFit(destAddr);
}

void interProcCondBranchToAddrCodeObject::
emitBranchInsnAssumeWillFit(tempBufferEmitter &em,
                            bool tryToKeepOriginalCode) const {
   instr_t *origBranchInsn_copy = instr_t::getInstr(*origBranchInsn);
   if (tryToKeepOriginalCode) {
      em.emitInterprocCondBranch(origBranchInsn_copy, destAddr);
   }
   else {
      // since we have permission to deviate from original code, we can convert
      // bicc to the newer, non-deprecated bpcc (and similar for floating point
      // branches).  Anyway, this "optimization" is not yet implemented.
      em.emitInterprocCondBranch(origBranchInsn_copy, destAddr);
   }
}

void interProcCondBranchToAddrCodeObject::
emitAssignIfTakenAddrToReg(tempBufferEmitter &em, sparc_reg r) const {
   em.emitImmAddr(destAddr, r);
}

void interProcCondBranchToAddrCodeObject::
emit1InsnCallToIfTakenAddr(tempBufferEmitter &em) const {
   em.emitCall(destAddr);
}

// ----------------------------------------------------------------------

interProcCondBranchToNameCodeObject::interProcCondBranchToNameCodeObject(XDR *xdr) :
   base_class(xdr) {
      
   destName.~pdstring();
   if (!P_xdr_recv(xdr, destName) ||
       !P_xdr_recv(xdr, destOriginalAddress))
      throw xdr_recv_fail();
}

bool interProcCondBranchToNameCodeObject::send(bool, XDR *xdr) const {
   return (base_class::send(false, xdr) &&
           P_xdr_send(xdr, destName) &&
           P_xdr_send(xdr, destOriginalAddress));
}

bool interProcCondBranchToNameCodeObject::
willBranchDefinitelyStillFit(const outliningLocations &whereThingsNowReside) const {
   return whereThingsNowReside.willInterprocBranchStillFit(destName,
                                                           destOriginalAddress);
}

void interProcCondBranchToNameCodeObject::
emitBranchInsnAssumeWillFit(tempBufferEmitter &em,
                            bool tryToKeepOriginalCode) const {
   instr_t *origBranchInsn_copy = instr_t::getInstr(*origBranchInsn);
   if (tryToKeepOriginalCode) {
      em.emitInterprocCondBranch(origBranchInsn_copy, destName);
   }
   else {
      // we have permission to deviate from the original code, so we can change
      // all bicc's to bpcc's (and similar for floating point).  This optimization
      // (of dubious benefit though certainly no harm) is not yet implemented.
      em.emitInterprocCondBranch(origBranchInsn_copy, destName);
   }
}

void interProcCondBranchToNameCodeObject::
emitAssignIfTakenAddrToReg(tempBufferEmitter &em, sparc_reg r) const {
   em.emitSymAddr(destName, r);
}

void interProcCondBranchToNameCodeObject::
emit1InsnCallToIfTakenAddr(tempBufferEmitter &em) const {
   em.emitCall(destName);
}
