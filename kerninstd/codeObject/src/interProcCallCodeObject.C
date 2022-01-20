// interProcCallCodeObject.C

#include "interProcCallCodeObject.h"
#include "util/h/xdr_send_recv.h"
#include "xdr_send_recv_common.h"

interProcCallCodeObjectBase::
interProcCallCodeObjectBase(bool idelayInsnToo,
                            instr_t *idelayInsn,
                            regset_t *iavailRegsAfterCodeObject,
                            bool icallIsToARoutineReturningOnCallersBehalf) :
      delayInsnToo(idelayInsnToo),
      delayInsn(idelayInsn),
      availRegsAfterCodeObject(iavailRegsAfterCodeObject),
      callIsToARoutineReturningOnCallersBehalf(icallIsToARoutineReturningOnCallersBehalf)
{
   // assert that delayInsn is pc-independent:
   sparc_instr::controlFlowInfo cfi;
   delayInsn->getControlFlowInfo(&cfi);
   assert(!cfi.fields.controlFlowInstruction); // too strong an assert?
}

interProcCallCodeObjectBase::
interProcCallCodeObjectBase(const interProcCallCodeObjectBase &src) :
   codeObject(src),
   delayInsnToo(src.delayInsnToo),
   callIsToARoutineReturningOnCallersBehalf(src.callIsToARoutineReturningOnCallersBehalf)  
{
   delayInsn = instr_t::getInstr(*(src.delayInsn));
   availRegsAfterCodeObject = regset_t::getRegSet((regset_t &)*(src.availRegsAfterCodeObject));
}

interProcCallCodeObjectBase::interProcCallCodeObjectBase(XDR *xdr) {
   delayInsn = (instr_t *) malloc(sizeof(sparc_instr));
   trick_regset *tr = (trick_regset*)malloc(sizeof(trick_regset));
      // allocated but unintialized, just like igen expects
   if (!P_xdr_recv(xdr, delayInsnToo) ||
       !P_xdr_recv(xdr, *delayInsn) ||
       !P_xdr_recv(xdr, *tr) ||
       !P_xdr_recv(xdr, callIsToARoutineReturningOnCallersBehalf))
      throw xdr_recv_fail();
   availRegsAfterCodeObject = const_cast<regset_t*>(tr->get());
   free(tr);
}

bool interProcCallCodeObjectBase::send(bool, XDR *xdr) const {
   return (P_xdr_send(xdr, delayInsnToo) &&
           P_xdr_send(xdr, *delayInsn) &&
           availRegsAfterCodeObject->send(xdr) &&
           P_xdr_send(xdr, callIsToARoutineReturningOnCallersBehalf));
}


void interProcCallCodeObjectBase::
emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                    const function_t &parentFn,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &whereBlocksNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t fallThruBlockId
                       // NOT nec. the same as the block that'll get
                       // emitted next
                    ) const {
   // Optimization: if this is the last code object in the block, and if
   // fallThruBlockId isn't going to be emitted next, then we'd normally have
   // to emit the following sequence:
   //    call
   //    <delay slot as usual>
   //    ba,a to fallThruBlockId
   //
   // As always, branches suck.  We can try to avoid this situation by fudging
   // %o7, in the delay slot, such that it equals the address of fallThruBlockId.
   // This would require moving the usual delay slot instruction to before the
   // call instruction (or frying it entirely, it is was a nop), which is only safe if
   // it does not make use of, and does not write to, %o7.
   // Also, it's only allowed if the delay slot instruction wasn't originally a
   // restore (which, in this class can indeed be asserted.)
   // Also, this scheme requires us to be able to set %o7 to the address of
   // fallThruBlockId using just a single add instruction
   // (add %o7, <address of fallThruBlockId minus the address of the call insn>, %o7)
   // This will work only if fallThruBlockId isn't that far away from the current
   // basic block.  With big routines like tcp_rput_data, you just never know.
   // Let's say that it's definitely out of range.  Then we could still make it work
   // without a branch instruction, if we have a scratch register that's available
   // both before the call and after it (i.e. not %o7)
   //    emitImm32(address of fallThruBlockId, scratchreg)
   //    delay instruction (skip if nop)
   //    call
   //    mov scratchreg, %o7
   // Unfortunately this leads to three additional instructions, or two more than the
   // naive case requiring a ba,a.  We can optimize it down to two additional
   // instructions (one more than the naive ba,a case):
   //    sethi %hi(addr of fallThruBlockId), scratchreg
   //    delay instruction (skip if nop)
   //    call
   //    or scratchreg, %lo(addr of fallThruBlockId), %o7
   // But if may be that this final case will backfire, because writing to %o7 in the
   // delay slot will fool the UltraSparc's call return address stack (RAS) [the
   // RAS will cause prefetching upon 'return' to take place right after the call
   // instruction].  You can argue that due to ubiquitous call;restore optimizations,
   // which also fool the RAS, we should ignore the RAS with impunity.  I'm not so
   // sure, especially with a codeObject like this, which will not contain a restore
   // in the delay slot and therefore very likely did well with the RAS in the 
   // original code, so we shouldn't take a step backwards.
   //
   // So until we figure this all out, THESE OPTIMIZATIONS ARE NOT YET IMPLEMENTED.
   //
   // Optimization #2: As we presently see in Solaris7 tcp_rput_data, there are lots
   // of potentially tail-call-optimized calls that aren't being optimized at all.
   // For example:
   // 0x101b71bc call 0x100b56f4 
   // 0x101b71c0 mov %i0, %o0
   // 0x101b71c4 ret
   // 0x101b71c8 restore
   // 
   // So if this call instruction isn't the last in a basic block, and if the next
   // code object in the basic block is the last, and it's ret; restore, then we
   // can emit:
   // mov %i0, %o0 (move delay insn before the call; no harm in that if it doesn't
   //               mess with %o7)
   // call 0x100b56f4
   // restore (but what if original restore was non-trivial)

   // Let the derived class emit the call instruction itself:
   emitCallInsn(tryToKeepOriginalCode, owning_bb_id, em, whereBlocksNowReside);

   // Delay slot instruction (which is hopefully pc-independent):
   if (delayInsnToo) {
      instr_t *delayInsn_copy =  instr_t::getInstr(*delayInsn);
      em.emit(delayInsn_copy);
   }

   if (thisIsLastCodeObjectInBlock && !callIsToARoutineReturningOnCallersBehalf) {
      // There's a fall-through; emit getting to it now
      if (fallThruBlockId == (bbid_t)-1) {
         // interprocedural fall-thru
         emitGettingToInterProcFallee(em, whereBlocksNowReside,
                                      false, // don't leave delay slot open
                                      *availRegsAfterCodeObject,
                                      parentFn,
                                      owning_bb_id);
      }
      else
         emitGettingToIntraProcFallee(em,
                                      whereBlocksNowReside,
                                      availRegsAfterCodeObject,
                                      owning_bb_id,
                                      fallThruBlockId);
   }

   // Possible optimization: fudge with the value in %o7 in the delay slot, so that
   // the call automatically returns to the desired fall-thru block.  This can work
   // if the instruction in the delay slot is a nop.
}

// ----------------------------------------------------------------------

interProcCallToAddrCodeObject::
interProcCallToAddrCodeObject(kptr_t icalleeAddr,
                              bool idelayInsnToo,
                              instr_t *idelayInsn,
                              regset_t *iavailRegsAfterCodeObject,
                              bool icallIsToARoutineReturningOnCallersBehalf) :
   base_class(idelayInsnToo, idelayInsn, iavailRegsAfterCodeObject,
              icallIsToARoutineReturningOnCallersBehalf),
   calleeAddr(icalleeAddr) {
}

interProcCallToAddrCodeObject::
interProcCallToAddrCodeObject(const interProcCallToAddrCodeObject &src) :
   base_class(src),
   calleeAddr(src.calleeAddr) {
}

interProcCallToAddrCodeObject::interProcCallToAddrCodeObject(XDR *xdr) :
   base_class(xdr) // we have no choice but to receive the base class stuff first,
      // so make sure that it's sent in that order, too!
{
   if (!P_xdr_recv(xdr, calleeAddr))
      throw xdr_recv_fail();
}

bool interProcCallToAddrCodeObject::send(bool, XDR *xdr) const {
   return (base_class::send(false, xdr) &&
           P_xdr_send(xdr, calleeAddr));
}

void interProcCallToAddrCodeObject::
emitCallInsn(bool /*tryToKeepOriginalCode*/,
             bbid_t /*owning_bb_id*/,
             tempBufferEmitter &em,
             const outliningLocations &) const {
   // Assume it's within range of the call
   em.emitCall(calleeAddr); // gets resolved later, when we know the insnAddr
}

// ----------------------------------------------------------------------

interProcCallToNameCodeObject::
interProcCallToNameCodeObject(const pdstring &icalleeSymName,
                              bool idelayInsnToo,
                              instr_t *idelayInsn,
                              regset_t *iavailRegsAfterCodeObject,
                              bool icallIsToARoutineReturningOnCallersBehalf) :
      base_class(idelayInsnToo, idelayInsn, iavailRegsAfterCodeObject,
                 icallIsToARoutineReturningOnCallersBehalf),
      calleeSymName(icalleeSymName) {
}

interProcCallToNameCodeObject::
interProcCallToNameCodeObject(const interProcCallToNameCodeObject &src) :
   base_class(src),
   calleeSymName(src.calleeSymName) {
}

interProcCallToNameCodeObject::interProcCallToNameCodeObject(XDR *xdr) :
   base_class(xdr) // we have no choice but to receive the base class stuff first,
      // so make sure that it's sent in that order, too!
{
   calleeSymName.~pdstring(); // needed before a P_xdr_recv()
   if (!P_xdr_recv(xdr, calleeSymName))
      throw xdr_recv_fail();
}

bool interProcCallToNameCodeObject::send(bool, XDR *xdr) const {
   // base class stuff must be sent first, since constructor ordering dictates that such
   // stuff is received first.
   return (base_class::send(false, xdr) &&
           P_xdr_send(xdr, calleeSymName));
}

void interProcCallToNameCodeObject::
emitCallInsn(bool /*tryToKeepOriginalCode*/,
             bbid_t /*owning_bb_id*/,
             tempBufferEmitter &em,
             const outliningLocations &) const {
   // Assume it's within range of the call
   em.emitCall(calleeSymName); // gets resolved MUCH later
}
