// test_fmov_fneg_fabs.C

#include "test_fmov_fneg_fabs.h"
#include "sparc_instr.h"

static void test_building_fmov_fneg_fabs(unsigned codenum, unsigned precision,
                                         sparc_reg rs2, sparc_reg rd,
                                         const string &disass_cmp) {
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl;

   switch (codenum) {
      case 0: {
         sparc_instr i(sparc_instr::fmov, precision, rs2, rd);
         i.getInformation(&ru, NULL, &fl, &cfi, NULL);
         break;
      }
      case 1: {
         sparc_instr i(sparc_instr::fneg, precision, rs2, rd);
         i.getInformation(&ru, NULL, &fl, &cfi, NULL);
         break;
      }
      case 2: {
         sparc_instr i(sparc_instr::fabs, precision, rs2, rd);
         i.getInformation(&ru, NULL, &fl, &cfi, NULL);
         break;
      }
      default:
         assert(false);
   }
   
   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
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

static void test_building_fmov_fneg_fabs(unsigned codenum,
                                         unsigned precision, const string &idisass_cmp) {
   string disass_cmp = idisass_cmp + sparc_instr::disassFloatPrecision(precision) + ' ';
   
   for (unsigned rs2lcv=0; rs2lcv < 64; rs2lcv += precision) {
      if (precision == 1 && rs2lcv >= 32)
         break;

      const sparc_reg rs2(sparc_reg::f, rs2lcv);

      const string disass_cmp_rs2 = disass_cmp + rs2.disass() + ", ";

      for (unsigned rdlcv=0; rdlcv < 64; rdlcv += precision) {
         if (precision == 1 && rdlcv >= 32)
            break;

         const sparc_reg rd(sparc_reg::f, rdlcv);

         test_building_fmov_fneg_fabs(codenum, precision, rs2, rd,
                                      disass_cmp_rs2 + rd.disass());
      }
   }
}
   
void test_building_fmov_fneg_fabs(unsigned codenum, const string &disass_cmp) {
   test_building_fmov_fneg_fabs(codenum, 1, disass_cmp);
   test_building_fmov_fneg_fabs(codenum, 2, disass_cmp);
   test_building_fmov_fneg_fabs(codenum, 4, disass_cmp);
}

void test_building_fmov() {
   test_building_fmov_fneg_fabs(0, "fmov");
}

void test_building_fneg() {
   test_building_fmov_fneg_fabs(1, "fneg");
}

void test_building_fabs() {
   test_building_fmov_fneg_fabs(2, "fabs");
}
