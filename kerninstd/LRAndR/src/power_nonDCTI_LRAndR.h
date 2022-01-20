// power_nonDCTI_LRAndR.h
//This file handles non-branch instructions for Power.  The nonDCTI (non-delay control transfer) titel
// is to stay consistent with SPARC.  
//


#ifndef _POWER_NON_DCTI_LRANDR_H_
#define _POWER_NON_DCTI_LRANDR_H_

#include "LRAndR.h"

class nonDCTI_LRAndR : public LRAndR {
 private:
   nonDCTI_LRAndR(const nonDCTI_LRAndR &);
   nonDCTI_LRAndR &operator=(const nonDCTI_LRAndR &);

 public:
   nonDCTI_LRAndR(kptr_t iLaunchSiteAddr,
		  const regset_t *iFreeIntRegsBeforeInsn,
		  const regset_t *iFreeIntRegsAfterInsn,
		  const instr_t *ioriginsn,
		  SpringBoardHeap &isbheap) : LRAndR(iLaunchSiteAddr, 
						     ioriginsn, 
						     iFreeIntRegsBeforeInsn,
						     iFreeIntRegsAfterInsn,
						     isbheap) {}
   ~nonDCTI_LRAndR() {}
   
   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif
