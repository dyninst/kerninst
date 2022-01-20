// test_float2float.C

#include "test_float2float.h"
#include "sparc_instr.h"

static void test_building_float2float(unsigned src_precision,
                                      unsigned dest_precision,
                                      sparc_reg rs2, sparc_reg rd,
                                      const string &disass_cmp) {
   assert(src_precision != dest_precision);
   
   sparc_instr i(sparc_instr::float2float, src_precision, dest_precision, rs2, rd);
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
   
   assert(ru.maybeWritten.count() == dest_precision);
   assert(ru.maybeWritten.exists(rd));
   for (unsigned lcv=0; lcv < dest_precision; ++lcv)
      assert(ru.maybeWritten.exists(sparc_reg(sparc_reg::f,
                                              rd.getFloatNum() + lcv)));
   
   assert(ru.definitelyUsedBeforeWritten.count() == src_precision);
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   for (unsigned lcv=0; lcv < src_precision; ++lcv)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::f,
                                                             rs2.getFloatNum() + lcv)));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_float2float(unsigned src_precision,
                                      unsigned dest_precision,
                                      const string &idisass_cmp) {
   assert(src_precision != dest_precision);

   const string disass_cmp = idisass_cmp +
                            sparc_instr::disassFloatPrecision(dest_precision) + " ";
   
   for (unsigned rs2lcv=0; rs2lcv<64; rs2lcv += src_precision) {
      if (rs2lcv >=32 && src_precision == 1)
         break;
      
      const sparc_reg rs2(sparc_reg::f, rs2lcv);

      for (unsigned rdlcv=0; rdlcv<64; rdlcv += dest_precision) {
         if (rdlcv >=32 && dest_precision == 1)
            break;
      
         const sparc_reg rd(sparc_reg::f, rdlcv);

         test_building_float2float(src_precision, dest_precision, rs2, rd,
                                   disass_cmp + rs2.disass() + ", " + rd.disass());
      }
   }
}

static void test_building_float2float(unsigned src_precision,
                                      const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp +
                            sparc_instr::disassFloatPrecision(src_precision) + "to";
   
   if (src_precision != 1)
      test_building_float2float(src_precision, 1, disass_cmp);

   if (src_precision != 2)
      test_building_float2float(src_precision, 2, disass_cmp);

   if (src_precision != 4)
      test_building_float2float(src_precision, 4, disass_cmp);
}

void test_building_float2float() {
   const string disass_cmp = "f";
   
   test_building_float2float(1, disass_cmp);
   test_building_float2float(2, disass_cmp);
   test_building_float2float(4, disass_cmp);
}

