// sparc_unwound_callRestore_LRAndR.h
// Think of this as like call_LRAndR.h, except that we relocate a rather fancy
// version of the call insn and its delay slot.
// Useful, of course, for unwinding call; restore tail-call sequences.
// But let's be more specific: it can unwind "call C; restore rs1, rs2, rd", but
// it cannot unwind "call C; mov xxx, %o7" nor "jmpl C, %o7; restore" nor "jmp C".

// Launcher: annulled (as usual)
//
// rAndR contents:
// [preinsn snippets here.  regs free: regs free *before* orig insn,
// minus %o0-%o5 and rd', then plus %i0-%i5 and all %l's.  Note that %o7
// should also be free]
// [StuffAfterPreInsnSnippets:]
// add rs1, rs2, rd' where rd'={rd if rd was a %g, %i if rd was %o0-o5,
// else error} // the effects of the restore.  Leave out if rd was %g0
// OR: add rs1, <simm13>, rd'
// mov %i0-%i5 --> %o0-%o5
// call <dest>  (if it doesn't fit, assert fail [in future, we'll use jmpl <addr>, %o7])
// nop
// mov %o0-%o5 --> %i0-%i5
// [prereturn] snippets (regs free: %o0-%o5 and all %l regs. Perhaps %o7 too)
// ret
// restore
//
// Also, we have he usual delay slot bug: we assume that any delay slot instruction
// that is itself a non-dcti can trivially be relocated, but at least one case,
// "rd %pc, <rdest>" cannot!

#ifndef _UNWOUND_CALLRESTORE_LRANDR_H_
#define _UNWOUND_CALLRESTORE_LRANDR_H_

#include "LRAndR.h"
#include "sparc_instr.h"
#include "sparc_reg.h"
#include "sparc_reg_set.h"

class unwound_callRestore_LRAndR : public LRAndR {
 private:
   unwound_callRestore_LRAndR(const unwound_callRestore_LRAndR &);
   unwound_callRestore_LRAndR& operator=(const unwound_callRestore_LRAndR &);

   kptr_t call_destination;
   sparc_reg restore_rs1;

   bool usingRs2; // if false, using a constant simm13 instead
   sparc_reg restore_rs2;
   int restore_simm13; // either this or restore_rs2 is defined, not both

   sparc_reg restore_rd;
      // the regs used in restore...may all be %g0 of course
   sparc_reg new_restore_rd; // after appropriate alteration...

   sparc_reg_set intRegsForExecutingPreInsnSnippets;
      // must be stored, since getIntRegsForExecutingPreInsnSnippets() expects a
      // reference return value

   sparc_reg_set intRegsForExecutingPreReturnSnippets;
      // must be stored, since getIntRegsForExecutingPreReturnSnippets() expects a
      // reference return value

 public:
   unwound_callRestore_LRAndR(kptr_t iLaunchSiteAddr,
                              const instr_t *ioriginsn, 
			      sparc_instr irestoreinsn,
                              const regset_t *iFreeIntRegsBeforeInsn,
                              const regset_t *iFreeIntRegsAfterInsn,
                              SpringBoardHeap &);

   ~unwound_callRestore_LRAndR() {}

   unsigned conservative_numInsnBytesForRAndR() const;

   // The base class provides defaults for the following methods which don't work
   // for us, so we override the virtual fn calls:
   const regset_t *getInitialIntRegsForExecutingPreInsnSnippets() const;
   const regset_t *getInitialIntRegsForExecutingPreReturnSnippets() const;

   bool preReturnHasMeaning() const { return true; }
//   bool belongsPreInsn(const snippet &s) const;
//   bool belongsPreReturn(const snippet &s) const;

   void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                           const instr_t *whenAllDoneRestoreToThisInsn);

   void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                      const function_t &,
                                      const instr_t::controlFlowInfo*) const;
   void emitStuffAfterPreReturnSnippets(directToMemoryEmitter_t &,
                                        const function_t &,
                                        const instr_t::controlFlowInfo*) const;
};

#endif
