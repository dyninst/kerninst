// test_fcmp.C

#include "test_fcmp.h"
#include "sparc_instr.h"

static void test_building_fcmp(bool exceptionIfUnordered,
                               unsigned precision, sparc_reg condreg,
                               sparc_reg rs1, sparc_reg rs2,
                               const string &disass_cmp) {
   sparc_instr i(sparc_instr::fcmp, exceptionIfUnordered, precision,
                 condreg.getFloatCondCodeNum(),
                 rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x40000000);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);
   
   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == exceptionIfUnordered ? 0U : 1U);
   if (!exceptionIfUnordered)
      assert(ru.definitelyWritten.exists(condreg));
   
   assert(ru.maybeWritten.count() == exceptionIfUnordered ? 1U : 0U);
   if (exceptionIfUnordered)
      assert(ru.maybeWritten.exists(condreg));
   
   assert(ru.definitelyUsedBeforeWritten.count() == precision * (rs1 == rs2 ? 1 : 2));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs1.getFloatNum()+lcv)));

   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs2.getFloatNum()+lcv)));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_fcmp(bool exceptionIfUnordered,
                               unsigned precision, const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp + sparc_instr::disassFloatPrecision(precision) +
                            ' ';

   for (unsigned fccnum=0; fccnum < 4; ++fccnum) {
      const sparc_reg condreg(sparc_reg::fcc_type, fccnum);
      
      const string disass_cmp_fcc = disass_cmp + condreg.disass() + ", ";

      for (unsigned rs1lcv=0; rs1lcv < 64; rs1lcv += precision) {
         if (rs1lcv >= 32 && precision == 1)
            break;
      
         sparc_reg rs1(sparc_reg::f, rs1lcv);

         const string disass_cmp_rs1 = disass_cmp_fcc + rs1.disass() + ", ";
      
         for (unsigned rs2lcv=0; rs2lcv < 64; rs2lcv += precision) {
            if (rs2lcv >= 32 && precision == 1)
               break;
      
            sparc_reg rs2(sparc_reg::f, rs2lcv);
            
            const string disass_cmp_rs2 = disass_cmp_rs1 + rs2.disass();

            test_building_fcmp(exceptionIfUnordered, precision, condreg, rs1, rs2,
                               disass_cmp_rs2);
         }
      }
   }
}

void test_building_fcmp(bool exceptionIfUnordered, const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp + (exceptionIfUnordered ? "e" : "");

   test_building_fcmp(exceptionIfUnordered, 1, disass_cmp);
   test_building_fcmp(exceptionIfUnordered, 2, disass_cmp);
   test_building_fcmp(exceptionIfUnordered, 4, disass_cmp);
}

void test_building_fcmp() {
   const string disass_cmp("fcmp");

   test_building_fcmp(false, disass_cmp);
   test_building_fcmp(true, disass_cmp);
}
