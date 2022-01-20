// unwoundCallWriteO7_LRAndR.C
// See the .h file for comments

#include "sparc_unwound_callWriteO7_LRAndR.h"
#include "directToMemoryEmitter.h"

unwound_callWriteO7_LRAndR::
unwound_callWriteO7_LRAndR(kptr_t iLaunchSiteAddr,
                           const instr_t *ioriginsn, sparc_instr idsinsn,
                           const regset_t *iFreeIntRegsBeforeInsn,
                           const regset_t *iFreeIntRegsAfterInsn,
                           SpringBoardHeap &isbheap) :
      LRAndR(iLaunchSiteAddr, ioriginsn,
	     iFreeIntRegsBeforeInsn, iFreeIntRegsAfterInsn, isbheap),
      intRegsForExecutingPreInsnSnippets(sparc_reg_set::allLs), // for now...
      intRegsForExecutingPreReturnSnippets(sparc_reg_set::allLs) // for now...
{
   sparc_instr::sparc_cfi cfi;
   ioriginsn->getControlFlowInfo(&cfi);
   assert(cfi.fields.controlFlowInstruction && cfi.fields.isCall);
   call_destination = iLaunchSiteAddr + cfi.offset_nbytes;

   // We want to catch cases when the write to o7 may have side
   // effects. In that case, we cannot unwind the sequence (currently,
   // we simply discard the delay slot instruction when
   // unwinding). For now we'll assert that the idsinsn is "mov xxx, %o7".
   sparc_reg dont_care_reg = sparc_reg::g0;
   assert(idsinsn.isMove(sparc_reg::o7, dont_care_reg));

   // o0-o5 are free for pre-insn snippets: we've executed a save
   intRegsForExecutingPreInsnSnippets += sparc_reg::o0;
   intRegsForExecutingPreInsnSnippets += sparc_reg::o1;
   intRegsForExecutingPreInsnSnippets += sparc_reg::o2;
   intRegsForExecutingPreInsnSnippets += sparc_reg::o3;
   intRegsForExecutingPreInsnSnippets += sparc_reg::o4;
   intRegsForExecutingPreInsnSnippets += sparc_reg::o5;

   // o0-o5,o7 are free for pre-return snippets: we will do ret; restore
   // at the very end so their contents will vanish anyway
   intRegsForExecutingPreReturnSnippets += sparc_reg::o0;
   intRegsForExecutingPreReturnSnippets += sparc_reg::o1;
   intRegsForExecutingPreReturnSnippets += sparc_reg::o2;
   intRegsForExecutingPreReturnSnippets += sparc_reg::o3;
   intRegsForExecutingPreReturnSnippets += sparc_reg::o4;
   intRegsForExecutingPreReturnSnippets += sparc_reg::o5;
   intRegsForExecutingPreReturnSnippets += sparc_reg::o7;
}

unwound_callWriteO7_LRAndR::~unwound_callWriteO7_LRAndR()
{
}

unsigned unwound_callWriteO7_LRAndR::conservative_numInsnBytesForRAndR() const
{
    // 1 save
    // 6 moves; call; nop;
    // 6 unmoves; ret; restore
    return 4*(1 + 6 + 2 + 6 + 2);
}

const regset_t*
unwound_callWriteO7_LRAndR::getInitialIntRegsForExecutingPreInsnSnippets()
const {
   return &intRegsForExecutingPreInsnSnippets;
}

const regset_t*
unwound_callWriteO7_LRAndR::getInitialIntRegsForExecutingPreReturnSnippets()
const {
   return &intRegsForExecutingPreReturnSnippets;
}

void unwound_callWriteO7_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr, 
                                   const instr_t *whenAllDoneRestoreToThisInsn)
{
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void unwound_callWriteO7_LRAndR::
emitStuffBeforePreInsnSnippets(directToMemoryEmitter_t &em, 
                               const function_t &,
                               const instr_t::controlFlowInfo *) const
{
    const abi &theABI = em.getABI();
    const unsigned stackFrameNumBytes = theABI.getMinFrameAlignedNumBytes();
    instr_t *i = new sparc_instr(sparc_instr::save, -stackFrameNumBytes);
    em.emit(i);
}

void unwound_callWriteO7_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &,
                              const instr_t::controlFlowInfo *) const
{
   // move %i0-%i5 --> %o0-%o5
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::i0, sparc_reg::o0));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::i1, sparc_reg::o1));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::i2, sparc_reg::o2));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::i3, sparc_reg::o3));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::i4, sparc_reg::o4));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::i5, sparc_reg::o5));

   const kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();

   em.emitCall(currInsnAddr, call_destination);
   em.emit(new sparc_instr(sparc_instr::nop));

   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::o0, sparc_reg::i0));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::o1, sparc_reg::i1));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::o2, sparc_reg::i2));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::o3, sparc_reg::i3));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::o4, sparc_reg::i4));
   em.emit(new sparc_instr(sparc_instr::mov, sparc_reg::o5, sparc_reg::i5));
}

void unwound_callWriteO7_LRAndR::
emitStuffAfterPreReturnSnippets(directToMemoryEmitter_t &em,
                                const function_t &,
                                const instr_t::controlFlowInfo *) const
{
   em.emit(new sparc_instr(sparc_instr::ret));
   em.emit(new sparc_instr(sparc_instr::restore)); // always a trivial restore
}
