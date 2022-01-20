// test_int2float.C

#include "test_int2float.h"
#include "sparc_instr.h"

static void test_building_int2float(bool src_reg_extended, sparc_reg rs2,
                                    unsigned dest_reg_precision, sparc_reg rd,
                                    const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp +
                            sparc_instr::disassFloatPrecision(dest_reg_precision) +
                            " " + rs2.disass() + ", " + rd.disass();
   
   if (src_reg_extended)
      assert(rs2.getFloatNum() % 2 == 0);
   
   sparc_instr i(sparc_instr::int2float, src_reg_extended, dest_reg_precision,
                 rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl;
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);
   
   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(ru.definitelyWritten.count() == dest_reg_precision);
   assert(ru.definitelyWritten.exists(rd));
   for (unsigned lcv=0; lcv < dest_reg_precision; ++lcv)
      assert(ru.definitelyWritten.exists(sparc_reg(sparc_reg::f,
                                                   rd.getFloatNum() + lcv)));

   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == src_reg_extended ? 2U : 1U);
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   if (src_reg_extended)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs2.getFloatNum() + 1)));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_int2float(bool src_reg_extended, sparc_reg rs2,
                                    const string &idisass_cmp) {
   // First, try single precision destination
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      const sparc_reg rd(sparc_reg::f, rdlcv);

      test_building_int2float(src_reg_extended, rs2, 1, rd, idisass_cmp);
   }
   
   // Next, try double precision destination
   for (unsigned rdlcv=0; rdlcv < 64; rdlcv += 2) {
      const sparc_reg rd(sparc_reg::f, rdlcv);
      
      test_building_int2float(src_reg_extended, rs2, 2, rd, idisass_cmp);
   }
   
   // And finally, quad precision destination
   for (unsigned rdlcv=0; rdlcv < 64; rdlcv += 4) {
      const sparc_reg rd(sparc_reg::f, rdlcv);
      
      test_building_int2float(src_reg_extended, rs2, 4, rd, idisass_cmp);
   }
}

void test_building_int2float() {
   // First, try fito{s,d,q} (32-bit integer source, in a float register)
   const string disass_cmp = "f";

   for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
      const sparc_reg rs2(sparc_reg::f, rs2lcv);

      test_building_int2float(false, rs2, disass_cmp + "ito");
   }

   // Next, try fxto{s,d,q} (64-bit integer source, in a float register)
   for (unsigned rs2lcv=0; rs2lcv<64; rs2lcv+=2) {
      sparc_reg rs2(sparc_reg::f, rs2lcv);

      test_building_int2float(true, rs2, disass_cmp + "xto");
   }
}

