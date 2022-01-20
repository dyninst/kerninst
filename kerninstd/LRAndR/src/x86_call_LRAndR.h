// x86_call_LRAndR.h
//The snippet after preinsn is simply a jump to callee (16 or 32-bit 
//depending on the distance).

#ifndef _X86_CALL_LRANDR_H_
#define _X86_CALL_LRANDR_H_

#include "LRAndR.h"

class call_LRAndR : public LRAndR {
 private:
   call_LRAndR(const call_LRAndR &);
   call_LRAndR& operator=(const call_LRAndR &);

   kptr_t call_destination;

 public:
   call_LRAndR(kptr_t iLaunchSiteAddr,
               const instr_t *ioriginsn,
               const regset_t *iFreeIntRegsBeforeInsn,
               const regset_t *iFreeIntRegsAfterInsn,
               SpringBoardHeap &);

   ~call_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo*) const;
};

#endif
