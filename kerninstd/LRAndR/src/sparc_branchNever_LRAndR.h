// sparc_branchNever_LRAndR.h
// handles the weird cases of bn and bn,a
// The branch could be a bicc, bpcc, fbfcc, or fbpfcc
//
// Case 1) freeIntRegsBeforeInsn is empty
//    snippets
//    delay instruction, if exists
//    usual jump to launchSiteAddr + 8 using no free regs; don't leave ds open
// Case 2) freeIntRegsBeforeInsn has at least 1 reg
//    snippets
//    usual jump to launchSiteAddr + 8 using regs free before insn; leave ds open iff
//       delay instruction exists
//    delay instruction, if exists
//
// Note that the rAndR contents are the same whether it's an int or fp branch never, and
// whether it's predicted or not.
//
// TO DO: if possible (and it usally ain't because we're out of displacement range),
// try and keep the bpn,a's insn prefetch semantics.

#ifndef _BRANCH_NEVER_LRANDR_H_
#define _BRANCH_NEVER_LRANDR_H_

#include "LRAndR.h"
#include "sparc_instr.h"

class branchNever_LRAndR : public LRAndR {
 private:
   template <class emitter>
   unsigned branchNever_LRAndR::
   emit_RAndR(emitter &theEmitter,
              const function_t &fn,
              const sparc_instr::sparc_cfi &cfi) const;

 public:
   branchNever_LRAndR(kptr_t iLaunchSiteAddr,
                      const instr_t *ioriginsn,
                      const regset_t *iFreeIntRegsBeforeInsn,
                      const regset_t *iFreeIntRegsAfterInsn,
                      SpringBoardHeap &isbheap) :
      LRAndR(iLaunchSiteAddr, ioriginsn, iFreeIntRegsBeforeInsn, 
	     iFreeIntRegsAfterInsn, isbheap) {}

   ~branchNever_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif
