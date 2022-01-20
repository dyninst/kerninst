// sparc_unwoundCallRestore_LRAndR.C
// See the .h file for comments

#include "sparc_unwound_callRestore_LRAndR.h"
#include "directToMemoryEmitter.h"

unwound_callRestore_LRAndR::
unwound_callRestore_LRAndR(kptr_t iLaunchSiteAddr,
                           const instr_t *ioriginsn, sparc_instr irestoreinsn,
                           const regset_t *iFreeIntRegsBeforeInsn,
                           const regset_t *iFreeIntRegsAfterInsn,
                           SpringBoardHeap &isbheap) :
      LRAndR(iLaunchSiteAddr, ioriginsn, iFreeIntRegsBeforeInsn, 
	     iFreeIntRegsAfterInsn, isbheap),
      restore_rs1(sparc_reg::g0),
      usingRs2(true),
      restore_rs2(sparc_reg::g0),
      restore_rd(sparc_reg::g0),
      new_restore_rd(sparc_reg::g0),
      intRegsForExecutingPreInsnSnippets(*(const sparc_reg_set*)iFreeIntRegsBeforeInsn), // for now...
      intRegsForExecutingPreReturnSnippets(sparc_reg_set::allLs) // for now...
{
   sparc_instr::sparc_cfi cfi;
   ioriginsn->getControlFlowInfo(&cfi);
   assert(cfi.fields.controlFlowInstruction && cfi.fields.isCall);
   call_destination = iLaunchSiteAddr + cfi.offset_nbytes;

   if (irestoreinsn.isRestoreUsing2RegInstr(restore_rs1, restore_rs2, restore_rd))
      usingRs2 = true;
   else if (irestoreinsn.isRestoreUsingSimm13Instr(restore_rs1,
                                                   restore_simm13, restore_rd))
      usingRs2 = false;
   else
      assert(false);
   
   if (restore_rd != sparc_reg::g0) {
      unsigned num;

      if (restore_rd.is_gReg(num))
         new_restore_rd = restore_rd;
      else if (restore_rd.is_oReg(num)) // most likely
         if (num <= 5)
            new_restore_rd = sparc_reg(sparc_reg::in, num);
         else
            assert(false);
      else if (restore_rd.is_lReg(num))
         assert(false);
      else if (restore_rd.is_iReg(num))
         assert(false);
      else
         assert(false);
   }

   // o0-o5 are free for pre-insn snippets: we will move i's there later anyway
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
   //intRegsForExecutingPreReturnSnippets += sparc_reg_set::allLs;
   // above line commented out because it was already done in the ctor initializer,
   // above (!)
}

unsigned unwound_callRestore_LRAndR::conservative_numInsnBytesForRAndR() const {
   // 6 moves; 1 add; 
   // genImmAddr(hi) -> o7 -- up to 3 insns;
   // jmpl o7+lo, o7; nop; 6 unmoves; ret/restore
   return 4*(6 + 1 + 3 + 1 + 1 + 6 + 2);
}

const regset_t*
unwound_callRestore_LRAndR::getInitialIntRegsForExecutingPreInsnSnippets() const {
   return &intRegsForExecutingPreInsnSnippets;
}

const regset_t*
unwound_callRestore_LRAndR::getInitialIntRegsForExecutingPreReturnSnippets() const {
   return &intRegsForExecutingPreReturnSnippets;
}

//  bool unwound_callRestore_LRAndR::belongsPreInsn(const snippet &s) const {
//     return (s.getPlacement() == snippet::preinsn);
//  }
//  bool unwound_callRestore_LRAndR::belongsPreReturn(const snippet &s) const {
//     return (s.getPlacement() == snippet::prereturn);
//  }

void unwound_callRestore_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void unwound_callRestore_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &,
                              const instr_t::controlFlowInfo *) const {

   // add restore_rs1, restore_rs2, rd' where
   // rd' = {restore_rd if restore_rd=%g0-%o7
   //        %ix if restore_rd=%ox for x=0 to 5
   //        error! otherwise (throw an exception?)
   //       }
   // (leave out if restore_rd was %g0)

   if (restore_rd != sparc_reg::g0) {
      if (usingRs2) {
	 instr_t *i = new sparc_instr(sparc_instr::add,
				      restore_rs1, restore_rs2, 
				      new_restore_rd);
         em.emit(i);
      }
      else {
	 instr_t *i = new sparc_instr(sparc_instr::add,
				      restore_rs1, restore_simm13, 
				      new_restore_rd);
         em.emit(i);
      }
   }

   // move %i0-%i5 --> %o0-%o5
   instr_t *mov = new sparc_instr(sparc_instr::mov, sparc_reg::i0, 
				  sparc_reg::o0);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::i1, sparc_reg::o1);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::i2, sparc_reg::o2);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::i3, sparc_reg::o3);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::i4, sparc_reg::o4);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::i5, sparc_reg::o5);
   em.emit(mov);

   const kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();

   em.emitCall(currInsnAddr, call_destination);
   instr_t *nop = new sparc_instr(sparc_instr::nop);
   em.emit(nop);

   mov = new sparc_instr(sparc_instr::mov, sparc_reg::o0, sparc_reg::i0);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::o1, sparc_reg::i1);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::o2, sparc_reg::i2);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::o3, sparc_reg::i3);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::o4, sparc_reg::i4);
   em.emit(mov);
   mov = new sparc_instr(sparc_instr::mov, sparc_reg::o5, sparc_reg::i5);
   em.emit(mov);
}

void unwound_callRestore_LRAndR::
emitStuffAfterPreReturnSnippets(directToMemoryEmitter_t &em,
                                const function_t &,
                                const instr_t::controlFlowInfo *) const {
   instr_t *i = new sparc_instr(sparc_instr::ret);
   em.emit(i);
   i = new sparc_instr(sparc_instr::restore);
   em.emit(i); // always a trivial restore
}
