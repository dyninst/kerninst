// x86_branch_LRAndR.h

#ifndef _X86_BRANCH_LRANDR_H_
#define _X86_BRANCH_LRANDR_H_

#include "LRAndR.h"

class branch_LRAndR : public LRAndR {
 private:
   branch_LRAndR(const branch_LRAndR &);
   branch_LRAndR& operator=(const branch_LRAndR &);

   kptr_t call_or_jmp_destination;

 public:
   branch_LRAndR(kptr_t iLaunchSiteAddr,
                 const instr_t *ioriginsn,
                 const regset_t *iFreeIntRegsBeforeInsn,
                 const regset_t *iFreeIntRegsAfterInsn,
                 SpringBoardHeap &);

   ~branch_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo*) const;
};

#endif
