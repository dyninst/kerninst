// x86_call_LRAndR.C
// See the .h file for comments

#include "x86_call_LRAndR.h"
#include "x86_instr.h"
#include "x86_reg_set.h"
#include "directToMemoryEmitter.h"
#include "funkshun.h"
#include "usualJump.h"

call_LRAndR::call_LRAndR(kptr_t iLaunchSiteAddr,
                         const instr_t *ioriginsn,
                         const regset_t *iFreeIntRegsBeforeInsn,
                         const regset_t *iFreeIntRegsAfterInsn,
                         SpringBoardHeap &isbheap) : LRAndR(iLaunchSiteAddr, 
							    ioriginsn, 
							    iFreeIntRegsBeforeInsn, 
							    iFreeIntRegsAfterInsn,
							    isbheap)
{
   x86_instr::x86_cfi cfi;
   ioriginsn->getControlFlowInfo(&cfi);
   assert(cfi.fields.controlFlowInstruction && cfi.fields.isCall);
   call_destination = (iLaunchSiteAddr + 5) + cfi.offset_nbytes;
}

unsigned call_LRAndR::conservative_numInsnBytesForRAndR() const {
   return 5; 
      // 5 for jmp to callee
}

void call_LRAndR::
registerPatchAddrAndCreateLauncher(kptr_t ipatchAddr,
                                   const instr_t *whenAllDoneRestoreToThisInsn) {
   setPatchAddr(ipatchAddr);
   setLauncherToAnyAnnulledLauncher(patchAddr, whenAllDoneRestoreToThisInsn);
}

void call_LRAndR::emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &em,
                                                const function_t &fn,
                                                const instr_t::controlFlowInfo*
                                                ) const {
   const unsigned initialNumInsnBytesEmitted = em.numInsnBytesEmitted();
   kptr_t currInsnAddr = patchAddr + em.numInsnBytesEmitted();
   cout << "MJB DEBUG: x86 call_LRAndR::emitStuffAfterPreInsnSnippets() - currInsnAddr is 0x" << std::hex << currInsnAddr << std::dec;
   cout << ", call_destination is 0x" << std::hex << call_destination << std::dec;

   //We don't really need the overhead of usualJump on x86, since our
   //task is simple: generate either a 16-bit relative or 32-bit relative
   //jump.
   //The question is if it's even worth the trouble to save one byte...
   if ( instr_t::inRangeOf(currInsnAddr, //from
                           call_destination, //to
                           16 //bits of signed displacement
                           ) ) {
      int16_t relativeOffset = call_destination - (currInsnAddr+5);
      instr_t *jumpToCallee = new x86_instr(x86_instr::jmp, relativeOffset);
      em.emit(jumpToCallee);
   } else { //must use 32-bit offset
      int32_t relativeOffset = call_destination - (currInsnAddr+5);
      cout << ", relativeOffset is 0x" << std::hex << relativeOffset << std::dec << endl;
      instr_t *jumpToCallee = new x86_instr(x86_instr::jmp, relativeOffset);
      em.emit(jumpToCallee);
   }
    
   assert(em.numInsnBytesEmitted() - initialNumInsnBytesEmitted <= conservative_numInsnBytesForRAndR());
}
