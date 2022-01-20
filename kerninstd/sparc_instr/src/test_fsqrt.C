// test_fsqrt.C

#include "test_fsqrt.h"
#include "sparc_instr.h"

static void test_building_fsqrt(unsigned precision, sparc_reg rs2, sparc_reg rd) {
   sparc_instr i(sparc_instr::fsqrt, precision, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(ru.definitelyWritten.count() == precision);
   assert(ru.definitelyWritten.exists(rd));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.definitelyWritten.exists(sparc_reg(sparc_reg::f,
                                                   rd.getFloatNum() + lcv)));

   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == precision);
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs2.getFloatNum() + lcv)));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_fsqrt(unsigned precision) {
   for (unsigned rs2lcv=0; rs2lcv < 64; rs2lcv += precision) {
      if (precision == 1 && rs2lcv >= 32)
         break;

      sparc_reg rs2(sparc_reg::f, rs2lcv);

      for (unsigned rdlcv=0; rdlcv < 64; rdlcv += precision) {
         if (precision == 1 && rdlcv >= 32)
            break;

         sparc_reg rd(sparc_reg::f, rdlcv);

         test_building_fsqrt(precision, rs2, rd);
      }
   }
}
   
void test_building_fsqrt() {
   test_building_fsqrt(1);
   test_building_fsqrt(2);
   test_building_fsqrt(4);
}
