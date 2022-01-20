// sparc_jmpl_LRAndR.h

// Launcher strategy: annulled

// rAndR strategy:
// snippets; (in context of regs free *before* the *orig* insn)
// write to link reg, if not g0 (up to 6 insns for 64-bit register...)
//    (still in context of regs free *before* the *orig* insn)
// jmp <rs1>
// relocated delay slot insn
//
// Also, we have he usual delay slot bug: we assume that any delay slot instruction
// that is itself a non-dcti can trivially be relocated, but at least one case,
// "rd %pc, <rdest>" cannot!

#ifndef _JMPL_LRANDR_H_
#define _JMPL_LRANDR_H_

#include "LRAndR.h"

class jmpl_LRAndR : public LRAndR {
 private:
   jmpl_LRAndR(const jmpl_LRAndR &);
   jmpl_LRAndR& operator=(const jmpl_LRAndR &);

 public:
   jmpl_LRAndR(kptr_t iLaunchSiteAddr,
               const instr_t *ioriginsn,
               const regset_t *iFreeIntRegsBeforeInsn,
               const regset_t *iFreeIntRegsAfterInsn,
               SpringBoardHeap &isbheap) : LRAndR(iLaunchSiteAddr, ioriginsn, 
						  iFreeIntRegsBeforeInsn, 
						  iFreeIntRegsAfterInsn,
						  isbheap) {}
   ~jmpl_LRAndR() {}
   
   unsigned conservative_numInsnBytesForRAndR() const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo*) const;
};

#endif
