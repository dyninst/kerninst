// test_mulx_sdivx_udivx.C

#include "test_mulx_sdivx_udivx.h"
#include "sparc_instr.h"

static void test_building_mulx_sdivx_udivx(bool multiply, bool signedDivide,
                                           sparc_reg rd, sparc_reg rs1, sparc_reg rs2) {
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;

   if (multiply) {
      sparc_instr i(sparc_instr::mulx, rs1, rs2, rd);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   else if (signedDivide) {
      sparc_instr i(sparc_instr::sdivx, rs1, rs2, rd);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   else {
      sparc_instr i(sparc_instr::udivx, rs1, rs2, rd);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_mulx_sdivx_udivx(bool multiply, bool signedDivide,
                                           sparc_reg rd, sparc_reg rs1, int val) {
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;

   if (multiply) {
      sparc_instr i(sparc_instr::mulx, rs1, val, rd);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   else if (signedDivide) {
      sparc_instr i(sparc_instr::sdivx, rs1, val, rd);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   else {
      sparc_instr i(sparc_instr::udivx, rs1, val, rd);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_mulx_sdivx_udivx(bool multiply, bool signedDivide) {
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
         
         for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
            sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
            
            test_building_mulx_sdivx_udivx(multiply, signedDivide, rd, rs1, rs2);
         }
         
         int minAllowableValue = simmediate<13>::getMinAllowableValue();
         int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_mulx_sdivx_udivx(multiply, signedDivide,
                                           rd, rs1, minAllowableValue + lcv);
         for (int lcv=-50; lcv < 50; ++lcv)
            test_building_mulx_sdivx_udivx(multiply, signedDivide,
                                           rd, rs1, lcv);
         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_mulx_sdivx_udivx(multiply, signedDivide,
                                           rd, rs1, maxAllowableValue - lcv);
      }
   }
}

/* ---------------------------------------------------------------------- */

void test_building_mulx() {
   return test_building_mulx_sdivx_udivx(true, false);
}
void test_building_udivx() {
   return test_building_mulx_sdivx_udivx(false, false);
}
void test_building_sdivx() {
   return test_building_mulx_sdivx_udivx(false, true);
}
