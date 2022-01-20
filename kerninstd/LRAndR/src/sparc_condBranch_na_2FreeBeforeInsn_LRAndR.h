// sparc_condBranch_na_2FreeBeforeInsn_LRAndR.h
// handles the branches with the following conditions:
// a) conditional (i.e. not branch-always), and
// b) having the annul bit NOT set (so the delay slot insn is always executed), and
// c) can be either integer, fp, or even bpr, and
// d) has 2 regs free *before* the orig insn (i.e., in the same context that the
//    snippets are executing in).

// rAndR contents:
// snippets; (in context of regs free *before* the orig insn)
// set r2 to taken addr
// set r1 to fall-through addr
// movr <cond,testreg> OR movcc <xcc/fcc> r2 onto r1
// jmp r1
// i+1

// This is an especially nice rAndR, since it actually gets rid of the conditional
// branch!  Unfortunately, the value of <reg> in jmp <reg> isn't trivially predicated
// ahead of time, so modern cpus may miss in their branch-target-buffers, etc.
// Also, the predict bit, if any (BPcc, FBPfcc) is thrown on the floor.

// XXX TODO: Please see condBranch_0FreeAfterInsn_LRAndR.h for an important
// bug: instrumentation snippets shouldn't always go before the relocated branch;
// in particular, what about preReturn instrumentation when the fall-through case
// goes to another function?  For that matter, what if the if-taken case goes
// to another function (perhaps also, perhaps only the if-taken case does).
// (I think the solution in this case is that it simply can't work in the presence
// of any preReturn instrumentation.)
//
// Also, we have he usual delay slot bug: we assume that any delay slot instruction
// that is itself a non-dcti can trivially be relocated, but at least one case,
// "rd %pc, <rdest>" cannot!

#ifndef _COND_BRANCH_NA_2FREE_LRANDR_H_
#define _COND_BRANCH_NA_2FREE_LRANDR_H_

#include "LRAndR.h"
#include "sparc_reg.h"

class condBranch_na_2FreeBeforeInsn_LRAndR : public LRAndR {
 private:
   sparc_reg freeReg1, freeReg2; // both are always integer registers
   
 public:
   condBranch_na_2FreeBeforeInsn_LRAndR(kptr_t iLaunchSiteAddr,
                                        const instr_t *ioriginsn,
                                        const regset_t *iFreeRegsBeforeInsn,
                                        const regset_t *iFreeRegsAfterInsn,
                                        SpringBoardHeap &isbheap,
                                        const sparc_reg &iFreeReg1,
                                        const sparc_reg &iFreeReg2);
      // assumes iFreeRegsBeforeInsn has 2 non-g0 integer registers in it.
  ~condBranch_na_2FreeBeforeInsn_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo *) const;
};

#endif
