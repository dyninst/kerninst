// sparc_jmpl_LRAndR.C
// see .h file for comments

#include "sparc_jmpl_LRAndR.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"

unsigned jmpl_LRAndR::conservative_numInsnBytesForRAndR() const {
   // 2 to mov destReg1 and destReg2 to free registers if necessary
   // 2 to set link reg (unless it's g0, in which case it's none)
   // plus 1 for jmp plus 1 for relocated delay slot insn
   return 6*4;
}

void jmpl_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void jmpl_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &fn,
                              const instr_t::controlFlowInfo *cfi) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   sparc_reg destReg1(*(sparc_reg*)(cfi->destreg1));
   sparc_reg destReg2(sparc_reg::g0);
   if (cfi->destination == instr_t::controlFlowInfo::doubleReg) {
       destReg2 = *(sparc_reg*)(cfi->destreg2);
   }

   // if needed, write to link reg
   sparc_instr::sparc_ru ru;
   originsn->getRegisterUsage(&ru);
      // ru will contain (1) the pc and (2) the link register, unless it's g0.
   *(sparc_reg_set*)(ru.definitelyWritten) -= sparc_reg::reg_pc;
      
   // now ru will contain the link register (empty if it was g0, as in a return)
   regset_t::const_iterator iter = ru.definitelyWritten->begin();

   if (iter == ru.definitelyWritten->end())
      // link register was %g0.  Don't need to write it.  "ret" and "retl" come to mind.
      ;
   else {
      // We need to put the address of the original callsite into linkReg
      sparc_reg &linkReg = (sparc_reg&)*iter;
      ++iter;
      assert(iter == ru.definitelyWritten->end());
      assert(linkReg != sparc_reg::g0);

      // But first, let's make sure that linkReg is not used as destReg1 or 2.
      // Cases like "jmpl %o7, %o7" are not that uncommon
      regset_t::const_iterator free_iter =
	  ((const sparc_reg_set*)freeIntRegsBeforeInsn)->begin_intregs_afterG0();
      if (linkReg == destReg1) {
	  assert(free_iter != ((const sparc_reg_set*)freeIntRegsBeforeInsn)->end_intregs());
	  const sparc_reg &newDest = (sparc_reg&)*free_iter;

	  instr_t *i = new sparc_instr(sparc_instr::mov, destReg1, newDest);
	  em.emit(i);
	  destReg1 = newDest;
	  free_iter++;
      }
      if (linkReg == destReg2) {
	  assert(free_iter != ((const sparc_reg_set*)freeIntRegsBeforeInsn)->end_intregs());
	  const sparc_reg &newDest = (sparc_reg&)*free_iter;

	  instr_t *i = new sparc_instr(sparc_instr::mov, destReg2, newDest);
	  em.emit(i);
	  destReg2 = newDest;
	  free_iter++;
      }
      em.emitImmAddr(launchSiteAddr, linkReg);
   }

   // Now, jmp <original_address> (no link register)
   if (cfi->destination == instr_t::controlFlowInfo::regRelative) {
      // reg + offset
      instr_t *i = new sparc_instr(sparc_instr::jump,
				   *(sparc_reg*)(cfi->destreg1),
				   cfi->offset_nbytes);
      em.emit(i);
   }
   else if (cfi->destination == instr_t::controlFlowInfo::doubleReg) {
      // reg + reg
      instr_t *i = new sparc_instr(sparc_instr::jump,
				   *(sparc_reg*)(cfi->destreg1), 
				   *(sparc_reg*)(cfi->destreg2));
      em.emit(i);
   }
   else
      assert(0);

   // Finally, the relocated delay slot insn:
   const kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   instr_t *relocated_delay =
         fn.get1OrigInsn(launchSiteAddr+4)->relocate(launchSiteAddr + 4,
						     currInsnAddr);
   em.emit(relocated_delay);

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}
