// intraProcJmpToFixedAddrCodeObject.C

#include "intraProcJmpToFixedAddrCodeObject.h"
#include "xdr_send_recv_common.h"
#include "util/h/xdr_send_recv.h"

#include "sparc_tempBufferEmitter.h"

intraProcJmpToFixedAddrCodeObject::
intraProcJmpToFixedAddrCodeObject(instr_t *iorig_jumpinsn,
                                  bbid_t idest_bbid,
                                  kptr_t iorig_destaddr,
                                  bool ihasDelaySlot,
                                  instr_t *idsInsn,
                                  bbid_t isetHiInsnBBId,
                                  unsigned isetHiCodeObjectNdxWithinBB,
                                  unsigned isetHiByteOffsetWithinCodeObject,
                                  bbid_t ibsetOrAddInsnBBId,
                                  unsigned ibsetOrAddCodeObjectNdxWithinBB,
                                  unsigned ibsetOrAddByteOffsetWithinCodeObject) :
   dest_bbid(idest_bbid),
   orig_destaddr(iorig_destaddr),
   orig_jumpinsn(iorig_jumpinsn),
   hasDelaySlot(ihasDelaySlot),
   dsInsn(idsInsn),
   setHiInsnBBId(isetHiInsnBBId),
   setHiCodeObjectNdxWithinBB(isetHiCodeObjectNdxWithinBB),
   setHiByteOffsetWithinCodeObject(isetHiByteOffsetWithinCodeObject),
   bsetOrAddInsnBBId(ibsetOrAddInsnBBId),
   bsetOrAddCodeObjectNdxWithinBB(ibsetOrAddCodeObjectNdxWithinBB),
   bsetOrAddByteOffsetWithinCodeObject(ibsetOrAddByteOffsetWithinCodeObject) 
{
   assert(setHiCodeObjectNdxWithinBB < 1000); // arbitrary
   assert(setHiByteOffsetWithinCodeObject < 1000); // arbitrary
   assert(bsetOrAddCodeObjectNdxWithinBB < 1000); // arbitrary
   assert(bsetOrAddByteOffsetWithinCodeObject < 1000); // arbitrary

   // assert that orig insn is of the form "jmp [R]", not "jmp [R1+R2 (except %g0)]"
   // or "jmp [R + non-zero-offset]"
   sparc_instr::sparc_cfi cfi;
   orig_jumpinsn->getControlFlowInfo(&cfi);

   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.fields.isJmpLink);
   if (cfi.destination == instr_t::controlFlowInfo::regRelative) {
      // "jmp [R + offset]" --> offset must be 0
      assert(cfi.offset_nbytes == 0);
   }
   else if (cfi.destination == instr_t::controlFlowInfo::doubleReg) {
      // "jmp [R1 + R2]" --> one of R1,R2 must be %g0
      assert((*((sparc_reg*)cfi.destreg1) == sparc_reg::g0) || 
	     (*((sparc_reg*)cfi.destreg2) == sparc_reg::g0));
   }
   else
      assert(false);
}

intraProcJmpToFixedAddrCodeObject::
intraProcJmpToFixedAddrCodeObject(const intraProcJmpToFixedAddrCodeObject &src) :
   codeObject(src),
   dest_bbid(src.dest_bbid),
   orig_destaddr(src.orig_destaddr),
   hasDelaySlot(src.hasDelaySlot),
   setHiInsnBBId(src.setHiInsnBBId),
   setHiCodeObjectNdxWithinBB(src.setHiCodeObjectNdxWithinBB),
   setHiByteOffsetWithinCodeObject(src.setHiByteOffsetWithinCodeObject),
   bsetOrAddInsnBBId(src.bsetOrAddInsnBBId),
   bsetOrAddCodeObjectNdxWithinBB(src.bsetOrAddCodeObjectNdxWithinBB),
   bsetOrAddByteOffsetWithinCodeObject(src.bsetOrAddByteOffsetWithinCodeObject) {
   orig_jumpinsn = instr_t::getInstr(*(src.orig_jumpinsn));
   dsInsn = instr_t::getInstr(*(src.dsInsn));
}
   

template <class T>
static void destruct1(T &t) {
   t.~T();
}

intraProcJmpToFixedAddrCodeObject::
intraProcJmpToFixedAddrCodeObject(XDR *xdr) {
   orig_jumpinsn = (instr_t*) new sparc_instr(sparc_instr::nop);
   dsInsn = (instr_t*) new sparc_instr(sparc_instr::nop);
   destruct1(*orig_jumpinsn);
   destruct1(*dsInsn);
   
   if (!P_xdr_recv(xdr, dest_bbid) ||
       !P_xdr_recv(xdr, orig_destaddr) ||
       !P_xdr_recv(xdr, orig_jumpinsn) ||
       !P_xdr_recv(xdr, hasDelaySlot) ||
       !P_xdr_recv(xdr, dsInsn) ||
       !P_xdr_recv(xdr, setHiInsnBBId) ||
       !P_xdr_recv(xdr, setHiCodeObjectNdxWithinBB) ||
       !P_xdr_recv(xdr, setHiByteOffsetWithinCodeObject) ||
       !P_xdr_recv(xdr, bsetOrAddInsnBBId) ||
       !P_xdr_recv(xdr, bsetOrAddCodeObjectNdxWithinBB) ||
       !P_xdr_recv(xdr, bsetOrAddByteOffsetWithinCodeObject))
      throw xdr_recv_fail();
}

bool intraProcJmpToFixedAddrCodeObject::send(bool, XDR *xdr) const {
   return (P_xdr_send(xdr, dest_bbid) &&
           P_xdr_send(xdr, orig_destaddr) &&
           P_xdr_send(xdr, orig_jumpinsn) &&
           P_xdr_send(xdr, hasDelaySlot) &&
           P_xdr_send(xdr, dsInsn) &&
           P_xdr_send(xdr, setHiInsnBBId) &&
           P_xdr_send(xdr, setHiCodeObjectNdxWithinBB) &&
           P_xdr_send(xdr, setHiByteOffsetWithinCodeObject) &&
           P_xdr_send(xdr, bsetOrAddInsnBBId) &&
           P_xdr_send(xdr, bsetOrAddCodeObjectNdxWithinBB) &&
           P_xdr_send(xdr, bsetOrAddByteOffsetWithinCodeObject));
}

void intraProcJmpToFixedAddrCodeObject::
emitWhenRelocatedTo(bool /*tryToKeepOriginalCode*/,
                    const function_t &/*parentFn*/,
                    bbid_t /*owning_bb_id*/,
                    tempBufferEmitter &em,
                    const outliningLocations &/*whereThingsNowReside*/,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t /*fallThruBlockId*/
                       // NOT nec. same as block that'll be emitted next.
                       // Instead, it complements iftaken_bb_id.
                    ) const {
   // we just need to emit the original jmp insn, with perhaps an adjustment
   // for the offset, if any, used in the jmp.  That is to say, if the orig jmp
   // insn was "jmp [R]" then change nothing; if it was "jmp [R1 + const]" then
   // update the constant to reflect the new destination.  (Actually we only support
   // the first.)
   // 
   // Then, backpatch the prior sethi/bset insn(s) to also reflect the new
   // destination.
   //
   // Speaking of which, the new destination is a function of the original destination
   // (as stored, redundantly, in both dest_bbid and orig_destaddr); simply pass one
   // of those to "whereThingsNowReside" to figure out the new destination.
   // It's similar to a backpatch kludge done in offset32 jump tables.

   // Emit the jmp and delay slot (unchanged)
   instr_t *orig_jumpinsn_copy = instr_t::getInstr(*orig_jumpinsn);
   em.emit(orig_jumpinsn_copy);
   if (hasDelaySlot) {
      instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
      em.emit(dsInsn_copy);
   }
   
   assert(thisIsLastCodeObjectInBlock);
   
   const pdstring codeObjContainingSetHiLabelName =
      outliningLocations::getCodeObjectLabel(setHiInsnBBId,
                                             setHiCodeObjectNdxWithinBB);
   
   const pdstring codeObjContainingBSetOrAddLabelName =
      outliningLocations::getCodeObjectLabel(bsetOrAddInsnBBId,
                                             bsetOrAddCodeObjectNdxWithinBB);
   
   sparc_tempBufferEmitter::setHiBSetOrAddLocation
      setHiBSetOrAddLoc(codeObjContainingSetHiLabelName,
                        setHiByteOffsetWithinCodeObject,
                        codeObjContainingBSetOrAddLabelName,
                        bsetOrAddByteOffsetWithinCodeObject);
   
   ((sparc_tempBufferEmitter&)em).lateBackpatchOfSetHiBSetOrAddToLabelAddrPlusOffset(setHiBSetOrAddLoc,
                                                         outliningLocations::getBlockLabel(dest_bbid),
                                                         0);
      // 0 --> byte offset from the dest label
   
   // That's all we need.  All done.
}

