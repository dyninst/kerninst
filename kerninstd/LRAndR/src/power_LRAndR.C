// power_LRAndR.C
// See .h file for comments

#include "LRAndR.h"
#include "funkshun.h"
#include "util/h/rope-utils.h"
#include "power_instr.h"
#include "power_reg_set.h"
#include "power_nonDCTI_LRAndR.h"
#include "power_condBranch_LRAndR.h"
#include "power_branchAlways_LRAndR.h"
#include "power_regBranch_LRAndR.h"

LRAndR* LRAndR::create_LRAndR(kptr_t addr, const function_t & /* fn */,
			      const instr_t *originsn,
			      const instr_t::controlFlowInfo *icfi,
			      const regset_t *intRegsAvailBeforeInsn,
			      const regset_t *intRegsAvailAfterInsn,
			      bool /* anyPreReturnSnippets */,
			      bool /* verbose */) {
   // This static method does one thing only: pick the appropriate LRAndR.

   // sorry for this hack:
   extern SpringBoardHeap *global_springboardHeap;
   SpringBoardHeap &theSpringBoardHeap = *global_springboardHeap;

   const power_instr::power_cfi *cfi = (const power_instr::power_cfi *)icfi;

   if (!cfi->fields.controlFlowInstruction)
     return new nonDCTI_LRAndR(addr, 
			       intRegsAvailBeforeInsn,
			       intRegsAvailAfterInsn,
                               originsn,
			       theSpringBoardHeap);
   
   if (cfi->destination == instr_t::controlFlowInfo::regRelative)
      return new regBranch_LRAndR(addr,   
			       intRegsAvailBeforeInsn,
			       intRegsAvailAfterInsn,
                               originsn,
			       theSpringBoardHeap);

   const bool condBranch =  cfi->fields.isConditional;
   
   if (condBranch) {
     // A conditional branch
     return new condBranch_LRAndR(addr, 
				  intRegsAvailBeforeInsn,
				  intRegsAvailAfterInsn,
                                  originsn,
				  theSpringBoardHeap);
   }     else {
         // branch-always
       return new branchAlways_LRAndR(addr, 
				    intRegsAvailBeforeInsn,
				    intRegsAvailAfterInsn,
                                    originsn,
				    theSpringBoardHeap);
     
   }
}

