// power_nonDCTI_LRAndR.C
// See .h file for comments

#include "power_nonDCTI_LRAndR.h"
#include "directToMemoryEmitter.h"
#include "power_instr.h"
unsigned nonDCTI_LRAndR::conservative_numInsnBytesForRAndR() const {
   return 4*(1+1);
   //original instruction + the jump back
}

void nonDCTI_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void nonDCTI_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &,
                              const instr_t::controlFlowInfo*) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   // insn (relocated verbatim) and any usual jump to launchAddr + 4
   
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   em.emit(originsn->relocate(launchSiteAddr, currInsnAddr));

   currInsnAddr = patchAddr + em.numInsnBytesEmitted();

   //assert we can actually do the jump we are about to emit
   assert(power_instr::inRangeOfBranch(currInsnAddr, launchSiteAddr + 4));
   
   kaddrdiff_t jump_distance = launchSiteAddr + 4 - currInsnAddr;
   instr_t *theJumpBack = new power_instr(power_instr::branch, jump_distance, 0 /*not absolute */, 0 /*no link */ );
   em.emit(theJumpBack);
 
   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}
