// vtimer_asserts.C

#include "vtimer_asserts.h"

// Because of old vs. new vtimer formats, the following is not valid any more.
// We'd need to make this more clever, and somehow distinguish between old and
// new formats.  This can be done by comparing the how-to-stop field to a known
// value for old, and for new.

//  void emit_assert_all_vtimers_are_stopped(tempBufferEmitter &em,
//                                           unsigned long allvtimers_kernelAddr,
//                                           sparc_reg vtimer_iter_reg, // 32 bit reg
//                                           sparc_reg vtimer_reg64 // 64 bit reg
//                                           ) {
//     // NOTE: this routine overwrites %ccr without saving it

//     em.emitImm32(allvtimers_kernelAddr, vtimer_iter_reg);

//     const unsigned loopbeginbytes = em.numInsnBytesEmitted();
//     em.emit(sparc_instr(sparc_instr::load, sparc_instr::ldUnsignedWord,
//                         vtimer_iter_reg, 0,
//                         vtimer_reg64));
//     em.emitCondBranchToForwardLabel(sparc_instr(sparc_instr::bpr,
//                                                 sparc_instr::regZero,
//                                                 false, // not annulled
//                                                 false, // predict not taken
//                                                 vtimer_reg64,
//                                                 0 // displacement for now
//                                                 ),
//                                     "done_assert_all_vtimers_are_stopped");
//     em.emit(sparc_instr(sparc_instr::nop));
   
//     em.emit(sparc_instr(sparc_instr::load, sparc_instr::ldExtendedWord,
//                         vtimer_reg64, 0, // offset 0 --> the state/total field
//                         vtimer_reg64));
//     em.emit(sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
//                         true, // extended
//                         vtimer_reg64, // dest
//                         vtimer_reg64, 62));
//     em.emit(sparc_instr(sparc_instr::tst, vtimer_reg64));
//     em.emit(sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
//                         true, // xcc (may not matter here)
//                         0x34));
   
//     em.emit(sparc_instr(sparc_instr::bicc, sparc_instr::iccAlways,
//                         false, // not annulled
//                         -(em.numInsnBytesEmitted() - loopbeginbytes)));
//     em.emit(sparc_instr(sparc_instr::add, vtimer_iter_reg, 4, vtimer_iter_reg));
   
//     em.emitLabel("done_assert_all_vtimers_are_stopped");
//  }

