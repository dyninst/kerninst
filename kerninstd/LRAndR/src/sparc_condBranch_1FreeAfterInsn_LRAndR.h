// sparc_condBranch_1FreeAfterInsn_LRAndR.h
// handles conditional branches (except those which are branch-always or branch-never).
// The annul bit can be set or not.
// The branch can be integer or fp.
// There must be at least one int reg in "freeRegsAfterInsn".
//
// If the annul bit isn't set, and if there are 2 regs free before the insn, use the
// optimized condBranch_2Free_na_LRAndR instead.

// Launcher strategy: annulled

// rAndR contents:
//    snippets; (in context of regs free *before* the orig insn)
//    b<cond> icc/fcc/xcc/fccnum ifTaken (will be 2 insns + size of jmp to launchAddr+8)
//    nop
// [notTaken:]
//    usual jmp to launchAddr+8 [in ctx of regs free after orig insn].  If annul bit
//       was *not* set then leave ds slot open and put i+1 there.  (Note that leaving
//       the delay slot open will succeed because we have at least 1 free reg in
//       freeRegsAfterInsn.)
// [Taken:]
//    usual jmp to branch's destination [ctx of regs free after orig insn]; leave ds
//       open; and then emit delay insn.    (Note that leaving the delay slot open
//       will succeed because we have at least 1 free reg in freeRegsAfterInsn.)
//
// XXX TODO: Please see condBranch_0FreeAfterInsn_LRAndR.h for an important
// bug: instrumentation snippets shouldn't always go before the relocated branch;
// in particular, what about preReturn instrumentation when the fall-through case
// goes to another function?  For that matter, what if the if-taken case goes
// to another function (perhaps also, perhaps only the if-taken case does).
//
// Also, we have he usual delay slot bug: we assume that any delay slot instruction
// that is itself a non-dcti can trivially be relocated, but at least one case,
// "rd %pc, <rdest>" cannot!

#ifndef _COND_BRANCH_1FREE_AFTERINSN_LRANDR_H_
#define _COND_BRANCH_1FREE_AFTERINSN_LRANDR_H_

#include "LRAndR.h"

class condBranch_1FreeAfterInsn_LRAndR : public LRAndR {
 public:
   condBranch_1FreeAfterInsn_LRAndR(kptr_t iLaunchSiteAddr,
                                    const instr_t *ioriginsn,
                                    const regset_t *iFreeRegsBeforeInsn,
                                    const regset_t *iFreeRegsAfterInsn,
                                    SpringBoardHeap &isbheap);

  ~condBranch_1FreeAfterInsn_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif

