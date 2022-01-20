// unwound_callWriteO7_LRAndR.h
// A copy of unwound_callRestore_LRAndR, except that it unwinds
// "call C; mov xxx, %o7"

// Launcher: annulled (as usual)
//
// rAndR contents:
// [StuffBeforePreInsnSnippets:]
// save
// [preinsn snippets here. regs free: regs free *before* orig insn after "save"
// plus %o0-%o5, %o7
// [StuffAfterPreInsnSnippets:]
// mov %i0-%i5 --> %o0-%o5
// call <dest>
// nop
// mov %o0-%o5 --> %i0-%i5
// [prereturn] snippets (regs free: %o0-%o5, %o7 and all %l regs.
// [StuffAfterPreReturnSnippets:]
// ret
// restore
//
// Also, we have the usual delay slot bug: we assume that any delay
// slot instruction that is itself a non-dcti can trivially be
// relocated, but at least one case, "rd %pc, <rdest>" cannot!

#ifndef _UNWOUND_CALLWRITEO7_LRANDR_H_
#define _UNWOUND_CALLWRITEO7_LRANDR_H_

#include "LRAndR.h"
#include "sparc_instr.h"
#include "sparc_reg.h"
#include "sparc_reg_set.h"

class unwound_callWriteO7_LRAndR : public LRAndR {
 private:
   unwound_callWriteO7_LRAndR(const unwound_callWriteO7_LRAndR &);
   unwound_callWriteO7_LRAndR& operator=(const unwound_callWriteO7_LRAndR &);

   kptr_t call_destination;

   sparc_reg_set intRegsForExecutingPreInsnSnippets;
      // must be stored, since getIntRegsForExecutingPreInsnSnippets()
      // expects a reference return value

   sparc_reg_set intRegsForExecutingPreReturnSnippets;
      // must be stored, since
      // getIntRegsForExecutingPreReturnSnippets() expects a reference
      // return value

 public:
   unwound_callWriteO7_LRAndR(kptr_t iLaunchSiteAddr,
                              const instr_t *ioriginsn, 
                              sparc_instr idsinsn,
                              const regset_t *iFreeIntRegsBeforeInsn,
                              const regset_t *iFreeIntRegsAfterInsn,
                              SpringBoardHeap &);

  ~unwound_callWriteO7_LRAndR();

   unsigned conservative_numInsnBytesForRAndR() const;

   // The base class provides defaults for the following methods which
   // don't work for us, so we override the virtual fn calls:
   const regset_t *getInitialIntRegsForExecutingPreInsnSnippets() const;
   const regset_t *getInitialIntRegsForExecutingPreReturnSnippets() const;

   bool preReturnHasMeaning() const { return true; }
//   bool belongsPreInsn(const snippet &s) const;
//   bool belongsPreReturn(const snippet &s) const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr, 
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffBeforePreInsnSnippets(directToMemoryEmitter_t &, 
                                       const function_t &,
                                       const instr_t::controlFlowInfo*) const;
   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &, 
                                      const function_t &,
                                      const instr_t::controlFlowInfo*) const;
   void emitStuffAfterPreReturnSnippets(directToMemoryEmitter_t &, 
                                        const function_t &,
                                        const instr_t::controlFlowInfo*) const;
};

#endif
