// x86_basicblock.C - implementation of arch-specific basicblock members
//                    for x86

#include "x86_basicblock.h"

kptr_t x86_basicblock::getExitPoint(const fnCode_t &fnCode) const {
   // when one asks to instrument the end of a basic block, what instruction should
   // it be?  It's not trivial.  Some basic blocks end in a certain 2-instruction
   // "return" sequence (e.g. ret/restore or retl/nop); in those cases we should
   // instrument just before those two instructions.  At the other extreme,
   // some basic blocks are, due to splitting, just a single non-pc-changing
   // instruction.  In those cases, we should instrument just after that instruction
   // (and since postInstruction instrumentation is not yet implemented, it'll have
   // to be before that instruction for now)

   const kptr_t bbStartAddr = getStartAddr();
   const kptr_t bbNumInsnBytes = getNumInsnBytes();
   
   assert(bbNumInsnBytes >= 1);

   const instr_t *firstInsn = fnCode.get1Insn(bbStartAddr);
   const unsigned firstNumBytes = firstInsn->getNumBytes();
   
   if (firstNumBytes == bbNumInsnBytes) {
      // Just a single instruction.  If it's not a CTI, then we'd like to
      // instrument "postInstruction" of this instruction (and we must settle for
      // "preInsn" of this instruction for now).  If it is a CTI, then we pretty
      // much have to settle for "preInsn" of this instruction in all cases.
      return bbStartAddr;
   }

   // At least two instructions, find last insn

   kptr_t lastInsnAddr = bbStartAddr + firstNumBytes;
   instr_t *lastInsn = fnCode.get1Insn(lastInsnAddr);
   unsigned lastNumBytes = lastInsn->getNumBytes();
   while((lastInsnAddr + lastNumBytes) != (bbStartAddr + bbNumInsnBytes)) {
      lastInsnAddr += lastNumBytes;
      lastInsn = fnCode.get1Insn(lastInsnAddr);
      lastNumBytes = lastInsn->getNumBytes();
   }
   
   x86_instr::x86_cfi cfi;
   lastInsn->getControlFlowInfo(&cfi);
   
   if (cfi.fields.controlFlowInstruction) {
      // The basic block ends in a control flow instruction.
      // Instrumentation should be placed before the control flow instruction.
      // (Perhaps we can improve on this in the future)
      return lastInsnAddr;
   }
   else {
      // The last instruction is not a CTI; it is "normal",
      // so we certainly don't need to instrument before it.
      // But, we still have no postInstr instrumentation so the only 
      // option is to instrument before the final instruction.
      return lastInsnAddr;
   }
}
