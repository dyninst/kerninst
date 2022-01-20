// sparc_condBranch_0FreeAfterInsn_LRAndR.C
// See .h file for comments

#include "sparc_condBranch_0FreeAfterInsn_LRAndR.h"
#include "sparc_instr.h"
#include "sparc_reg_set.h"
#include "usualJump.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"

condBranch_0FreeAfterInsn_LRAndR::
condBranch_0FreeAfterInsn_LRAndR(kptr_t iLaunchSiteAddr,
                                 const instr_t *ioriginsn,
                                 const regset_t *iFreeRegsBeforeInsn,
                                 const regset_t *iFreeRegsAfterInsn,
                                 SpringBoardHeap &isbheap
                                 ) :
      LRAndR(iLaunchSiteAddr, ioriginsn, iFreeRegsBeforeInsn, 
	     iFreeRegsAfterInsn, isbheap) {
   assert(iFreeRegsAfterInsn->isempty());
      // if not empty, you should be using another kind of LRAndR, which emits 
      // better code!  You only use this lowly class when there aren't any free
      // regs after the insn.
}

unsigned
condBranch_0FreeAfterInsn_LRAndR::conservative_numInsnBytesForRAndR() const {
   return 4*(2 + 1 + 1) + usualJump::maxNumInsnBytesNoFreeRegsNoOpenDS() +
      usualJump::maxNumInsnBytesNoFreeRegsNoOpenDS();
      // branch/nop, delay insn (if !annul), usual jump, delay insn, usual jump
}

void condBranch_0FreeAfterInsn_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &fn,
                              const instr_t::controlFlowInfo *icfi) const {
   const sparc_instr::sparc_cfi *cfi = (const sparc_instr::sparc_cfi*)icfi;
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();

   const kptr_t notTakenJump_from = currInsnAddr + 8 + (!cfi->sparc_fields.annul ? 4 : 0);
   const kptr_t notTakenJump_to = launchSiteAddr + 8;
   
   usualJump *notTakenJump = usualJump::allocate_appropriate(notTakenJump_from,
                                                             notTakenJump_to,
                                                             sparc_reg_set::emptyset,
                                                             false);
      // never leave the delay slot open

   const unsigned notTakenStuffNumBytes = (!cfi->sparc_fields.annul ? 4 : 0) + notTakenJump->numInsnBytes();
   const unsigned branchDisplacement = 8 + notTakenStuffNumBytes;

   // The original branch insn (i.e., keep the distinction between BPcc/bicc,
   // and fbfcc and fbpfcc, keep predict bits if any, etc.  But *CLEAR* the
   // annul bit.)
   assert(cfi->fields.controlFlowInstruction);

   // can't use sparc_instr's changeBranchOffsetAndAnnulBit() since we did not
   // store the original instruction here.
   if (cfi->sparc_fields.isBicc) {
      instr_t *i = new sparc_instr(sparc_instr::bicc, cfi->condition,
				   false, // don't annul
				   branchDisplacement);
      em.emit(i);
   }
   else if (cfi->sparc_fields.isBPcc) {
      instr_t *i = new sparc_instr(sparc_instr::bpcc, cfi->condition,
				   false, // don't annul
				   cfi->sparc_fields.predict, cfi->xcc, 
				   branchDisplacement);
      em.emit(i);
   }
   else if (cfi->sparc_fields.isFBfcc) {
      instr_t *i = new sparc_instr(sparc_instr::fbfcc, cfi->condition,
				   false, // don't annul
				   branchDisplacement);
      em.emit(i);
   }
   else if (cfi->sparc_fields.isFBPfcc) {
      instr_t *i = new sparc_instr(sparc_instr::fbpfcc, cfi->condition,
				   false, // don't annul
				   cfi->sparc_fields.predict, cfi->fcc_num,
				   branchDisplacement);
      em.emit(i);
   }
   else if (cfi->sparc_fields.isBPr) {
      instr_t *i = new sparc_instr(sparc_instr::bpr, cfi->condition,
				   false, // don't annul
				   cfi->sparc_fields.predict,
				   *(sparc_reg*)(cfi->conditionReg), // compare this reg
				   branchDisplacement);
      em.emit(i);
   }
   else
      assert(0);

   // nop in delay slot of orig branch:
   instr_t *nop = new sparc_instr(sparc_instr::nop);
   em.emit(nop);

   // Fall-through: if the orig branch wasn't annulled then emit delay insn.
   // Then, the usual jump that we've already allocated.
   if (!cfi->sparc_fields.annul) {
      currInsnAddr = patchAddr + em.numInsnBytesEmitted();
      em.emit(fn.get1OrigInsn(launchSiteAddr+4)->relocate(launchSiteAddr+4,
							  currInsnAddr));
   }

   currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   assert(currInsnAddr == notTakenJump_from);
   
   // usual jump to launchSiteAddr+8, using zero free registers.
   notTakenJump->generate(em);

   // taken: delay slot instruction and then usual jump (using zero free regs)
   // to branch's destination.
   currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   em.emit(fn.get1OrigInsn(launchSiteAddr+4)->relocate(launchSiteAddr+4,
						       currInsnAddr));

   currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   const kptr_t destination_addr = launchSiteAddr + cfi->offset_nbytes;
   usualJump *jumpToDestination =
      usualJump::allocate_appropriate(currInsnAddr, // from
                                      destination_addr,
                                      sparc_reg_set::emptyset,
                                      false // don't leave ds open
                                      );
   jumpToDestination->generate(em);
   
   delete notTakenJump;
   notTakenJump = NULL;

   delete jumpToDestination;
   jumpToDestination = NULL;

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}

void condBranch_0FreeAfterInsn_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}
