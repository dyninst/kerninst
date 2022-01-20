// sparc_condBranch_1FreeAfterInsn_LRAndR.C
// See .h file for comments

#include "sparc_condBranch_1FreeAfterInsn_LRAndR.h"
#include "sparc_instr.h"
#include "usualJump.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"

condBranch_1FreeAfterInsn_LRAndR::
condBranch_1FreeAfterInsn_LRAndR(kptr_t iLaunchSiteAddr,
                                 const instr_t *ioriginsn,
                                 const regset_t *iFreeRegsBeforeInsn,
                                 const regset_t *iFreeRegsAfterInsn,
                                 SpringBoardHeap &isbheap
                                 ) :
      LRAndR(iLaunchSiteAddr, ioriginsn, iFreeRegsBeforeInsn, 
	     iFreeRegsAfterInsn, isbheap) {
   assert(!iFreeRegsAfterInsn->isempty()); // this class requires at least 1 in this set
}

unsigned condBranch_1FreeAfterInsn_LRAndR::
conservative_numInsnBytesForRAndR() const {
   // 2 for branch/nop, 3 for open-ds usualJump plus delay insn {if !annul}
   // or 3 for 1-reg-free non-open ds usualJump {if annul},
   // 3 for open-ds usualJump plus delay insn.  Total is 8.
   return 4 * 8;
}

void condBranch_1FreeAfterInsn_LRAndR::
emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                              const function_t &fn,
                              const instr_t::controlFlowInfo *icfi) const {
   const sparc_instr::sparc_cfi *cfi = (const sparc_instr::sparc_cfi*)icfi;
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();

   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   usualJump *notTakenJump = usualJump::allocate_appropriate(currInsnAddr + 8, // from
                                                             launchSiteAddr+8, // to
                                                             *(const sparc_reg_set*)freeIntRegsAfterInsn,
                                                             !cfi->sparc_fields.annul);
      // leave ds open (for the ds insn) iff the annul bit wasn't set, since otherwise,
      // the delay insn shouldn't get executed.  We note that in theory, leaving
      // the delay slot open in a usualJump can fail (throws an exception when there
      // are no avail regs), but since iFreeRegsAfterInsn isn't empty, this won't
      // happen in this class.

   const unsigned notTakenStuffNumBytes = notTakenJump->numInsnBytes();
   const unsigned displacement = 8 + notTakenStuffNumBytes;

   // The original branch insn (i.e., keep the distinction between BPcc/bicc,
   // and fbfcc and fbpfcc, keep predict bits if any, etc.)
   assert(cfi->fields.controlFlowInstruction);

   if (cfi->sparc_fields.isBicc) {
      instr_t *i = new sparc_instr(sparc_instr::bicc, cfi->condition,
				   false, // don't annul
				   displacement);
      em.emit(i);
   }
   else if (cfi->sparc_fields.isBPcc) {
      instr_t *i = new sparc_instr(sparc_instr::bpcc, cfi->condition,
				   false, // don't annul
				   cfi->sparc_fields.predict, cfi->xcc,
				   displacement);
      em.emit(i);
   }
   else if (cfi->sparc_fields.isFBfcc) {
      instr_t *i = new sparc_instr(sparc_instr::fbfcc, cfi->condition,
				   false, // don't annul
				   displacement);
      em.emit(i);
   }
   else if (cfi->sparc_fields.isFBPfcc) {
      instr_t *i = new sparc_instr(sparc_instr::fbpfcc, cfi->condition,
				   false, // don't annul
				   cfi->sparc_fields.predict, cfi->fcc_num,
				   displacement);
      em.emit(i);
   }
   else if (cfi->sparc_fields.isBPr) {
      instr_t *i = new sparc_instr(sparc_instr::bpr, cfi->condition,
				   false, // don't annul
				   cfi->sparc_fields.predict,
				   *(sparc_reg*)(cfi->conditionReg), // compare this reg against regCondition
				   displacement);
      em.emit(i);
   }
   else
      assert(0);

   // nop in delay slot of orig branch:
   instr_t *nop = new sparc_instr(sparc_instr::nop);
   em.emit(nop);

   // fall-through: usual jump to launchSiteAddr+8 [in ctx of regs free after orig
   // insn].  If not annulled, leave ds open and put i+1 there.
   notTakenJump->generate(em);

   if (!cfi->sparc_fields.annul) {
      currInsnAddr = patchAddr + em.numInsnBytesEmitted();
      em.emit(fn.get1OrigInsn(launchSiteAddr+4)->relocate(launchSiteAddr+4,
							  currInsnAddr));
   }
   
   // taken: usual jump to destination of the branch [in ctx of regs free after orig
   // insn].  Leave ds open; place i+1 there. (Do this regardless of the annul bit's
   // setting).
   const kptr_t destination_addr = launchSiteAddr + cfi->offset_nbytes;
   currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   usualJump *jumpToDestination = usualJump::allocate_appropriate(currInsnAddr, // from
                                                                  destination_addr,
                                                                  *(const sparc_reg_set*)freeIntRegsAfterInsn,
                                                                  true // leave ds open
                                                                  );
   jumpToDestination->generate(em);
   currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   em.emit(fn.get1OrigInsn(launchSiteAddr+4)->relocate(launchSiteAddr+4, currInsnAddr));
   
   delete notTakenJump;
   notTakenJump = NULL;

   delete jumpToDestination;
   jumpToDestination = NULL;

   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}

void condBranch_1FreeAfterInsn_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}
