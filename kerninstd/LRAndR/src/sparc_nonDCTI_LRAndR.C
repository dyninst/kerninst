// sparc_nonDCTI_LRAndR.C
// See .h file for comments

#include "sparc_nonDCTI_LRAndR.h"
#include "sparc_reg_set.h"
#include "directToMemoryEmitter.h"
#include "usualJump.h"

unsigned nonDCTI_LRAndR::conservative_numInsnBytesForRAndR() const {
   unsigned result = 4;
   
   if (freeIntRegsAfterInsn->countIntRegs() > 0)
      result += usualJump::maxNumInsnBytes1FreeReg(false); // no open ds
   else
      result += usualJump::maxNumInsnBytesNoFreeRegsNoOpenDS();

   return result;
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
   
   usualJump *theJumpBack = usualJump::allocate_appropriate(currInsnAddr, // from
                                                            launchSiteAddr + 4, // to
                                                            *(const sparc_reg_set*)freeIntRegsAfterInsn,
                                                            false // don't leave ds open
                                                            );
   theJumpBack->generate(em);

   delete theJumpBack;
   theJumpBack = NULL; // help purify find mem leaks

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}
