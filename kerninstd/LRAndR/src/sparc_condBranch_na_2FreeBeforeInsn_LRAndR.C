// sparc_condBranch_na_2FreeBeforeInsn_LRAndR.C
// See .h file for comments

#include "sparc_condBranch_na_2FreeBeforeInsn_LRAndR.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"

condBranch_na_2FreeBeforeInsn_LRAndR::
condBranch_na_2FreeBeforeInsn_LRAndR(kptr_t iLaunchSiteAddr,
                                     const instr_t *ioriginsn,
                                     const regset_t *iFreeRegsBeforeInsn,
                                     const regset_t *iFreeRegsAfterInsn,
                                     SpringBoardHeap &isbheap,
                                     const sparc_reg &iFreeReg1,
                                     const sparc_reg &iFreeReg2
                                     ) :
      LRAndR(iLaunchSiteAddr, ioriginsn, iFreeRegsBeforeInsn, 
	     iFreeRegsAfterInsn, isbheap),
      freeReg1(iFreeReg1),
      freeReg2(iFreeReg2) {
   assert(freeReg1 != sparc_reg::g0);
   assert(freeReg2 != sparc_reg::g0);

   assert(iFreeRegsBeforeInsn->exists(freeReg1));
   assert(iFreeRegsBeforeInsn->exists(freeReg2));
}

unsigned condBranch_na_2FreeBeforeInsn_LRAndR::
conservative_numInsnBytesForRAndR() const {
   return 7*4;
      // 2 to set taken addr, 2 to set fall-through addr (will need more in
      // a 64-bit-address world), 1 for movr, 1 for jmp, 1 for relocated ds insn.
}

void condBranch_na_2FreeBeforeInsn_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void condBranch_na_2FreeBeforeInsn_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &fn,
                              const instr_t::controlFlowInfo *cfi) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   // set r1 and r2 to takenAddr and fall-through-addr, respectively:
   const kptr_t takenAddr = launchSiteAddr + cfi->offset_nbytes;
   const kptr_t fallThroughAddr = launchSiteAddr + 8;

   // Our strategy:
   // Set freeReg1 to fall-through-address; set freeReg2 to taken-address.
   // Then move freeReg2 onto freeReg1 if the condition is true.
   // Then unconditionally jmp to freeReg1.
   em.emitImmAddr(fallThroughAddr, (reg_t &)freeReg1);
   em.emitImmAddr(takenAddr, (reg_t &)freeReg2);

   assert(cfi->fields.controlFlowInstruction);

   instr_t *i = new sparc_instr(sparc_instr::cfi2ConditionalMove
                                             (*(const sparc_instr::sparc_cfi*)cfi,
					      freeReg2, // ...move this
					      freeReg1 // ... onto this
					      ));
   em.emit(i);

   i = new sparc_instr(sparc_instr::jump, freeReg1);
   em.emit(i);

   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   instr_t *relocated_delay =
      fn.get1OrigInsn(launchSiteAddr+4)->relocate(launchSiteAddr+4,
						  currInsnAddr);
   em.emit(relocated_delay);

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}
