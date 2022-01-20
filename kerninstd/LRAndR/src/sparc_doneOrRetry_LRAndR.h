// sparc_doneOrRetryDCTI_LRAndR.h
// A particularly easy LRAndR: no delay slot; no need to jump back to
// the instruction point; relocation of the instruction itself is trivial.

// Launcher strategy: annulled

// patch contents strategy:
// snippets; (in context of regs free *before* the *orig* insn)
// insn (relocated verbatim)
// that's it!

#ifndef _DONE_OR_RETRY_LRANDR_H_
#define _DONE_OR_RETRY_LRANDR_H_

#include "LRAndR.h"

class doneOrRetry_LRAndR : public LRAndR {
 private:
   doneOrRetry_LRAndR(const doneOrRetry_LRAndR &);
   doneOrRetry_LRAndR &operator=(const doneOrRetry_LRAndR &);

 public:
   doneOrRetry_LRAndR(kptr_t iLaunchSiteAddr,
		      const instr_t *ioriginsn,
		      const regset_t *iFreeIntRegsBeforeInsn,
		      const regset_t *iFreeIntRegsAfterInsn,
		      SpringBoardHeap &isbheap) :
      LRAndR(iLaunchSiteAddr, ioriginsn, iFreeIntRegsBeforeInsn, 
	     iFreeIntRegsAfterInsn, isbheap) {}
   ~doneOrRetry_LRAndR() {}
   
   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif
