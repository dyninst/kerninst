// interProcTailCallCodeObject.C
// call; restore    or    call; mov xxx, %o7
// True calls only (not jmpl)

#include "interProcTailCallCodeObject.h"
#include "util/h/xdr_send_recv.h"
#include "xdr_send_recv_common.h"

interProcTailCallCodeObjectBase::
interProcTailCallCodeObjectBase(bool idelaySlotInThisBlock,
                                instr_t *idelayInsn) :
      delaySlotInThisBlock(idelaySlotInThisBlock), delayInsn(idelayInsn) {
   // assert that delayInsn is pc-independent:
   sparc_instr::controlFlowInfo cfi;
   delayInsn->getControlFlowInfo(&cfi);
   assert(!cfi.fields.controlFlowInstruction); // too strong an assert?
}

interProcTailCallCodeObjectBase::
interProcTailCallCodeObjectBase(const interProcTailCallCodeObjectBase &src) :
   codeObject(src),
   delaySlotInThisBlock(src.delaySlotInThisBlock)
{
   delayInsn = instr_t::getInstr(*(src.delayInsn));
}

interProcTailCallCodeObjectBase::interProcTailCallCodeObjectBase(XDR *xdr) {
   delayInsn = (instr_t*) new sparc_instr(sparc_instr::nop);
   if (!P_xdr_recv(xdr, delaySlotInThisBlock) ||
       !P_xdr_recv(xdr, delayInsn))
      throw xdr_recv_fail();
}

bool interProcTailCallCodeObjectBase::send(bool, XDR *xdr) const {
   return (P_xdr_send(xdr, delaySlotInThisBlock) &&
           P_xdr_send(xdr, delayInsn));
}

void interProcTailCallCodeObjectBase::
emitWhenRelocatedTo(bool tryToKeepOriginalCode,
                    const function_t &/*parentFn*/,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &whereBlocksNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t /*fallThruBlockId*/
                       // NOT nec. the same as the block that'll get
                       // emitted next
                    ) const {
   emitCallInsn(tryToKeepOriginalCode, owning_bb_id, em, whereBlocksNowReside,
                true // always keep delay slot open, since we'll always emit it
                     // below (even if !delaySlotInThisBlock)
                );
   
   // Delay slot instruction (which is hopefully pc-independent, and must be either
   // a restore or an insn that writes to %o7)
   // NOTE: we don't need to check "delaySlotInSameBlock".  If it's false (the
   // unusual case) then we'll end up emitting an extra copy for use only by this
   // call instruction.
   sparc_instr::registerUsage ru;
   delayInsn->getRegisterUsage(&ru);
   const bool writesToO7 = *(ru.definitelyWritten) == (reg_t &)sparc_reg::o7;
      
   assert(delayInsn->isRestore() || writesToO7);
  
   instr_t *delayInsn_copy = instr_t::getInstr(*delayInsn);
   em.emit(delayInsn_copy); // the restore

   // No need to emit code to get to the fallee, since call/restore never falls
   // through.
   assert(thisIsLastCodeObjectInBlock);

   // Now let's think about some possible optimizations.  It would seem that there
   // can be none, but let's not jump to conclusions.  As mws of Sun has noted,
   // call;restore fools the UltraSparc's return address stack (RAS), preventing
   // proper prefetching when a 'return' is encountered.  We have a golden opportunity
   // to reverse this situation here, by emitting something *other* than a call
   // instruction to get to the destination, thus preventing a useless entry from being
   // pushed onto the RAS.
   // So how about emitting:
   //    sethi %hi(destAddr), %o7  // %o7 is surely available as a scratch reg
   //    jmp [%o7 + %lo(destAddr)] // no link register used
   //    restore instruction, as before (requires no use of %o7 to be safe)
   // One caveat with all of this, besides the extra instruction required, is that
   // according to the USparc-II User's Guide, the top of RAS is still used for
   // other jmp instructions (only the RAS isn't popped).  So, since we're using
   // a jmp instruction here, it'll try to prefetch from the top of the RAS, which will
   // be (in all likelihood) the address of whoever called us.  Not a great place to
   // be prefetching from!
   //
   // So until we figure all of this out, THIS POTENTIAL OPTIMIZATION IS NOT IMPLEMENTED
}

// ----------------------------------------------------------------------

interProcTailCallToAddrCodeObject::
interProcTailCallToAddrCodeObject(kptr_t icalleeAddr,
                                  bool idelayInsnToo,
                                  instr_t *idelayInsn) :
   base_class(idelayInsnToo, idelayInsn),
   calleeAddr(icalleeAddr) {
}

interProcTailCallToAddrCodeObject::
interProcTailCallToAddrCodeObject(const interProcTailCallToAddrCodeObject &src) :
   base_class(src),
   calleeAddr(src.calleeAddr) {
}

interProcTailCallToAddrCodeObject::interProcTailCallToAddrCodeObject(XDR *xdr) :
   base_class(xdr)
{
   if (!P_xdr_recv(xdr, calleeAddr))
      throw xdr_recv_fail();
}

bool interProcTailCallToAddrCodeObject::send(bool, XDR *xdr) const {
   return (base_class::send(false, xdr) &&
           P_xdr_send(xdr, calleeAddr));
}

void interProcTailCallToAddrCodeObject::
emitCallInsn(bool /*tryToKeepOriginalCode*/,
             bbid_t /*owning_bb_id*/,
             tempBufferEmitter &em,
             const outliningLocations &/*whereBlocksNowReside*/,
             bool /*leaveDelaySlotOpen*/) const {
   em.emitCall(calleeAddr);
      // will be resolved later, when we know insnAddr
}

// ----------------------------------------------------------------------

interProcTailCallToNameCodeObject::
interProcTailCallToNameCodeObject(const pdstring &icalleeSymName,
                                  bool idelayInsnToo,
                                  instr_t *idelayInsn) :
      base_class(idelayInsnToo, idelayInsn),
      calleeSymName(icalleeSymName) {
}

interProcTailCallToNameCodeObject::
interProcTailCallToNameCodeObject(const interProcTailCallToNameCodeObject &src) :
   base_class(src),
   calleeSymName(src.calleeSymName) {
}

interProcTailCallToNameCodeObject::interProcTailCallToNameCodeObject(XDR *xdr) :
   base_class(xdr)
{
   calleeSymName.~pdstring();
   if (!::P_xdr_recv(xdr, calleeSymName))
      throw xdr_recv_fail();
}

bool interProcTailCallToNameCodeObject::send(bool, XDR *xdr) const {
   return (base_class::send(false, xdr) &&
           P_xdr_send(xdr, calleeSymName));
}

void interProcTailCallToNameCodeObject::
emitCallInsn(bool /*tryToKeepOriginalCode*/,
             bbid_t /*owning_bb_id*/,
             tempBufferEmitter &em,
             const outliningLocations &,
             bool /*leaveDelaySlotOpen*/) const {
   em.emitCall(calleeSymName);
      // will be resolved later, when we know insnAddr and calleeSymName's addr.
}
