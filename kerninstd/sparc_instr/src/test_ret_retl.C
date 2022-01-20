// test_ret_retl.C

#include "test_ret_retl.h"
#include "sparc_instr.h"

void test_building_ret() {
   sparc_instr i(sparc_instr::ret);
   
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == 1);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));

   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::i7));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(cfi.fields.controlFlowInstruction);
   assert(!cfi.fields.isCall);
   assert(cfi.fields.isRet);
   assert(!cfi.fields.isRetl);
   assert(cfi.fields.isJmpl);
   assert(!cfi.fields.isBicc && !cfi.fields.isBPcc && !cfi.fields.isBPr &&
          !cfi.fields.isV9Return &&
          !cfi.fields.isFBfcc && !cfi.fields.isFBPfcc && !cfi.fields.isDoneOrRetry);
   assert(cfi.delaySlot == sparc_instr::dsAlways);
   assert(cfi.destination == sparc_instr::controlFlowInfo::registerrelative);
   assert(cfi.destreg1 == sparc_reg::i7);
   assert(cfi.offset_nbytes == 8);

   assert(i.disassemble(0x1000) == string("ret"));
}

void test_building_retl() {
   sparc_instr i(sparc_instr::retl);
   
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == 1);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));

   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::o7));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(cfi.fields.controlFlowInstruction);
   assert(!cfi.fields.isCall);
   assert(!cfi.fields.isRet);
   assert(cfi.fields.isRetl);
   assert(cfi.fields.isJmpl);
   assert(!cfi.fields.isBicc && !cfi.fields.isBPcc && !cfi.fields.isBPr &&
          !cfi.fields.isV9Return &&
          !cfi.fields.isFBfcc && !cfi.fields.isFBPfcc && !cfi.fields.isDoneOrRetry);
   assert(cfi.delaySlot == sparc_instr::dsAlways);
   assert(cfi.destination == sparc_instr::controlFlowInfo::registerrelative);
   assert(cfi.destreg1 == sparc_reg::o7);
   assert(cfi.offset_nbytes == 8);

   assert(i.disassemble(0x1000) == string("retl"));
}

