// x86_branch_LRAndR.C
// See the .h file for comments

#include "x86_branch_LRAndR.h"
#include "x86_instr.h"
#include "x86_reg_set.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"
#include "usualJump.h"

branch_LRAndR::branch_LRAndR(kptr_t iLaunchSiteAddr,
                             const instr_t *ioriginsn,
                             const regset_t *iFreeIntRegsBeforeInsn,
                             const regset_t *iFreeIntRegsAfterInsn,
                             SpringBoardHeap &isbheap) : 
   LRAndR(iLaunchSiteAddr, 
          ioriginsn, 
          iFreeIntRegsBeforeInsn, 
          iFreeIntRegsAfterInsn,
          isbheap)
{
   x86_instr::x86_cfi cfi;
   originsn->getControlFlowInfo(&cfi);
   if(cfi.fields.controlFlowInstruction) { 
      if(cfi.destination == instr_t::controlFlowInfo::pcRelative)
         call_or_jmp_destination = (iLaunchSiteAddr + 5) + cfi.offset_nbytes;
      else
         call_or_jmp_destination = 0;
   }
}

unsigned branch_LRAndR::conservative_numInsnBytesForRAndR() const {
   unsigned numbytes = originsn->getNumBytes();
   if(numbytes <= 5)
      // if orig insn was a short jump, it will be turned into a long jump,
      // so we need to account for that (max size of long jump is 7 bytes)
      numbytes = 7;
   else
      numbytes = originsn->getNumBytes() + 5;

   return numbytes + 5; // relocated insn + jmp to [lauchSite + insn.size()]
}

void branch_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void branch_LRAndR::emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                                                  const function_t &fn,
                                                  const instr_t::controlFlowInfo*
                                                ) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();

   // Basically, we just need to emit the relocated original insn and a 
   // jmp to the insn following the original insn

   x86_instr *i = (x86_instr*)originsn->relocate(launchSiteAddr, currInsnAddr);
   assert(i != NULL);
   currInsnAddr += i->getNumBytes();
   em.emit(i);

   int32_t next_insn_disp = launchSiteAddr + originsn->getNumBytes() - 
                            (currInsnAddr + 5);
   i = new x86_instr(x86_instr::jmp, next_insn_disp); 
   em.emit(i);

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}

