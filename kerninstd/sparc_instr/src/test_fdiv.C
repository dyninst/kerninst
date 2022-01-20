// test_fdiv.C

#include "test_fdiv.h"
#include "sparc_instr.h"

static void test_building_fdiv(unsigned precision, sparc_reg rs1, sparc_reg rs2,
                               sparc_reg rd) {
   sparc_instr i(sparc_instr::fdiv, precision, rs1, rs2, rd);
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
   
   assert(ru.definitelyUsedBeforeWritten.count() ==
          precision * (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs1.getFloatNum() + lcv)));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs2.getFloatNum() + lcv)));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_fdiv(unsigned precision) {
   for (unsigned rs1lcv=0; rs1lcv < 64; rs1lcv += precision) {
      if (rs1lcv >= 32 && precision == 1)
         break;
      sparc_reg rs1(sparc_reg::f, rs1lcv);
      
      for (unsigned rs2lcv=0; rs2lcv < 64; rs2lcv += precision) {
         if (rs2lcv >= 32 && precision == 1)
            break;
         sparc_reg rs2(sparc_reg::f, rs2lcv);
      
         for (unsigned rdlcv=0; rdlcv < 64; rdlcv += precision) {
            if (rdlcv >= 32 && precision == 1)
               break;
            sparc_reg rd(sparc_reg::f, rdlcv);

            test_building_fdiv(precision, rs1, rs2, rd);
         }
      }
   }
   
}

void test_building_fdiv() {
   test_building_fdiv(1);
   test_building_fdiv(2);
   test_building_fdiv(4);
}

