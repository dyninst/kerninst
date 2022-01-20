// sparc_call_LRAndR.C
// See the .h file for comments

#include "sparc_call_LRAndR.h"
#include "sparc_instr.h"
#include "sparc_reg_set.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"
#include "usualJump.h"

call_LRAndR::call_LRAndR(kptr_t iLaunchSiteAddr,
                         const instr_t *ioriginsn,
                         const regset_t *iFreeIntRegsBeforeInsn,
                         const regset_t *iFreeIntRegsAfterInsn,
                         SpringBoardHeap &isbheap) : LRAndR(iLaunchSiteAddr, 
							    ioriginsn, 
							    iFreeIntRegsBeforeInsn, 
							    iFreeIntRegsAfterInsn,
							    isbheap)
{
   sparc_instr::sparc_cfi cfi;
   ioriginsn->getControlFlowInfo(&cfi);
   assert(cfi.fields.controlFlowInstruction && cfi.fields.isCall);
   call_destination = iLaunchSiteAddr + cfi.offset_nbytes;
}

unsigned call_LRAndR::conservative_numInsnBytesForRAndR() const {
   return 4*(2 + 4 + 1);
      // 2 to set o7, usual jump, and delay insn
}

void call_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void call_LRAndR::emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                                                const function_t &fn,
                                                const instr_t::controlFlowInfo*
                                                ) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   em.emitImmAddr(launchSiteAddr, sparc_reg::o7);

   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();

   const sparc_reg_set freeIntRegsAfterInsnMinusO7 = *(const sparc_reg_set*)freeIntRegsAfterInsn - sparc_reg::o7;
   
   if (freeIntRegsAfterInsnMinusO7.isempty()) {
      // delay insn, jmp to callee (using zero free regs)
      em.emit(fn.get1OrigInsn(launchSiteAddr + 4)->
	      relocate(launchSiteAddr + 4, currInsnAddr));
      currInsnAddr = patchAddr + em.numInsnBytesEmitted();
      usualJump *jumpToCallee =
         usualJump::allocate_appropriate(currInsnAddr, // from
                                         call_destination, // to
                                         sparc_reg_set::emptyset, // using 0 free regs
                                         false // don't leave delay slot open
                                         );
      jumpToCallee->generate(em);
      
      delete jumpToCallee;
   }
   else {
      // We have a free reg after the insn, so leaving the delay slot open will
      // succeed.  Make sure that the usual jump DOES NOT OVERWRITE %o7 (note that we
      // use freeIntRegsAfterInsnMinusO7 for the usualJump).
      kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
      usualJump *jumpToCallDest =
         usualJump::allocate_appropriate(currInsnAddr, // from
                                         call_destination, // to
                                         freeIntRegsAfterInsnMinusO7,
                                         true // leave ds open
                                         );
      jumpToCallDest->generate(em);
      delete jumpToCallDest;

      // relocated ds insn
      currInsnAddr = patchAddr + em.numInsnBytesEmitted();
      em.emit(fn.get1OrigInsn(launchSiteAddr + 4)->
	      relocate(launchSiteAddr + 4, currInsnAddr));
   }

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}

