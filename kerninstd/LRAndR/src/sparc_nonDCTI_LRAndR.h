// sparc_nonDCTI_LRAndR.h
// non-delayed-control-transfer-instruction lRAndR.
// non-delayed-control-xfer-insns never have a delay slot.

// BUG: there are at least two cases where non-dcti instructions cannot be
// so easily relocated:
// (1) rd %pc, <rd>  (must change to "rd <- splice address")
// (2) when the instruction is within a delay slot

// Launcher strategy: annulled

// patch contents strategy:
// snippets; (in context of regs free *before* the *orig* insn)
// insn (relocated verbatim);
// usual jmp to launchAddr+4 (in context of regs free *after* the *orig* insn)

#ifndef _NON_DCTI_LRANDR_H_
#define _NON_DCTI_LRANDR_H_

#include "LRAndR.h"

class nonDCTI_LRAndR : public LRAndR {
 private:
   nonDCTI_LRAndR(const nonDCTI_LRAndR &);
   nonDCTI_LRAndR &operator=(const nonDCTI_LRAndR &);

 public:
   nonDCTI_LRAndR(kptr_t iLaunchSiteAddr,
                  const instr_t *ioriginsn,
                  const regset_t *iFreeIntRegsBeforeInsn,
                  const regset_t *iFreeIntRegsAfterInsn,
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
