// power_condBranch_LRAndR.C
// See the .h file for comments

#include "power_condBranch_LRAndR.h"
#include "power_instr.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"


condBranch_LRAndR::condBranch_LRAndR(kptr_t iLaunchSiteAddr,
				     const regset_t *iFreeIntRegsBeforeInsn,
				     const regset_t *iFreeIntRegsAfterInsn,
				     const instr_t *ioriginsn,
				     SpringBoardHeap &isbheap) : LRAndR(iLaunchSiteAddr,
								ioriginsn, 
								iFreeIntRegsBeforeInsn,
								iFreeIntRegsAfterInsn,
								isbheap)
{
   power_instr::power_cfi cfi;
   ioriginsn->getControlFlowInfo(&cfi);
   assert(cfi.fields.controlFlowInstruction && cfi.fields.isConditional);
   if (cfi.destination == instr_t::controlFlowInfo::pcRelative)
     condBranch_destination = iLaunchSiteAddr + cfi.offset_nbytes;
   else {
     assert(cfi.destination == instr_t::controlFlowInfo::absolute);
     condBranch_destination = cfi.offset_nbytes;
   }
       
}

unsigned condBranch_LRAndR::conservative_numInsnBytesForRAndR() const {
  return 4*(1+1+1+1);
  //worst case: short-range conditional jump + unconditional jump to destination + not-taken jump + jump back for linking purposes
}

void condBranch_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void condBranch_LRAndR::emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                                                const function_t &fn,
                                                const instr_t::controlFlowInfo*
                                                ) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   bool relSuccess = false;
   power_instr *origBranch = (power_instr *)   fn.get1OrigInsn(launchSiteAddr);
   power_instr::power_cfi cfi;
   origBranch->getControlFlowInfo(&cfi);
   instr_t *relocatedBranch = 
     origBranch->tryRelocation(launchSiteAddr, currInsnAddr, &relSuccess);
   if (relSuccess) {
     em.emit(relocatedBranch);
   } else {
     //double check that we really are out of range
     assert(!power_instr::inRangeOfBranchCond(launchSiteAddr, currInsnAddr));
     assert(cfi.destination == instr_t::controlFlowInfo::pcRelative); //if absolute, how can it be out of range ?
     //this better work
     assert(power_instr::inRangeOfBranch(launchSiteAddr, currInsnAddr));
     
     //replace a "conditional" branch with a short-range conditional branch + a  long jump
     delete relocatedBranch; //useless now
     
     //the new short-range branch has same condition as the original, but a displacement of 8
     instr_t *shortBranch = new power_instr(power_instr::branchcond, origBranch->getBOField(),
					    origBranch->getBIField(), 8, 0, 0);
     em.emit(shortBranch);

   }
   currInsnAddr+=4;

   //jump back to "not-taken" case
   assert(power_instr::inRangeOfBranch(currInsnAddr, launchSiteAddr + 4));
   kaddrdiff_t jump_distance = launchSiteAddr + 4 - currInsnAddr;
   instr_t *theJumpBack = new power_instr(power_instr::branch, jump_distance, 0 /*not absolute */, 0 /*no link */ );
   em.emit(theJumpBack);
   currInsnAddr += 4;
   if (!relSuccess) {
     //the jump to taken case
     kaddrdiff_t new_displacement = launchSiteAddr + cfi.offset_nbytes - currInsnAddr;
     relocatedBranch = new power_instr(power_instr::branch, new_displacement, 0/*not absolute */ , cfi.power_fields.link /* linking as per orig insn */); 
     em.emit(relocatedBranch);
     currInsnAddr += 4;
   }
   
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

