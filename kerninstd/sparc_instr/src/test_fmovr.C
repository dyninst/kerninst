// test_fmovr.C

#include "test_fmovr.h"
#include "sparc_instr.h"

static void test_building_fmovr(unsigned precision, sparc_reg rs1,
                                sparc_reg rs2, sparc_reg rd,
                                sparc_instr::RegCondCodes cond) {
   sparc_instr i(sparc_instr::fmovr, precision, cond, rs1, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == precision);
   assert(ru.maybeWritten.exists(rd));
   for (unsigned lcv=0; lcv<precision; ++lcv)
      assert(ru.maybeWritten.exists(sparc_reg(sparc_reg::f, rd.getFloatNum() + lcv)));
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == precision);
   assert(ru.maybeUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv<precision; ++lcv)
      assert(ru.maybeUsedBeforeWritten.exists(sparc_reg(sparc_reg::f, rs2.getFloatNum() + lcv)));
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_fmovr(unsigned precision, sparc_reg rs1,
                                sparc_reg rs2, sparc_reg rd) {
   test_building_fmovr(precision, rs1, rs2, rd, sparc_instr::regZero);
   test_building_fmovr(precision, rs1, rs2, rd, sparc_instr::regLessOrEqZero);
   test_building_fmovr(precision, rs1, rs2, rd, sparc_instr::regLessZero);
   test_building_fmovr(precision, rs1, rs2, rd, sparc_instr::regNotZero);
   test_building_fmovr(precision, rs1, rs2, rd, sparc_instr::regGrOrEqZero);
}

static void test_building_fmovr(unsigned precision, sparc_reg rs1) {
   for (unsigned rdlcv=0; rdlcv < 64; rdlcv += precision) {
      if (precision == 1 && rdlcv >= 32)
         break;

      sparc_reg rd(sparc_reg::f, rdlcv);

      for (unsigned rs2lcv=0; rs2lcv < 64; rs2lcv += precision) {
         if (precision == 1 && rs2lcv >= 32)
            break;

         sparc_reg rs2(sparc_reg::f, rs2lcv);

         test_building_fmovr(precision, rs1, rs2, rd);
      }
   }
}

static void test_building_fmovr(unsigned precision) {
   for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
      sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      test_building_fmovr(precision, rs1);
   }
}

void test_building_fmovr() {
   test_building_fmovr(1);
   test_building_fmovr(2);
   test_building_fmovr(4);
}

