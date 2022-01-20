// sparc_branchNever_LRAndR.C
// See .h file for comments

#include "sparc_branchNever_LRAndR.h"
#include "sparc_reg_set.h"
#include "util/h/minmax.h"
#include "usualJump.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"

unsigned branchNever_LRAndR::conservative_numInsnBytesForRAndR() const {
   if (freeIntRegsBeforeInsn->isempty())
      // delay insn, if appropriate, and then a usual jump (ds not left open) with
      // no avail regs
      return 4 + usualJump::maxNumInsnBytesNoFreeRegsNoOpenDS();
   else {
      // usual jump, leaving ds open iff ds exists
      const unsigned resultIfDelaySlot = usualJump::maxNumInsnBytes1FreeRegPlusDelayIfAny(true);
      const unsigned resultIfNoDelaySlot = usualJump::maxNumInsnBytes1FreeReg(false);
      return max(resultIfDelaySlot, resultIfNoDelaySlot);
   }
}

void branchNever_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void branchNever_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &fn,
                              const instr_t::controlFlowInfo *cfi) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   const kptr_t destaddr = launchSiteAddr + 8; // return *after* the delay insn
   
   // In a branch-never, the delay slot always exists.  It is either
   // always executed (if not annulled) or never executed (if annulled).
   // Here, we treat the latter case like ba,a (pretend there is no delay slot).

   assert(cfi->delaySlot == instr_t::dsAlways ||
          cfi->delaySlot == instr_t::dsNever);
   const bool delaySlot = (cfi->delaySlot == instr_t::dsAlways);

   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();

   if (freeIntRegsBeforeInsn->isempty()) {
      // delay insn (if appropriate)
      // usual jump to launchSiteAddr + 8 (using no free regs); don't leave ds open
      if (delaySlot)
         em.emit(fn.get1OrigInsn(launchSiteAddr + 4)->relocate(launchSiteAddr + 4,
							       currInsnAddr));

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
      // usual jump to launchSiteAddr + 8 using regs free before insn; leave ds open
      //    iff exists a delay slot insn
      // delay slot insn, if appropriate
      usualJump *jump =
         usualJump::allocate_appropriate(currInsnAddr, // from
                                         destaddr, // to
                                         *(const sparc_reg_set*)freeIntRegsBeforeInsn,
                                         delaySlot // leave ds open iff delay slot
                                         );
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
