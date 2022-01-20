// x86_LRAndR.C
// See .h file for comments

#include "LRAndR.h"
#include "funkshun.h"
#include "util/h/rope-utils.h"
#include "x86_instr.h"
#include "x86_reg_set.h"
#include "x86_branch_LRAndR.h"

LRAndR* LRAndR::create_LRAndR(kptr_t addr, const function_t &fn,
			      const instr_t *originsn,
			      const instr_t::controlFlowInfo *icfi,
			      const regset_t *intRegsAvailBeforeInsn,
			      const regset_t *intRegsAvailAfterInsn,
			      bool anyPreReturnSnippets,
			      bool verbose) {
   // This static method does one thing only: pick the appropriate LRAndR.

   // Assert that intRegsAvailBeforeInsn and intRegsAvailAfterInsn are indeed
   // only sets of integer registers.  This is important, because it allows 
   // us to check for at least one free int register with simply a call 
   // to !isempty
   assert(*(const x86_reg_set*)intRegsAvailBeforeInsn == (*(const x86_reg_set*)intRegsAvailBeforeInsn & x86_reg_set::allIntRegs));
   assert(*(const x86_reg_set*)intRegsAvailAfterInsn == (*(const x86_reg_set*)intRegsAvailAfterInsn & x86_reg_set::allIntRegs));

   // sorry for this hack:
   extern SpringBoardHeap *global_springboardHeap;
   SpringBoardHeap &theSpringBoardHeap = *global_springboardHeap;

   return new branch_LRAndR(addr, originsn,
                            intRegsAvailBeforeInsn,
                            intRegsAvailAfterInsn,
                            theSpringBoardHeap);
}
