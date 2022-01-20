// sparc_LRAndR.C
// See .h file for comments

#include "LRAndR.h"
#include "funkshun.h"
#include "util/h/rope-utils.h"
#include "sparc_instr.h"
#include "sparc_reg_set.h"
#include "sparc_nonDCTI_LRAndR.h"
#include "sparc_jmpl_LRAndR.h"
#include "sparc_call_LRAndR.h"
#include "sparc_unwound_callRestore_LRAndR.h"
#include "sparc_unwound_callWriteO7_LRAndR.h"
#include "sparc_condBranch_na_2FreeBeforeInsn_LRAndR.h"
#include "sparc_condBranch_0FreeAfterInsn_LRAndR.h"
#include "sparc_condBranch_1FreeAfterInsn_LRAndR.h"
#include "sparc_branchAlways_LRAndR.h"
#include "sparc_branchNever_LRAndR.h"
#include "sparc_doneOrRetry_LRAndR.h"
#include "sparc_v9Return_LRAndR.h"

LRAndR* LRAndR::create_LRAndR(kptr_t addr, const function_t &fn,
			      const instr_t *originsn,
			      const instr_t::controlFlowInfo *icfi,
			      const regset_t *intRegsAvailBeforeInsn,
			      const regset_t *intRegsAvailAfterInsn,
			      bool anyPreReturnSnippets,
			      bool verbose) {
   // This static method does one thing only: pick the appropriate LRAndR.

   // Assert that intRegsAvailBeforeInsn and intRegsAvailAfterInsn are indeed
   // only sets of integer registers.  This is important, because it allows us to
   // check for at least one free int register with simply a call to !isempty
   assert(*(const sparc_reg_set*)intRegsAvailBeforeInsn == (*(const sparc_reg_set*)intRegsAvailBeforeInsn & sparc_reg_set::allIntRegs));
   assert(*(const sparc_reg_set*)intRegsAvailAfterInsn == (*(const sparc_reg_set*)intRegsAvailAfterInsn & sparc_reg_set::allIntRegs));

   // sorry for this hack:
   extern SpringBoardHeap *global_springboardHeap;
   SpringBoardHeap &theSpringBoardHeap = *global_springboardHeap;

   const sparc_instr::sparc_cfi *cfi = (const sparc_instr::sparc_cfi *)icfi;

   if (!cfi->fields.controlFlowInstruction)
      return new nonDCTI_LRAndR(addr, originsn,
                                intRegsAvailBeforeInsn,
                                intRegsAvailAfterInsn,
                                theSpringBoardHeap);

   if (cfi->fields.isCall || cfi->fields.isJmpLink) {
      // call or jmpl.  Perhaps a tail call/jump; perhaps not.  Set some flags.
      bool trueCall = cfi->fields.isCall && !cfi->fields.isJmpLink;
      bool followedByRestore = false;
      bool followedByWriteToO7 = false;

      const kptr_t nextInsnAddr = addr + 4;
      assert(fn.containsAddr(nextInsnAddr) && "call/jmpl should always have a delay slot insn that falls w/in the bounds of the fn");
      
      sparc_instr *nextInsn = (sparc_instr*)fn.get1OrigInsn(nextInsnAddr);
      sparc_instr::sparc_ru ru;
      nextInsn->getRegisterUsage(&ru);
      if (ru.sr.isRestore)
         followedByRestore = true;
      else if (ru.definitelyWritten->exists(sparc_reg::o7) ||
               ru.maybeWritten->exists(sparc_reg::o7)) {
         // it's a call (true call) followed by something that writes to o7.
         // perhaps call; mov %g1, %o7
         followedByWriteToO7 = true;
      }

      if (trueCall) {
         if (followedByRestore && anyPreReturnSnippets)
            return new unwound_callRestore_LRAndR(addr,
                                                  originsn, *nextInsn,
                                                  intRegsAvailBeforeInsn,
                                                  intRegsAvailAfterInsn,
                                                  theSpringBoardHeap);

         if (followedByWriteToO7 && anyPreReturnSnippets)
            return new unwound_callWriteO7_LRAndR(addr,
                                                  originsn, *nextInsn,
                                                  intRegsAvailBeforeInsn,
                                                  intRegsAvailAfterInsn,
                                                  theSpringBoardHeap);

         return new call_LRAndR(addr, originsn,
                                intRegsAvailBeforeInsn,
                                intRegsAvailAfterInsn,
                                theSpringBoardHeap);
      }
      else {
         // jmpl.  We're not going to unwind, though perhaps some day we should
         // implement unwinding where appropriate, because this is an increasingly
         // common tail call sequence!
         // Anyway, in the usual cases of ret and retl, that's the desired action.

         assert(cfi->fields.isJmpLink);
         if (!cfi->fields.isRet && !cfi->sparc_fields.isRetl &&
             (followedByRestore || followedByWriteToO7) &&
             anyPreReturnSnippets) {
            if (verbose) {
               cout << "At launchAddr=" << addr2hex(addr) << endl;
               cout << "Found jmpl (*perhaps* an interprocedural tail call)" << endl;
               cout << "followed by a restore or write to o7, but I" << endl;
               cout << "do not yet know how to unwind this sequence," << endl;
               cout << "so all instrumentation will be pre-jmp; sorry" << endl;
            }
            // fall through
         }

         return new jmpl_LRAndR(addr, originsn,
                                intRegsAvailBeforeInsn,
                                intRegsAvailAfterInsn,
                                theSpringBoardHeap);
      }
   }
      
   const bool intCondBranch = (cfi->sparc_fields.isBicc || cfi->sparc_fields.isBPcc);
   const bool floatCondBranch = (cfi->sparc_fields.isFBfcc || cfi->sparc_fields.isFBPfcc);
   const bool regValueBranch = cfi->sparc_fields.isBPr;
   
   if (intCondBranch || floatCondBranch || regValueBranch) {
      bool always = regValueBranch ? false :
                       intCondBranch ? cfi->condition == sparc_instr::iccAlways :
                                       cfi->condition == sparc_instr::fccAlways;
      bool never = regValueBranch ? false :
                       intCondBranch ? cfi->condition == sparc_instr::iccNever :
                                       cfi->condition == sparc_instr::fccNever;

      bool really_conditional = !always && !never;

      if (really_conditional && !cfi->sparc_fields.annul) {
         // A conditional branch (not always or never), and annul bit isn't set.
         // So maybe we can use the optimized condBranch_na_2FreeBeforeInsn_RAndR.
         sparc_reg_set::const_iterator iter = ((const sparc_reg_set*)intRegsAvailBeforeInsn)->begin_intregs_afterG0();
         sparc_reg_set::const_iterator finish = ((const sparc_reg_set*)intRegsAvailBeforeInsn)->end_intregs();

         if (iter != finish) {
            sparc_reg &freeReg1 = (sparc_reg&)*iter++;

            if (iter != finish) {
               // There are 2 int regs free before the insn.  Can therefore use the
               // nicely optimized condBranch_na_2FreeRAndR.
               sparc_reg &freeReg2 = (sparc_reg&)*iter;

               return new condBranch_na_2FreeBeforeInsn_LRAndR(addr, originsn,
                                                               intRegsAvailBeforeInsn,
                                                               intRegsAvailAfterInsn,
                                                               theSpringBoardHeap,
                                                               freeReg1, freeReg2);
            }
         }
      }

      if (really_conditional && !intRegsAvailAfterInsn->isempty()) {
         // A truly conditional branch (whether annulled or not; whether int or fp)
         // that has at least 1 reg free after the instruction.  This makes it
         // possible to emit "usual jumps" with the delay slot left open, so we make
         // this special case its own LRAndR
         return new condBranch_1FreeAfterInsn_LRAndR(addr, originsn,
                                                     intRegsAvailBeforeInsn,
                                                     intRegsAvailAfterInsn,
                                                     theSpringBoardHeap);
      }
      else if (really_conditional) {
         // A truly conditional branch (whether annulled or not; int or fp)
         // that has zero regs free after the insn.  This makes it impossible to
         // emit two "usual jump"s with the delay slot left open, so we make this
         // special case its own LRAndR.
         return new condBranch_0FreeAfterInsn_LRAndR(addr, originsn,
                                                     intRegsAvailBeforeInsn,
                                                     intRegsAvailAfterInsn,
                                                     theSpringBoardHeap);
      }
      else {
         // branch-always or branch-never (could be int or fp branch).
         // Annul bit could be set, or not.

         if (always)
            return new branchAlways_LRAndR(addr, originsn,
                                           intRegsAvailBeforeInsn,
                                           intRegsAvailAfterInsn,
                                           theSpringBoardHeap);
         else if (never)
            return new branchNever_LRAndR(addr, originsn,
                                          intRegsAvailBeforeInsn,
                                          intRegsAvailAfterInsn,
                                          theSpringBoardHeap);
         else
            assert(0);
      }
   }

   if (cfi->sparc_fields.isV9Return) {
      // v9 return acts like a ret; restore super-instruction.  Plus, it has
      // a delay slot where you can do other things.
      return new v9Return_LRAndR(addr, originsn,
                                 intRegsAvailBeforeInsn,
                                 intRegsAvailAfterInsn,
                                 theSpringBoardHeap);
   }
   
   if (cfi->sparc_fields.isDoneOrRetry) {
      // done or retry: trivial to handle; treat like a non-DCTI but
      // even simpler: no need to jump back to instrumentation point+4.
      return new doneOrRetry_LRAndR(addr, originsn,
                                    intRegsAvailBeforeInsn,
                                    intRegsAvailAfterInsn, // will be unused I think
                                    theSpringBoardHeap);
   }
   
   assert(0 && "sparc_LRAndR.C: LRAndR choice not yet implemented!");
   abort(); // placate compiler when compiling with NDEBUG
}
