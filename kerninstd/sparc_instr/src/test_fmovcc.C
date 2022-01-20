// test_fmovcc.C

#include "test_fmovcc.h"
#include "sparc_instr.h"

static void test_building_fmovcc(unsigned precision,
                                 sparc_reg rs2, sparc_reg rd,
                                 sparc_instr::IntCondCodes cond,
                                 bool xcc) {
   sparc_instr i(sparc_instr::fmovcc, precision, cond, xcc, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == precision);
   assert(ru.maybeWritten.exists(rd));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.maybeWritten.exists(sparc_reg(sparc_reg::f, rd.getFloatNum() + lcv)));
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(xcc ? sparc_reg::reg_xcc : sparc_reg::reg_icc));
   
   assert(ru.maybeUsedBeforeWritten.count() == precision);
   assert(ru.maybeUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.maybeUsedBeforeWritten.exists(sparc_reg(sparc_reg::f, rs2.getFloatNum() + lcv)));

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_fmovcc(unsigned precision,
                                 sparc_reg rs2, sparc_reg rd,
                                 sparc_instr::FloatCondCodes cond,
                                 unsigned fcc_num) {
   sparc_instr i(sparc_instr::fmovcc, precision, cond, fcc_num, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == precision);
   assert(ru.maybeWritten.exists(rd));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.maybeWritten.exists(sparc_reg(sparc_reg::f, rd.getFloatNum() + lcv)));
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::fcc_type,
                                                          fcc_num)));
   
   assert(ru.maybeUsedBeforeWritten.count() == precision);
   assert(ru.maybeUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.maybeUsedBeforeWritten.exists(sparc_reg(sparc_reg::f, rs2.getFloatNum() + lcv)));

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_fmovcc(unsigned precision,
                                 sparc_reg rs2, sparc_reg rd) {
   // For integer condition codes:
   for (unsigned lcv=0; lcv < 16; ++lcv) {
      sparc_instr::IntCondCodes cond = (sparc_instr::IntCondCodes)lcv;
      
      test_building_fmovcc(precision, rs2, rd, cond, false);
      test_building_fmovcc(precision, rs2, rd, cond, true);
   }

   // For float condition codes:
   for (unsigned lcv=0; lcv < 16; ++lcv) {
      sparc_instr::FloatCondCodes cond = (sparc_instr::FloatCondCodes)lcv;
      
      test_building_fmovcc(precision, rs2, rd, cond, 0);
      test_building_fmovcc(precision, rs2, rd, cond, 1);
      test_building_fmovcc(precision, rs2, rd, cond, 2);
      test_building_fmovcc(precision, rs2, rd, cond, 3);
   }
}

static void test_building_fmovcc(unsigned precision) {
   for (unsigned rdlcv=0; rdlcv<64; rdlcv += precision) {
      if (precision == 1 && rdlcv >= 32)
         break;
      
      sparc_reg rd(sparc_reg::f, rdlcv);

      for (unsigned rs2lcv=0; rs2lcv<64; rs2lcv += precision) {
         if (precision == 1 && rs2lcv >= 32)
            break;
      
         sparc_reg rs2(sparc_reg::f, rs2lcv);

         test_building_fmovcc(precision, rs2, rd);
      }
   }
}

void test_building_fmovcc() {
   test_building_fmovcc(1);
   test_building_fmovcc(2);
   test_building_fmovcc(4);
}
