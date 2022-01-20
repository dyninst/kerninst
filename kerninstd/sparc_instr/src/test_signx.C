// test_signx.C

#include "test_signx.h"
#include "sparc_instr.h"

void test_building_signx(sparc_reg rs1) {
   sparc_instr i(sparc_instr::signx, rs1);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   assert(ru.definitelyWritten.count() == (rs1 == sparc_reg::g0 ? 0U : 1U));
   if (rs1 != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rs1));

   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1U);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_signx(sparc_reg rs1, sparc_reg rd) {
   sparc_instr i(sparc_instr::signx, rs1, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));

   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1U);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_signx() {
   for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
      sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
      test_building_signx(rs1);

      
      for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
         sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
         test_building_signx(rs1, rd);
      }
   }
}
