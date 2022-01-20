// power_condBranch_LRAndR.h

//this class takes care of conditional branches on Power.  We attempt simple relocation first.
//If that fails, we need to replace the conditional branch (smaller range) with an unconditional
//jump to the taken case.  Since Power does not have conditional arithmetical instructions (to my 
// knowledge), we are forced to use an extra short-distance conditional branch.  Whether or not
//we replace the conditional branch, we add an unconditional jump to the "not taken" case.
//
//Note that any linking (i.e. call-like behavior) is taken care of at launchsite level: we save the return
//address there by using an instruction with the link bit set.

#ifndef _POWER_CONDBRANCH_LRANDR_H_
#define _POWER_CONDBRANCH_LRANDR_H_

#include "LRAndR.h"

class condBranch_LRAndR : public LRAndR {
 private:
   condBranch_LRAndR(const condBranch_LRAndR &);
   condBranch_LRAndR& operator=(const condBranch_LRAndR &);

   kptr_t condBranch_destination;

 public:
   condBranch_LRAndR(kptr_t iLaunchSiteAddr,
		     const regset_t *iFreeIntRegsBeforeInsn,
		     const regset_t *iFreeIntRegsAfterInsn,
		     const instr_t *ioriginsn,
		     SpringBoardHeap &);

   ~condBranch_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo*) const;
};

#endif
