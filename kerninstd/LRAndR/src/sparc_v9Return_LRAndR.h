// sparc_v9ReturnDCTI_LRAndR.h

#ifndef _V9RETURN_LRANDR_H_
#define _V9RETURN_LRANDR_H_

#include "LRAndR.h"

class v9Return_LRAndR : public LRAndR {
 private:
   v9Return_LRAndR(const v9Return_LRAndR &);
   v9Return_LRAndR &operator=(const v9Return_LRAndR &);

 public:
   v9Return_LRAndR(kptr_t iLaunchSiteAddr,
                   const instr_t *ioriginsn,
                   const regset_t *iFreeIntRegsBeforeInsn,
                   const regset_t *iFreeIntRegsAfterInsn,
                   SpringBoardHeap &isbheap) :
      LRAndR(iLaunchSiteAddr, ioriginsn, iFreeIntRegsBeforeInsn, 
	     iFreeIntRegsAfterInsn, isbheap) {}
   ~v9Return_LRAndR() {}
   
   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif
