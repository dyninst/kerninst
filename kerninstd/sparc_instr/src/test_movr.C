// test_movr.C

#include "test_movr.h"
#include "sparc_instr.h"

static void test_building_movr(sparc_instr::RegCondCodes cond,
                               sparc_reg rd, sparc_reg rs1, sparc_reg rs2) {
   // if (rs1 matches cond) then rd <- rs2

   sparc_instr i(sparc_instr::movr, cond, rs1, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == 0);

   assert(ru.maybeWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.maybeWritten.exists(rd));
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1U);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == 1U);
   assert(ru.maybeUsedBeforeWritten.exists(rs2));

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_movr(sparc_instr::RegCondCodes cond,
                               sparc_reg rd, sparc_reg rs1, int val) {
   // if (rs1 matches cond) then rd <- imm10

   sparc_instr i(sparc_instr::movr, cond, rs1, val, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == 0);

   assert(ru.maybeWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.maybeWritten.exists(rd));
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1U);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0U);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_movr(sparc_instr::RegCondCodes cond,
                               sparc_reg rd, sparc_reg rs1) {
   for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
      sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
      
      test_building_movr(cond, rd, rs1, rs2);
   }
   
   int minAllowableValue = simmediate<10>::getMinAllowableValue();
   int maxAllowableValue = simmediate<10>::getMaxAllowableValue();
   
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_movr(cond, rd, rs1, minAllowableValue + lcv);
   for (int lcv=-50; lcv < 50; ++lcv)
      test_building_movr(cond, rd, rs1, lcv);
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_movr(cond, rd, rs1, maxAllowableValue - lcv);
}

static void test_building_movr(sparc_instr::RegCondCodes cond) {
   for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);

         test_building_movr(cond, rd, rs1);
      }
   }
}

// ----------------------------------------------------------------------

void test_building_movr() {
   test_building_movr(sparc_instr::regZero);
   test_building_movr(sparc_instr::regLessOrEqZero);
   test_building_movr(sparc_instr::regLessZero);
   test_building_movr(sparc_instr::regNotZero);
   test_building_movr(sparc_instr::regGrZero);
   test_building_movr(sparc_instr::regGrOrEqZero);
}

