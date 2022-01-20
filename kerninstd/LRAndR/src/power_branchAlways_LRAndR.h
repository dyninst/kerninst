// power_branchAlways_LRAndR.h

//This handles any unconditional branch (including calls).  Try a simple relocation first.  If that fails,
//replace by an i-form branch (26-bit displacement).  This should never fail.  Assert this for now.
//If this is ever a problem, we'll need to worry about having CTR/LR free, and using bctr/blr 
//instructions.  
//
//Note that saving the return address for a call is taken care of at the launchsite. 
// At this stage, we must make sure the return address does *not* end up being saved.

#ifndef _POWER_BRANCHALWAYS_LRANDR_H_
#define _POWER_BRANCHALWAYS_LRANDR_H_

#include "LRAndR.h"

class branchAlways_LRAndR : public LRAndR {
 private:
   branchAlways_LRAndR(const branchAlways_LRAndR &);
   branchAlways_LRAndR& operator=(const branchAlways_LRAndR &);

   kptr_t branchAlways_destination;

 public:
   branchAlways_LRAndR(kptr_t iLaunchSiteAddr,
		       const regset_t *iFreeIntRegsBeforeInsn,
		       const regset_t *iFreeIntRegsAfterInsn,	       
		       const instr_t *ioriginsn,
		       SpringBoardHeap &);

   ~branchAlways_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo*) const;
};

#endif
