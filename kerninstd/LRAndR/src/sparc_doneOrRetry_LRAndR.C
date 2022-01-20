// sparc_doneOrRetry_LRAndR.C
// SPARC v9 done & retry instructions: return from trap
// The easiest kind of instruction to relocate: see .h file for comments

#include "sparc_doneOrRetry_LRAndR.h"
#include "directToMemoryEmitter.h"

unsigned doneOrRetry_LRAndR::conservative_numInsnBytesForRAndR() const {
   // really easy: no need to jump back to instrumentation point after relocated
   // insn.
   return 4;
}

void doneOrRetry_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void doneOrRetry_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &,
                              const instr_t::controlFlowInfo *) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   // insn (relocated verbatim)
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   em.emit(originsn->relocate(launchSiteAddr, currInsnAddr));

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <=
          conservative_numInsnBytesForRAndR());
}
