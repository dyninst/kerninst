// sparc_condBranch_0FreeAfterInsn_LRAndR.h
// handles conditional branches (except those which are branch-always or branch-never).
// The annul bit can be set or not.
// The branch can be integer or fp.  It can also be BPr.
// We assume that "freeRegsAfterInsn" is empty.  If it's not, you should be using the
// condBranch_1FreeAfterInsn_LRAndR class, which emits better code.

// Launcher strategy: annulled

// rAndR contents:
//    snippets; (in context of regs free *before* the orig insn)
//    b<cond> [cc/Reg] ifTaken (displacement will be 2 insns +
//                              size of jmp to launchAddr+8 plus [if !annul] 4)
//    nop
// [notTaken:]
//    if annul bit was set (we don't want to execute delay insn) then
//       emit jump to launchAddr+8 (context of regs free after insn, which is empty)
//    else (we do want to execute the delay insn)
//       emit delay instruction
//       emit jump to launchAddr+8 (context of regs free after delay slot insn, which
//                                  is unknown, so assume empty).
//    So in summary:  (1) if annul bit not set then emit delay insn, (2) emit a usual
//    jump to launchAddr+8, assuming *zero* free regs.
// [Taken:]
//    // we want to execute the delay slot insn, but we can't do it in the delay slot
//    // of a usualJump because there aren't any free regs and the usualJump will fail
//    // when trying to leave the delay slot open.  So, do delay insn and then
//    // a usual jump to the branch's destination (assuming *zero* free regs).
// 
// XXX TODO: Some functions end in a conditional branch; the if-not-taken case
// is an interprocedural fall-through.  (Among other cases, this happens all the
// time when treating instrumentation code as its own function.)
// When this happens, the above is incorrect for preReturn instrumentation, because
// it will always be executed, even when the cond branch is taken, remaining in
// the function!
// For that matter (and this isn't mutually exclusive with the above case), what
// if the if-taken branch exits the function (is interprocedural); what do we
// do about preReturn instrumentation then?
// So it seems that tail calls are not the only case where non-trivial relocation
// has to be done!  THIS BUG NEEDS TO BE ADDRESSED!
//
// Also, we have he usual delay slot bug: we assume that any delay slot instruction
// that is itself a non-dcti can trivially be relocated, but at least one case,
// "rd %pc, <rdest>" cannot!

#ifndef _COND_BRANCH_0FREE_AFTERINSN_LRANDR_H_
#define _COND_BRANCH_0FREE_AFTERINSN_LRANDR_H_

#include "LRAndR.h"

class condBranch_0FreeAfterInsn_LRAndR : public LRAndR {
 public:
   condBranch_0FreeAfterInsn_LRAndR(kptr_t iLaunchSiteAddr,
                                    const instr_t *ioriginsn,
                                    const regset_t *iFreeRegsBeforeInsn,
                                    const regset_t *iFreeRegsAfterInsn,
                                    SpringBoardHeap &isbheap);

  ~condBranch_0FreeAfterInsn_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif

