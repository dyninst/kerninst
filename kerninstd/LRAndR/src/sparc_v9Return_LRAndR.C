// sparc_v9Return_LRAndR.C

#include "sparc_v9Return_LRAndR.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"

unsigned v9Return_LRAndR::conservative_numInsnBytesForRAndR() const {
   // just relocated return and its delay slot
   return 8;
}

void v9Return_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void v9Return_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &fn,
                              const instr_t::controlFlowInfo *) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   // v9 return insn (relocated verbatim)
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   em.emit(originsn->relocate(launchSiteAddr, currInsnAddr));

   // Finally, the relocated delay slot insn:
   currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   instr_t *relocated_delay =
         fn.get1OrigInsn(launchSiteAddr+4)->relocate(launchSiteAddr + 4,
						     currInsnAddr);
   em.emit(relocated_delay);

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <=
          conservative_numInsnBytesForRAndR());
}
