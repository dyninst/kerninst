// test_float2int.C

#include "test_float2int.h"
#include "sparc_instr.h"

static void test_building_float2int(unsigned src_precision,
                                    bool dest_precision_extended,
                                    sparc_reg rs2,
                                    sparc_reg rd,
                                    const string &disass_cmp) {
   sparc_instr i(sparc_instr::float2int, src_precision, dest_precision_extended,
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

   assert(ru.definitelyWritten.count() == 0);

   assert(ru.maybeWritten.count() == (dest_precision_extended ? 2U : 1U));
   assert(ru.maybeWritten.exists(rd));
   if (dest_precision_extended)
      assert(ru.maybeWritten.exists(sparc_reg(sparc_reg::f,
                                              rd.getFloatNum() + 1)));
   
   assert(ru.definitelyUsedBeforeWritten.count() == src_precision);
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv < src_precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs2.getFloatNum() + lcv)));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_float2int(unsigned src_precision,
                                    bool dest_precision_extended,
                                    sparc_reg rs2,
                                    const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp + rs2.disass() + ", ";
   
   for (unsigned rdlcv=0; rdlcv < 64; rdlcv += (dest_precision_extended ? 2 : 1)) {
      if (!dest_precision_extended && rdlcv >= 32)
         break;
      
      const sparc_reg rd(sparc_reg::f, rdlcv);
      
      test_building_float2int(src_precision, dest_precision_extended,
                              rs2, rd, disass_cmp + rd.disass());
   }
}

static void test_building_float2int(unsigned src_precision,
                                    bool dest_precision_extended,
                                    const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp + (dest_precision_extended ? "x" : "i") + ' ';

   for (unsigned rs2lcv=0; rs2lcv < 64; rs2lcv += src_precision) {
      if (src_precision == 1 && rs2lcv >= 32)
         break;
      
      sparc_reg rs2(sparc_reg::f, rs2lcv);

      test_building_float2int(src_precision, dest_precision_extended, rs2, disass_cmp);
   }
}

static void test_building_float2int(unsigned src_precision,
                                    const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp +
                            sparc_instr::disassFloatPrecision(src_precision) + "to";

   test_building_float2int(src_precision, true, disass_cmp);
   test_building_float2int(src_precision, false, disass_cmp);
}

void test_building_float2int() {
   const string disass_cmp("f");
   
   test_building_float2int(1, disass_cmp);
   test_building_float2int(2, disass_cmp);
   test_building_float2int(4, disass_cmp);
}

