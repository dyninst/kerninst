// power_branchAlways_LRAndR.C
// See the .h file for comments

#include "power_branchAlways_LRAndR.h"
#include "power_instr.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"


branchAlways_LRAndR::branchAlways_LRAndR(kptr_t iLaunchSiteAddr,
					 const regset_t *iFreeIntRegsBeforeInsn,
					 const regset_t *iFreeIntRegsAfterInsn,
					 const instr_t *ioriginsn,
					 SpringBoardHeap &isbheap) : 
  LRAndR(iLaunchSiteAddr,
	 ioriginsn, 
	 iFreeIntRegsBeforeInsn,
	 iFreeIntRegsAfterInsn,
	 isbheap)
{
   power_instr::power_cfi cfi;
   ioriginsn->getControlFlowInfo(&cfi);
   assert(cfi.fields.controlFlowInstruction && cfi.fields.isAlways);
   if (cfi.destination == instr_t::controlFlowInfo::pcRelative)
     branchAlways_destination = iLaunchSiteAddr + cfi.offset_nbytes;
   else {
     assert(cfi.destination == instr_t::controlFlowInfo::absolute);
     branchAlways_destination = cfi.offset_nbytes;
   }
       
}

unsigned branchAlways_LRAndR::conservative_numInsnBytesForRAndR() const {
  return 8;
  //relocated jump + possibly a jump back if linked.
}

void branchAlways_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void branchAlways_LRAndR::emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                                                const function_t &fn,
                                                const instr_t::controlFlowInfo*
                                                ) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   bool relSuccess = false;
   power_instr *originsn = (power_instr *)fn.get1OrigInsn(launchSiteAddr);
   instr_t *relocatedBranch = 
     originsn->tryRelocation(launchSiteAddr, currInsnAddr, &relSuccess);
   power_instr::power_cfi cfi;
   originsn->getControlFlowInfo(&cfi);

   if (relSuccess)
     em.emit(relocatedBranch);
   else {
     
     //double check that we really are out of range
     assert(!power_instr::inRangeOfBranchCond(launchSiteAddr, currInsnAddr));
     assert(cfi.destination == instr_t::controlFlowInfo::pcRelative); //if absolute, how can it be out of range ?
     //this better work
     assert(power_instr::inRangeOfBranch(launchSiteAddr, currInsnAddr));
     
     //replace a "conditional" branch always with a long jump
     delete relocatedBranch; //useless now

     kaddrdiff_t new_displacement = launchSiteAddr + cfi.offset_nbytes - currInsnAddr;
     relocatedBranch = new power_instr(power_instr::branch, new_displacement, 0/*not absolute */ , cfi.power_fields.link /* linking as per orig insn */); 
     em.emit(relocatedBranch);

   }
   currInsnAddr += 4;

   if (cfi.power_fields.link) {
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

