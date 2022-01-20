// codeObject.C

#include "codeObject.h"

#ifdef sparc_sun_solaris2_7
#include "pcIndepWithFallThruCodeObject.h"
#include "pcIndepNoFallThruCodeObject.h"
#include "interProcCallCodeObject.h"
#include "interProcTailCallCodeObject.h"
#include "callToObtainPCCodeObject.h"
#include "condBranchCodeObject.h"
#include "branchAlwaysCodeObject.h"
#include "interProcCondBranchCodeObject.h"
#include "interProcBranchAlwaysCodeObject.h"
#include "simpleJumpTable32CodeObject.h"
#include "offsetJumpTable32CodeObject.h"
#include "recursiveCallCodeObject.h"
#include "intraProcJmpToFixedAddrCodeObject.h"
#endif

#include "util/h/xdr_send_recv.h"
#include "util/h/rope-utils.h"
#include "instr.h"
#include "abi.h"
#include "moduleMgr.h" // findModAndFn()
#include "outliningMisc.h" // for getOutlinedFnModName() and getOutlinedFnName()

codeObject::bbid_t codeObject::read1_bb_id(XDR *xdr) {
   // a static method
   bbid_t result;
   if (!P_xdr_recv(xdr, result))
      throw xdr_recv_fail();
   return result;
}

// ----------------------------------------------------------------------

codeObject *codeObject::make(XDR *xdr) {
#ifdef sparc_sun_solaris2_7
   uint32_t id;
   if (!P_xdr_recv(xdr, id))
      throw xdr_recv_fail();
   
   switch (id) {
      case pcIndepWithFallThruCodeObjectID:
         return new pcIndepWithFallThruCodeObject(xdr);
      case pcIndepNoFallThruCodeObjectID:
         return new pcIndepNoFallThruCodeObject(xdr);
      case interProcCallToAddrCodeObjectID:
         return new interProcCallToAddrCodeObject(xdr);
      case interProcCallToNameCodeObjectID:
         return new interProcCallToNameCodeObject(xdr);
      case interProcTailCallToAddrCodeObjectID:
         return new interProcTailCallToAddrCodeObject(xdr);
      case interProcTailCallToNameCodeObjectID:
         return new interProcTailCallToNameCodeObject(xdr);
      case callToObtainPCCodeObjectID:
         return new callToObtainPCCodeObject(xdr);
      case condBranchCodeObjectID:
         return new condBranchCodeObject(xdr);
      case branchAlwaysCodeObjectID:
         return new branchAlwaysCodeObject(xdr);
      case interProcCondBranchToAddrCodeObjectID:
         return new interProcCondBranchToAddrCodeObject(xdr);
      case interProcCondBranchToNameCodeObjectID:
         return new interProcCondBranchToNameCodeObject(xdr);
      case interProcBranchAlwaysToAddrCodeObjectID:
         return new interProcBranchAlwaysToAddrCodeObject(xdr);
      case interProcBranchAlwaysToNameCodeObjectID:
         return new interProcBranchAlwaysToNameCodeObject(xdr);
      case simpleJumpTable32CodeObjectID:
         return new simpleJumpTable32CodeObject(xdr);
      case offsetJumpTable32CodeObjectID:
         return new offsetJumpTable32CodeObject(xdr);
      case recursiveCallCodeObjectID:
         return new recursiveCallCodeObject(xdr);
      case intraProcJmpToFixedAddrCodeObjectID:
         return new intraProcJmpToFixedAddrCodeObject(xdr);
      default:
         assert(false && "unrecognized codeObject id over xdr connection");
   }
#endif
   return NULL; // placate compiler viz. return type
}

bool codeObject::send(XDR *xdr) const {
   return (P_xdr_send(xdr, getCodeObjectTypeID()) &&
           send(false, xdr));
}

void codeObject::
emitGettingToIntraProcFallee(tempBufferEmitter &em,
                             const outliningLocations &whereBlocksNowReside,
                             regset_t *availRegs,
                             bbid_t ourBlockId,
                             bbid_t destBlockId) const {
#ifdef sparc_sun_solaris2_7
   assert(destBlockId != (bbid_t)-1);
      // interproc fall-thru case should have been CAUGHT and avoided before
      // calling this routine

   const bbid_t blockEmittedNextIfAny =
      whereBlocksNowReside.whoIfAnyoneWillBeEmittedAfter(ourBlockId);
   // hopefully, this will equal destBlockId, for then we can truly just
   // emit nothing, and fall through, to get to destBlockId.

   // If it's a sentinel ((bbid_t)-1), then no block will be emitted
   // immediately following this one.  No, that's not cause for an assertion failure.

   if (blockEmittedNextIfAny == destBlockId)
      // rejoice!
      return;

   const bool ourBlockIsInlined = whereBlocksNowReside.isBlockInlined(ourBlockId);
   const bool destBlockIsInlined = whereBlocksNowReside.isBlockInlined(destBlockId);
   const bool bothBlocksInSameGroup = (ourBlockIsInlined == destBlockIsInlined);

   if (bothBlocksInSameGroup ||
       whereBlocksNowReside.getWhereOutlinedBlocksReside() ==
       outliningLocations::afterInlinedBlocks) {
      // Pretty good; only a single insn needed.
      const pdstring labelStr = whereBlocksNowReside.getBlockLabel(destBlockId);

      instr_t *i = (instr_t*) new sparc_instr(sparc_instr::bpcc,
			       sparc_instr::iccAlways,
			       true, // annulled
			       true, // predict taken
			       true, // xcc (matters?)
			       0 // displacement for now
			       );
      em.emitCondBranchToLabel(i, labelStr);
      return;
   }

   // If outlined blocks are being emitted far away from inlined blocks, and if
   // destBlockId is in the outlined group while ourBlockId is in the inlined group,
   // then assert fail, since outlined blocks are supposed to be emitted first.
   if (!destBlockIsInlined && ourBlockIsInlined)
      assert(false && "outlined blocks should have already been emitted");
   
   // OK I give up on a single-instruction jump.

   // Use labelAddr32() to represent the dest block id.  If a free reg exists, then
   // the sequence is obvious (labelAddr32(), jmp, nop).  If no free reg exists,
   // then save, labelAddr32(), jmp, restore.

   const pdstring destLabelName = whereBlocksNowReside.getBlockLabel(destBlockId);
      
   assert(!availRegs->exists(sparc_reg::g0));
         
   if (availRegs->isempty()) {
      instr_t *i = (instr_t*) new sparc_instr(sparc_instr::save,
			       -em.getABI().getMinFrameAlignedNumBytes());
      em.emit(i);
      em.emitLabelAddr(destLabelName, sparc_reg::l0);
      i = (instr_t*) new sparc_instr(sparc_instr::jump, sparc_reg::l0);
      em.emit(i);
      i = (instr_t*) new sparc_instr(sparc_instr::restore);
      em.emit(i);
      return;
   }
   else if (availRegs->exists(sparc_reg::o7)) {
      // call; nop
      em.emitCall(destLabelName);
      instr_t *nop = (instr_t*) new sparc_instr(sparc_instr::nop);
      em.emit(nop);
   }
   else {
      regset_t::const_iterator iter = availRegs->begin();
      sparc_reg &r = (sparc_reg &)*iter;
      assert(r != sparc_reg::g0);
      em.emitLabelAddr(destLabelName, r);
      instr_t *i = (instr_t*) new sparc_instr(sparc_instr::jump, r);
      em.emit(i);
      i = (instr_t*) new sparc_instr(sparc_instr::nop);
      em.emit(i);
      return;
   }
   
   assert(false);
#endif
}

void codeObject::
emitGettingToInterProcFallee(tempBufferEmitter &em,
                             const outliningLocations &whereBlocksNowReside,
                             bool leaveDelaySlotOpen,
                                // only pass true if you know we can achieve this
                                // (which is if availRegs contains at least 1 scratch
                                // int reg)
                             const regset_t &availRegs,
                             const function_t &parentFn,
                             bbid_t ourBlockId) const {
#ifdef sparc_sun_solaris2_7
   const kptr_t fallThruOrigAddr = parentFn.getBBInterProcFallThruAddrIfAny(ourBlockId);
      // will return (kptr_t)-1 if "ourBlockId" doesn't have an interprocedural
      // fall-thru.  That should now happen here, so we assert it:
   assert(fallThruOrigAddr != (kptr_t)-1 &&
          "emitGettingToInterProcFallee(): expected an interprocedural fall-thru");

   // We need the address of the destination function.  If the function isn't being
   // inlined/outlined, then it remains unchanged at "fallThruOrigAddr".  But otherwise,
   // we don't yet know its address, so we emit symAddr32s, and so on.

   if (whereBlocksNowReside.isFuncBeingInlinedAndOutlined(fallThruOrigAddr)) {
      // We don't yet know its address, just its name

      extern moduleMgr *global_moduleMgr;
      const pair<pdstring, const function_t*> destOrigModAndFn = 
         global_moduleMgr->findModAndFn(fallThruOrigAddr, true);
      assert(destOrigModAndFn.second != NULL);
         
      const pdstring outlinedFnFullName =
         outliningMisc::getOutlinedFnModName(whereBlocksNowReside.getGroupUniqueId())
            + "/" +
         outliningMisc::getOutlinedFnName(destOrigModAndFn.first,
                                          *destOrigModAndFn.second);

      if (availRegs.isempty()) {
         assert(!leaveDelaySlotOpen);
	 instr_t *i = (instr_t*) new sparc_instr(sparc_instr::save,
				  -em.getABI().getMinFrameAlignedNumBytes());
         em.emit(i);
         em.emitSymAddr(outlinedFnFullName, sparc_reg::l0);
	 i = (instr_t*) new sparc_instr(sparc_instr::jump, sparc_reg::l0);
         em.emit(i);
	 i = (instr_t*) new sparc_instr(sparc_instr::restore); // delay slot
         em.emit(i);
      }
      else {
         reg_t &r = *availRegs.begin();
         assert(r.isIntReg());
         
         em.emitSymAddr(outlinedFnFullName, r);
	 instr_t *i = (instr_t*) new sparc_instr(sparc_instr::jump, sparc_reg::l0);
         em.emit(i);
         if (!leaveDelaySlotOpen)
	    i = (instr_t*) new sparc_instr(sparc_instr::nop);
            em.emit(i);
      }
   }
   else {
      // the destination function isn't moving -- still at fallThruOrigAddr
      // So it's easy to get to

      if (availRegs.isempty()) {
         assert(!leaveDelaySlotOpen);
	 instr_t *i = (instr_t*) new sparc_instr(sparc_instr::save,
				  -em.getABI().getMinFrameAlignedNumBytes());
         em.emit(i);
         em.emitImmAddr(fallThruOrigAddr, sparc_reg::l0);
         i = (instr_t*) new sparc_instr(sparc_instr::jump, sparc_reg::l0);
	 em.emit(i);
	 i = (instr_t*) new sparc_instr(sparc_instr::restore);
         em.emit(i);
            // delay slot
      }
      else {
         reg_t &r = *availRegs.begin();
         assert(r.isIntReg());
         
         em.emitImmAddr(fallThruOrigAddr, r);
	 instr_t *i = (instr_t*) new sparc_instr(sparc_instr::jump, sparc_reg::l0);
         em.emit(i);
         if (!leaveDelaySlotOpen) {
	    i = (instr_t*) new sparc_instr(sparc_instr::nop);
            em.emit(i);
	 }
      }
   }
#endif
}

void codeObject::
emitSetRegToInterProcFalleeAddr(tempBufferEmitter &em,
                                const outliningLocations &whereBlocksNowReside,
                                reg_t &dest_reg,
                                regset_t &/*availRegs*/,
                                   // we might need this some day, to obtain a scratch
                                   // reg to set a 64-bit address, for example.
                                const function_t &parentFn,
                                bbid_t ourBlockId) const {
   // similar to emitGettingToInterProcFallee, except we're just setting a register.
   // Useful in certain circumstances.
   // NOTE: It would be good to share some code with
   // the above emitGettingToInterProcFallee()...

   const kptr_t fallThruOrigAddr = parentFn.getBBInterProcFallThruAddrIfAny(ourBlockId);
   assert(fallThruOrigAddr != (kptr_t)-1 &&
          "emitGettingToInterProcFallee(): expected an interprocedural fall-thru");

   if (whereBlocksNowReside.isFuncBeingInlinedAndOutlined(fallThruOrigAddr)) {
      // We don't yet know its address, just its name

      extern moduleMgr *global_moduleMgr;
      const pair<pdstring, const function_t*> destOrigModAndFn = 
         global_moduleMgr->findModAndFn(fallThruOrigAddr, true);
      assert(destOrigModAndFn.second != NULL);
         
      const pdstring outlinedFnFullName =
         outliningMisc::getOutlinedFnModName(whereBlocksNowReside.getGroupUniqueId())
            + "/" +
         outliningMisc::getOutlinedFnName(destOrigModAndFn.first,
                                          *destOrigModAndFn.second);         

      em.emitSymAddr(outlinedFnFullName, dest_reg);
   }
   else {
      // the destination function isn't moving -- still at fallThruOrigAddr.

      em.emitImmAddr(fallThruOrigAddr, dest_reg);
         // works even for 64-bit addrs, for now at least, since addrs are for now
         // at most 44 bits, so no scratch regs are needed.
   }
}
