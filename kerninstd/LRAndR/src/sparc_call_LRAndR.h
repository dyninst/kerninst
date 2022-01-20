// sparc_call_LRAndR.h

// 1) If there is at least 1 register free after the instruction (EXCLUDING %o7), then
//    snippets
//    %o7 <- launchAddr  (No, NOT launchAddr+8)
//    usual jmp to callee (using regs free after orig insn MINUS %o7); leave ds open
//    delay instruction
//    (Note that leaving the delay slot open will succeed because we have at least 1
//     reg free.)
// 2) If there are no regs free after the instruction, then
//    snippets
//    %o7 <- launchAddr
//    delay instruction
//    usual jmp to callee (using *zero* free regs)
//
// Also, we have he usual delay slot bug: we assume that any delay slot instruction
// that is itself a non-dcti can trivially be relocated, but at least one case,
// "rd %pc, <rdest>" cannot!

#ifndef _CALL_LRANDR_H_
#define _CALL_LRANDR_H_

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
