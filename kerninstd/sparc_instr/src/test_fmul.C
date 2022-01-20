// test_fmul.C

#include "test_fmul.h"
#include "sparc_instr.h"

static void test_building_fmul(unsigned src_precision, sparc_reg rs1, sparc_reg rs2,
                               unsigned dest_precision, sparc_reg rd) {
   sparc_instr i(sparc_instr::fmul, src_precision, rs1, rs2, dest_precision, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(ru.definitelyWritten.count() == dest_precision);
   assert(ru.definitelyWritten.exists(rd));
   for (unsigned lcv=0; lcv < dest_precision; ++lcv)
      assert(ru.definitelyWritten.exists(sparc_reg(sparc_reg::f,
                                                   rd.getFloatNum() + lcv)));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() ==
          src_precision * (rs1 == rs2 ? 1U : 2U));

   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   for (unsigned lcv=0; lcv < src_precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs1.getFloatNum() + lcv)));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv < src_precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs2.getFloatNum() + lcv)));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_fmul(unsigned src_precision, unsigned dest_precision) {
   for (unsigned rs1lcv=0; rs1lcv < 64; rs1lcv += src_precision) {
      if (rs1lcv >= 32 && src_precision == 1)
         break;
      sparc_reg rs1(sparc_reg::f, rs1lcv);

      for (unsigned rs2lcv=0; rs2lcv < 64; rs2lcv += src_precision) {
         if (rs2lcv >= 32 && src_precision == 1)
            break;
         sparc_reg rs2(sparc_reg::f, rs2lcv);

         for (unsigned rdlcv=0; rdlcv < 64; rdlcv += dest_precision) {
            if (rdlcv >= 32 && dest_precision == 1)
               break;
            sparc_reg rd(sparc_reg::f, rdlcv);

            test_building_fmul(src_precision, rs1, rs2, dest_precision, rd);
         }
      }
   }
   
}

static void test_building_fmul(unsigned src_precision) {
   test_building_fmul(src_precision, src_precision);
   
   if (src_precision == 1)
      test_building_fmul(1, 2);
   else if (src_precision == 2)
      test_building_fmul(2, 4);
}

void test_building_fmul() {
   test_building_fmul(1);
   test_building_fmul(2);
   test_building_fmul(4);
}
