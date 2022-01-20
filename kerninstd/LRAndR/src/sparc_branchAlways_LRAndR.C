// sparc_branchAlways_LRAndR.C
// See .h file for comments

#include "sparc_branchAlways_LRAndR.h"
#include "sparc_instr.h"
#include "sparc_reg_set.h"
#include "util/h/minmax.h"
#include "usualJump.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"

unsigned branchAlways_LRAndR::conservative_numInsnBytesForRAndR() const {
   if (freeIntRegsBeforeInsn->isempty())
      return 4 + usualJump::maxNumInsnBytesNoFreeRegsNoOpenDS();
         // delay insn + usual jump
   else {
      unsigned resultIfDelaySlot = usualJump::maxNumInsnBytes1FreeRegPlusDelayIfAny(true);
      unsigned resultIfNoDelaySlot = usualJump::maxNumInsnBytes1FreeReg(false);
      return max(resultIfDelaySlot, resultIfNoDelaySlot);
   }
}

void branchAlways_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void branchAlways_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &fn,
                              const instr_t::controlFlowInfo *icfi) const {
   const sparc_instr::sparc_cfi *cfi = (const sparc_instr::sparc_cfi*)icfi;
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();
   
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   const kptr_t destaddr = launchSiteAddr + cfi->offset_nbytes;

   const bool delaySlot = !cfi->sparc_fields.annul;
   
   if (freeIntRegsBeforeInsn->isempty()) {
      // delay insn [if appropriate]; jmp to destination (using 0 regs).
      if (delaySlot) {
         // POSSIBLE BUG: we should assert that the delay insn is independent of %pc
         // (i.e., isn't rd %pc), because by putting it up here, it won't be able to
         // see the effects of the branch.
         em.emit(fn.get1OrigInsn(launchSiteAddr + 4)->relocate(launchSiteAddr + 4,
							       currInsnAddr));
      }

      currInsnAddr = patchAddr + em.numInsnBytesEmitted();
      usualJump *jump = usualJump::allocate_appropriate(currInsnAddr, // from
                                                        destaddr, // to
                                                        sparc_reg_set::emptyset,
                                                        false // don't leave ds open
                                                        );
      jump->generate(em);

      delete jump;
   }
   else {
      // jmp to destination (using regs before insn); leave ds open iff ds exists;
      // then ds (if appropriate).
      usualJump *jump = usualJump::allocate_appropriate(currInsnAddr, // from
                                                        destaddr, // to
                                                        *(const sparc_reg_set*)freeIntRegsBeforeInsn,
                                                        delaySlot);
      jump->generate(em);
      
      delete jump;

      if (delaySlot) {
         currInsnAddr = patchAddr + em.numInsnBytesEmitted();
         em.emit(fn.get1OrigInsn(launchSiteAddr + 4)->relocate(launchSiteAddr + 4,
							       currInsnAddr));
      }
   }

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}
