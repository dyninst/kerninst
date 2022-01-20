// test_flush.C

#include "test_flush.h"
#include "sparc_instr.h"

static void test_building_flush_2reg(sparc_reg rs1, sparc_reg rs2) {
   sparc_instr i(sparc_instr::flush, rs1, rs2);

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == 0);
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_flush_2reg(sparc_reg rs1) {
   for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
      sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);

      test_building_flush_2reg(rs1, rs2);
   }
}

static void test_building_flush_simm13(sparc_reg rs1, int simm13) {
   sparc_instr i(sparc_instr::flush, rs1, simm13);

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == 0);
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);
}


static void test_building_flush_simm13(sparc_reg rs1) {
   const int minAllowableValue = simmediate<13>::getMinAllowableValue();
   const int maxAllowableValue = simmediate<13>::getMaxAllowableValue();

   for (int lcv=0; lcv < 100; ++lcv)
      test_building_flush_simm13(rs1, minAllowableValue + lcv);
   
   for (int lcv=-50; lcv < 50; ++lcv)
      test_building_flush_simm13(rs1, lcv);
   
   for (int lcv=0; lcv < 100; ++lcv)
      test_building_flush_simm13(rs1, maxAllowableValue - lcv);
}

void test_building_flush() {
   for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
      sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
      test_building_flush_2reg(rs1);
      test_building_flush_simm13(rs1);
   }
}
