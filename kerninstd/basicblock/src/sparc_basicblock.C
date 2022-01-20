// sparc_basicblock.C - implementation of arch-specific basicblock members
//                      for SPARC

#include "sparc_basicblock.h"

kptr_t sparc_basicblock::getExitPoint(const fnCode_t &fnCode) const {
   // when one asks to instrument the end of a basic block, what instruction should
   // it be?  It's not trivial.  Some basic blocks end in a certain 2-instruction
   // "return" sequence (e.g. ret/restore or retl/nop); in those cases we should
   // instrument just before those two instructions.  At the other extreme,
   // some basic blocks are, due to splitting, just a single non-pc-changing
   // instruction.  In those cases, we should instrument just after that instruction
   // (and since postInstruction instrumentation is not yet implemented, it'll have
   // to be before that instruction for now)

   // XXX TODO -- this routine is way too sparc-specific.  Partial specialize to
   // get it to work on x86.

   // XXX TODO -- the implementation here is pretty incomplete; compare with
   // function.C's getCurrExitPoints().  In fact, most of that functionality
   // should probably moved here!

   const kptr_t bbStartAddr = getStartAddr();
   const kptr_t bbNumInsnBytes = getNumInsnBytes();
   
   assert(bbNumInsnBytes >= 4);
   
   if (bbNumInsnBytes == 4) {
      // Just a single instruction.  If it's not a DCTI, then we'd like to
      // instrument "postInstruction" of this instruction (and we must settle for
      // "preInsn" of this instruction for now).  If it is a DCTI, then we pretty
      // much have to settle for "preInsn" of this instruction in all cases.

      return bbStartAddr;
   }

   // At least two instructions
   assert(bbNumInsnBytes >= 8);

   const kptr_t secondToLastInsnAddr = bbStartAddr + bbNumInsnBytes - 8;
   const kptr_t lastInsnAddr = bbStartAddr + bbNumInsnBytes - 4;

   sparc_instr *secondToLastInsn = (sparc_instr*)fnCode.get1Insn(secondToLastInsnAddr);
   
   sparc_instr::sparc_cfi cfi;
   secondToLastInsn->getControlFlowInfo(&cfi);
   
   if (cfi.fields.controlFlowInstruction) {
      // The basic block ends in a control flow instruction/delay slot
      // situation.
      assert(cfi.delaySlot != instr_t::dsNone);

      // Instrumentation should be placed before the control flow instruction.
      // (Perhaps we can improve on this in the future)

      return secondToLastInsnAddr;
   }
   else {
      // The second to last instruction is not a CTI; it is "normal".
      // So we certainly don't need to instrument before it.
      // The only option left is to instrument before the final instruction.
      // (In the future, there will be one other option: instrumenting "postInstruc"
      // of the last instruction).
      return lastInsnAddr;
   }
}
