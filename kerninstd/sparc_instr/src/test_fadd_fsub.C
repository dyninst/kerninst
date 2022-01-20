// test_fadd_fsub.C

#include "test_fadd_fsub.h"
#include "sparc_instr.h"

static void test_building_fadd_fsub(unsigned precision, bool add,
                                    sparc_reg rs1, sparc_reg rs2, sparc_reg rd,
                                    const string &disass_cmp) {
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x40000000);
   
   if (add) {
      sparc_instr i(sparc_instr::fadd, precision, rs1, rs2, rd);
      i.getInformation(&ru, NULL, &fl, &cfi, NULL);
   }
   else {
      sparc_instr i(sparc_instr::fsub, precision, rs1, rs2, rd);
      i.getInformation(&ru, NULL, &fl, &cfi, NULL);
   }

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == precision);
   assert(ru.definitelyWritten.exists(rd));
   for (unsigned lcv=0; lcv<precision; ++lcv)
      assert(ru.definitelyWritten.exists(sparc_reg(sparc_reg::f,
                                                   rd.getFloatNum() + lcv)));

   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == precision * (rs1==rs2 ? 1 : 2));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   for (unsigned lcv=0; lcv<precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                   rs1.getFloatNum() + lcv)));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv<precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                   rs2.getFloatNum() + lcv)));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static const string precision_strings[] = {"", "s", "d", "", "q"};

static void test_building_fadd_fsub(unsigned precision, bool add,
                                    const string &idisass_cmp) {
   assert(precision == 1 || precision == 2 || precision == 4);

   const string disass_cmp = idisass_cmp + precision_strings[precision] + ' ';
   
   for (unsigned rs1lcv=0; rs1lcv < 64; rs1lcv += precision) {
      if (precision == 1 && rs1lcv >= 32)
         break;
      sparc_reg rs1(sparc_reg::f, rs1lcv);

      const string disass_cmp_rs1 = disass_cmp + rs1.disass() + ", ";

      for (unsigned rs2lcv=0; rs2lcv < 64; rs2lcv += precision) {
         if (precision == 1 && rs2lcv >= 32)
            break;
         sparc_reg rs2(sparc_reg::f, rs2lcv);

         const string disass_cmp_rs2 = disass_cmp_rs1 + rs2.disass() + ", ";

         for (unsigned rdlcv=0; rdlcv < 64; rdlcv += precision) {
            if (precision == 1 && rdlcv >= 32)
               break;

            sparc_reg rd(sparc_reg::f, rdlcv);

            const string disass_cmp_rd = disass_cmp_rs2 + rd.disass();

            test_building_fadd_fsub(precision, add, rs1, rs2, rd,
                                    disass_cmp_rd);
         }
      }
   }

}

static void test_building_fadd_fsub(bool add) {
   string disass_cmp = add ? "fadd" : "fsub";

   unsigned opf = 0x040;
   
   for (unsigned precision=1; precision <= 3; ++precision) {
      opf += precision;
      
      if (!add)
         opf += 0x004;
      
      test_building_fadd_fsub(precision < 3 ? precision : 4, add, disass_cmp);
   }
}

void test_building_fadd() {
   test_building_fadd_fsub(true);
}

void test_building_fsub() {
   test_building_fadd_fsub(false);
}

