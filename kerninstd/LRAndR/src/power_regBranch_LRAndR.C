// power_regBranch_LRAndR.C
// See .h file for comments

#include "power_regBranch_LRAndR.h"
#include "directToMemoryEmitter.h"
#include "power_instr.h"
unsigned regBranch_LRAndR::conservative_numInsnBytesForRAndR() const {
   return 8;
   //just the original instruction + jump back if linking
}

void regBranch_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void regBranch_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &,
                              const instr_t::controlFlowInfo*) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();
   power_instr::power_cfi cfi;
   originsn->getControlFlowInfo(&cfi);

   //Just copy the insn (relocated verbatim).   
   
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   em.emit(originsn->relocate(launchSiteAddr, currInsnAddr));

   currInsnAddr = patchAddr + em.numInsnBytesEmitted();

   const bool condBranch =  cfi.fields.isConditional;
   
   //Notice how whether it's a conditional instruction or one that links,
   //(or both, oy!) we need to do exactly the same thing: emit a jump
   //back to the next instruction.
   if (cfi.power_fields.link || condBranch) {
      //need a jump back.  The alternative of emitting the address to
      //jump back to into LR is even less enticing, as it will involve
      //about 8 instructions (5 to load immediate into temp reg,
      // 1 to move into LR, 2 to save/restore temp reg).
        
      //assert we can actually do the jump we are about to emit
      assert(power_instr::inRangeOfBranch(currInsnAddr, launchSiteAddr + 4));
      
      kaddrdiff_t jump_distance = launchSiteAddr + 4 - currInsnAddr;
      instr_t *theJumpBack = new power_instr(power_instr::branch, jump_distance, 0 /*not absolute */, 0 /*no link */ );
      em.emit(theJumpBack);
   }

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}
