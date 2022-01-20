// interProcBranchAlwaysCodeObject.C

#include "interProcBranchAlwaysCodeObject.h"
#include "xdr_send_recv_common.h"
#include "util/h/xdr_send_recv.h"
#include "abi.h"

interProcBranchAlwaysCodeObjectBase::
interProcBranchAlwaysCodeObjectBase(instr_t *ioriginsn,
                                    bool idelayInsnToo,
                                    instr_t *idsInsn,
                                    regset_t *iavailRegsBeforeCodeObject,
                                    regset_t *iavailRegsAfterCodeObject)  :
   originsn(ioriginsn),
   hasDelaySlot(idelayInsnToo),
   dsInsn(idsInsn)
{
   availIntRegsBeforeCodeObject = (regset_t*) new sparc_reg_set(*((sparc_reg_set*)iavailRegsBeforeCodeObject) & 
								 sparc_reg_set::allIntRegs);
   availIntRegsAfterCodeObject = (regset_t*) new sparc_reg_set(*((sparc_reg_set*)iavailRegsAfterCodeObject) & 
							       sparc_reg_set::allIntRegs);

   delete iavailRegsBeforeCodeObject;
   delete iavailRegsAfterCodeObject;

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

   // dsInsn must be pc-independent; assert as such.  WARNING: the following check
   // isn't sufficient; what about "rd %pc, <rd>" for example?
   if (hasDelaySlot) {
      dsInsn->getControlFlowInfo(&cfi);
      assert(!cfi.fields.controlFlowInstruction);
   }
}

interProcBranchAlwaysCodeObjectBase::interProcBranchAlwaysCodeObjectBase(XDR *xdr) {
   originsn = (instr_t*) malloc(sizeof(sparc_instr));
   dsInsn = (instr_t*) malloc(sizeof(sparc_instr));

   trick_regset *tr1 = (trick_regset*) malloc(sizeof(trick_regset));
   trick_regset *tr2 = (trick_regset*) malloc(sizeof(trick_regset));

   if (!P_xdr_recv(xdr, originsn) ||
       !P_xdr_recv(xdr, hasDelaySlot) ||
       !P_xdr_recv(xdr, dsInsn) ||
       !P_xdr_recv(xdr, *tr1) ||
       !P_xdr_recv(xdr, *tr2))
      throw xdr_recv_fail();
   
   availIntRegsBeforeCodeObject = const_cast<regset_t*>(tr1->get());
   availIntRegsAfterCodeObject = const_cast<regset_t*>(tr2->get());
   free(tr1);
   free(tr2);

   assert((*((sparc_reg_set*)availIntRegsBeforeCodeObject) & sparc_reg_set::allIntRegs) ==
          *((sparc_reg_set*)availIntRegsBeforeCodeObject));
   assert((*((sparc_reg_set*)availIntRegsAfterCodeObject) & sparc_reg_set::allIntRegs) ==
          *((sparc_reg_set*)availIntRegsAfterCodeObject));
}

interProcBranchAlwaysCodeObjectBase::
interProcBranchAlwaysCodeObjectBase(const interProcBranchAlwaysCodeObjectBase &src) :
   codeObject(src),
   hasDelaySlot(src.hasDelaySlot) {
   
   dsInsn = instr_t::getInstr(*(src.dsInsn));
   originsn = instr_t::getInstr(*(src.originsn));
   availIntRegsBeforeCodeObject = regset_t::getRegSet((regset_t &)*(src.availIntRegsBeforeCodeObject)); 
   availIntRegsAfterCodeObject = regset_t::getRegSet((regset_t &)*(src.availIntRegsAfterCodeObject)); 

   assert((*((sparc_reg_set*)availIntRegsBeforeCodeObject) & sparc_reg_set::allIntRegs) ==
          *((sparc_reg_set*)availIntRegsBeforeCodeObject));
   assert((*((sparc_reg_set*)availIntRegsAfterCodeObject) & sparc_reg_set::allIntRegs) ==
          *((sparc_reg_set*)availIntRegsAfterCodeObject));
}

bool interProcBranchAlwaysCodeObjectBase::send(bool, XDR *xdr) const {
   assert((*((sparc_reg_set*)availIntRegsBeforeCodeObject) & sparc_reg_set::allIntRegs) ==
          *((sparc_reg_set*)availIntRegsBeforeCodeObject));
   assert((*((sparc_reg_set*)availIntRegsAfterCodeObject) & sparc_reg_set::allIntRegs) ==
          *((sparc_reg_set*)availIntRegsAfterCodeObject));

   return (P_xdr_send(xdr, *originsn) &&
           P_xdr_send(xdr, hasDelaySlot) &&
           P_xdr_send(xdr, *dsInsn) &&
           availIntRegsBeforeCodeObject->send(xdr) &&
           availIntRegsAfterCodeObject->send(xdr));
}

void interProcBranchAlwaysCodeObjectBase::
emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                    const function_t &/*parentFn*/,
                    bbid_t /*owning_bb_id*/,
                    tempBufferEmitter &em,
                    const outliningLocations &whereBlocksNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t /*fallThruBlockId*/
                       // NOT nec. same as block that'll be emitted next.
                       // Instead, it complements iftaken_bb_id.
                    ) const {
   assert(thisIsLastCodeObjectInBlock);

   // will a branch instruction have enough range?
   if (willBranchDefinitelyStillFit(whereBlocksNowReside)) {
      emitBranchInsnAssumeWillFit(em, tryToKeepOriginalCode);
      if (hasDelaySlot) {
         instr_t *dsInsn_copy =  instr_t::getInstr(*dsInsn);
         em.emit(dsInsn_copy);
      }
   }
   else {
      // not 100% sure that the branch can still fit.
      assert((*((sparc_reg_set*)availIntRegsBeforeCodeObject) & sparc_reg_set::allIntRegs) ==
             *((sparc_reg_set*)availIntRegsBeforeCodeObject));
      
      if (!availIntRegsBeforeCodeObject->isempty()) {
         sparc_reg r = (sparc_reg&) *availIntRegsBeforeCodeObject->begin();
         assert(r != sparc_reg::g0);
         assert(r.isIntReg());

         emitLongJumpEquivUsing1RegLeaveDelayOpen(em, r);
         if (hasDelaySlot) {
            instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
            em.emit(dsInsn_copy);
	 }
         else {
	    instr_t *nop = (instr_t*) new sparc_instr(sparc_instr::nop);
            em.emit(nop);
	 }
      }
      else {
         // no regs to play with.  Strategy becomes: delay insn (if any)
         // followed by save;call;restore.  Ick.
         if (hasDelaySlot) {
            // subtle bug: if dsInsn depends on the pc (e.g. is rd %pc) then
            // our movement of the delay insn up here could be wrong.
            instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
            em.emit(dsInsn_copy);
         }
         emitLongJumpEquivUsing0RegsIgnoreDelay(em);
            // save; call <destination>; restore
      }
   }

   // since this is a branch-always, no need to emit anything else (there's no
   // chance of a fall-thru).
}

// ----------------------------------------------------------------------

interProcBranchAlwaysToAddrCodeObject::
interProcBranchAlwaysToAddrCodeObject(kptr_t idestAddr,
                                      instr_t *ioriginsn,
                                      bool idelayInsnToo,
                                      instr_t *idsInsn,
                                      regset_t *iavailRegsBeforeCodeObject,
                                      regset_t *iavailRegsAfterCodeObject)  :
   base_class(ioriginsn, idelayInsnToo, idsInsn,
              iavailRegsBeforeCodeObject, iavailRegsAfterCodeObject),
   destAddr(idestAddr) {
}

interProcBranchAlwaysToAddrCodeObject::interProcBranchAlwaysToAddrCodeObject(XDR *xdr) :
   base_class(xdr)
{
   if (!P_xdr_recv(xdr, destAddr))
      throw xdr_recv_fail();
}

interProcBranchAlwaysToAddrCodeObject::
interProcBranchAlwaysToAddrCodeObject(const interProcBranchAlwaysToAddrCodeObject &src) :
   base_class(src),
   destAddr(src.destAddr) {
}

bool interProcBranchAlwaysToAddrCodeObject::send(bool, XDR *xdr) const {
   return (base_class::send(false, xdr) &&
           P_xdr_send(xdr, destAddr));
}

bool interProcBranchAlwaysToAddrCodeObject::
willBranchDefinitelyStillFit(const outliningLocations &whereBlocksNowReside) const {
   return whereBlocksNowReside.willInterprocBranchStillFit(destAddr);
}

void interProcBranchAlwaysToAddrCodeObject::
emitBranchInsnAssumeWillFit(tempBufferEmitter &em,
                            bool tryToKeepOriginalCode) const {
   if (tryToKeepOriginalCode) {
      instr_t *originsn_copy = instr_t::getInstr(*originsn);
      em.emitInterprocCondBranch(originsn_copy, destAddr);
   }
   else {
      // since we have permission to deviate from original code, let's always
      // emit the non-deprecated "bpcc" instruction here.
      instr_t *i = (instr_t*) new sparc_instr(sparc_instr::bpcc,
					      sparc_instr::iccAlways,
					      !hasDelaySlot, // annul bit
					      true, // predict taken
					      true, // xcc (doesn't matter)
					      0);
      em.emitInterprocCondBranch(i, destAddr);
   }
}

void interProcBranchAlwaysToAddrCodeObject::
emitLongJumpEquivUsing1RegLeaveDelayOpen(tempBufferEmitter &em, sparc_reg r) const {
   // note: not optimal (prefer the first to be emitImmAddrExceptLo10Bits or something
   // like that).  But at least this is 64-bit safe.
   em.emitImmAddr(destAddr, r);
   instr_t *i = (instr_t*) new sparc_instr(sparc_instr::jump, r, 0);
   em.emit(i);
}

void interProcBranchAlwaysToAddrCodeObject::
emitLongJumpEquivUsing0RegsIgnoreDelay(tempBufferEmitter &em) const {
   // no regs to play with.  Strategy becomes: delay insn (if any)
   // followed by save;call;restore.  Ick.
   instr_t *i = (instr_t*) new sparc_instr(sparc_instr::save, -em.getABI().getMinFrameAlignedNumBytes());
   em.emit(i);
   em.emitCall(destAddr); // yes, this is even OK for 64-bit addresses
   i = (instr_t*) new sparc_instr(sparc_instr::restore);
   em.emit(i); // delay slot of call
}

// ----------------------------------------------------------------------

interProcBranchAlwaysToNameCodeObject::
interProcBranchAlwaysToNameCodeObject(bool, // temp dummy param
                                      const pdstring &idestName, // "modName/fnName"
                                      kptr_t idestOriginalAddress,
                                      instr_t *ioriginsn,
                                      bool idelayInsnToo,
                                      instr_t *idsInsn,
                                      regset_t *iavailRegsBeforeCodeObject,
                                      regset_t *iavailRegsAfterCodeObject)  :
   base_class(ioriginsn, idelayInsnToo, idsInsn,
              iavailRegsBeforeCodeObject, iavailRegsAfterCodeObject),
   destName(idestName),
   destOriginalAddress(idestOriginalAddress)
{
}

interProcBranchAlwaysToNameCodeObject::interProcBranchAlwaysToNameCodeObject(XDR *xdr) :
   base_class(xdr)
{
   destName.~pdstring();
   
   if (!P_xdr_recv(xdr, destName) ||
       !P_xdr_recv(xdr, destOriginalAddress))
      throw xdr_recv_fail();
}

interProcBranchAlwaysToNameCodeObject::interProcBranchAlwaysToNameCodeObject(const interProcBranchAlwaysToNameCodeObject &src) :
   base_class(src),
   destName(src.destName),
   destOriginalAddress(src.destOriginalAddress) {
}

bool interProcBranchAlwaysToNameCodeObject::send(bool, XDR *xdr) const {
   return (base_class::send(false, xdr) &&
           P_xdr_send(xdr, destName) &&
           P_xdr_send(xdr, destOriginalAddress));
}

bool interProcBranchAlwaysToNameCodeObject::
willBranchDefinitelyStillFit(const outliningLocations &whereBlocksNowReside) const {
   return whereBlocksNowReside.willInterprocBranchStillFit(destName,
                                                           destOriginalAddress);
}

void interProcBranchAlwaysToNameCodeObject::
emitBranchInsnAssumeWillFit(tempBufferEmitter &em,
                            bool tryToKeepOriginalCode) const {
   if (tryToKeepOriginalCode) {
      instr_t *originsn_copy = instr_t::getInstr(*originsn);
      em.emitInterprocCondBranch(originsn_copy, destName);
      // same annul bit as original instruction, same int vs. fp, same bicc vs. bpcc
   }
   else {
      // since we have permission to deviate from original code, let's always
      // emit the non-deprecated "bpcc" instruction here.
      instr_t *i = (instr_t*) new sparc_instr(sparc_instr::bpcc,
					      sparc_instr::iccAlways,
					      !hasDelaySlot, // annul bit
					      true, // predict taken
					      true, // xcc (doesn't matter)
					      0);
      em.emitInterprocCondBranch(i, destName);
   }
}

void interProcBranchAlwaysToNameCodeObject::
emitLongJumpEquivUsing1RegLeaveDelayOpen(tempBufferEmitter &em,
                                         sparc_reg r) const {
   em.emitSymAddr(destName, r);
   instr_t *i = (instr_t*) new sparc_instr(sparc_instr::jump, r);
   em.emit(i);
}

void interProcBranchAlwaysToNameCodeObject::
emitLongJumpEquivUsing0RegsIgnoreDelay(tempBufferEmitter &em) const {
   instr_t *i = (instr_t*) new sparc_instr(sparc_instr::save, -em.getABI().getMinFrameAlignedNumBytes());
   em.emit(i);
   em.emitCall(destName);
   i = (instr_t*) new sparc_instr(sparc_instr::restore);
   em.emit(i); // delay slot of call
}
