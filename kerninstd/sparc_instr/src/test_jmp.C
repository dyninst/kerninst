// test_jmp.C

#include "test_jmp.h"
#include "sparc_instr.h"

void test_building_jmp(sparc_reg rs1, sparc_reg rs2) {
   sparc_instr i(sparc_instr::jump, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);   

   assert(ru.definitelyWritten.count() == 1);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));

   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(cfi.fields.controlFlowInstruction);
   assert(!cfi.fields.isCall && !cfi.fields.isRet && !cfi.fields.isRetl &&
          !cfi.fields.isBicc && !cfi.fields.isBPcc &&
          !cfi.fields.isBPr && !cfi.fields.isV9Return && !cfi.fields.isFBfcc &&
          !cfi.fields.isFBPfcc &&
          !cfi.fields.isDoneOrRetry);
   assert(cfi.fields.isJmpl);
}

void test_building_jmp(sparc_reg rs1, int offset) {
   sparc_instr i(sparc_instr::jump, rs1, offset);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);   

   assert(ru.definitelyWritten.count() == 1);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));

   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(cfi.fields.controlFlowInstruction);
   assert(!cfi.fields.isCall && !cfi.fields.isBicc && !cfi.fields.isBPcc &&
          !cfi.fields.isBPr && !cfi.fields.isV9Return && !cfi.fields.isFBfcc &&
          !cfi.fields.isFBPfcc &&
          !cfi.fields.isDoneOrRetry);
   if (rs1 == sparc_reg::o7 && (offset == 8 || offset == 12))
      assert(cfi.fields.isRetl);
   else
      assert(!cfi.fields.isRetl);

   if (rs1 == sparc_reg::i7 && (offset == 8 || offset == 12))
      assert(cfi.fields.isRet);
   else
      assert(!cfi.fields.isRet);

   assert(cfi.fields.isJmpl);
}

void test_building_jmp() {
   for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
      sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
      for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
         sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);

         test_building_jmp(rs1, rs2);
      }

      const int min_allowable_simm13 = simmediate<13>::getMinAllowableValue();
      const int max_allowable_simm13 = simmediate<13>::getMaxAllowableValue();

      for (unsigned lcv=0; lcv < 100; ++lcv)
         test_building_jmp(rs1, min_allowable_simm13 + lcv);
   
      for (int lcv=-50; lcv < 50; ++lcv)
         test_building_jmp(rs1, lcv);
   
      for (unsigned lcv=0; lcv < 100; ++lcv)
         test_building_jmp(rs1, max_allowable_simm13 - lcv);
   }
}
