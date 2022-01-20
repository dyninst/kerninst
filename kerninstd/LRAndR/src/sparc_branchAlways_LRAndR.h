// sparc_branchAlways_LRAndR.h
// handles all branch-always (be it from bicc, bpcc, fbfcc, or fbpfcc)
//
// Case 1: if freeIntRegsBeforeInsn is empty
//    snippets
//    delay insn (iff the orig insn was NOT annulled)
//    usual jmp to destination, using zero free regs.  Do not leave ds open.
// Case 2: if freeIntRegsBeforeInsn is not empty
//    snippets
//    usual jmp to destination (using regs free before orig insn).  Leave delay slot
//       open iff the orig branch was NOT annulled).
//    The delay slot insn, iff the orig branch was not annulled.
//
// XXX TODO: Please see condBranch_0FreeAfterInsn_LRAndR.h for an important bug.
// Perhaps it doesn't apply to branchAlways, however.  (If any instrumentation
// is preReturn, this case should, coincidentally, put such instrumentation in the
// exact same spot as preInstruction instrumentation, right?  Not so easy for
// branches that are truly conditional [as opposed to branchAlways], however.)
//
// Also, we have he usual delay slot bug: we assume that any delay slot instruction
// that is itself a non-dcti can trivially be relocated, but at least one case,
// "rd %pc, <rdest>" cannot!

#ifndef _BRANCH_ALWAYS_LRANDR_H_
#define _BRANCH_ALWAYS_LRANDR_H_

#include "LRAndR.h"

class branchAlways_LRAndR : public LRAndR {
 public:
   branchAlways_LRAndR(kptr_t iLaunchSiteAddr,
                       const instr_t *ioriginsn,
                       const regset_t *iFreeIntRegsBeforeInsn,
                       const regset_t *iFreeIntRegsAfterInsn,
                       SpringBoardHeap &isbheap) :
      LRAndR(iLaunchSiteAddr, ioriginsn, iFreeIntRegsBeforeInsn, 
	     iFreeIntRegsAfterInsn, isbheap) {}

  ~branchAlways_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif
