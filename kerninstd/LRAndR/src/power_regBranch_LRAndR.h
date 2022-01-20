// power_regBranch_LRAndR.h
//This file handles register-based branches.
//


#ifndef _POWER_REG_BRANCH_LRANDR_H_
#define _POWER_REG_BRANCH_LRANDR_H_

#include "LRAndR.h"

class regBranch_LRAndR : public LRAndR {
 private:
   regBranch_LRAndR(const regBranch_LRAndR &);
   regBranch_LRAndR &operator=(const regBranch_LRAndR &);

 public:
   regBranch_LRAndR(kptr_t iLaunchSiteAddr,
		  const regset_t *iFreeIntRegsBeforeInsn,
		  const regset_t *iFreeIntRegsAfterInsn,
		  const instr_t *ioriginsn,
		  SpringBoardHeap &isbheap) : LRAndR(iLaunchSiteAddr, 
						     ioriginsn, 
						     iFreeIntRegsBeforeInsn,
						     iFreeIntRegsAfterInsn,
						     isbheap) {}
   ~regBranch_LRAndR() {}
   
   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif
