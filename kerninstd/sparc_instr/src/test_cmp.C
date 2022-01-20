// test_cmp.C

#include "test_cmp.h"
#include "sparc_instr.h"
#include "util/h/rope-utils.h"

static void test_building_cmp(sparc_reg rs1, sparc_reg rs2, const string &disass_cmp) {
   sparc_instr i(sparc_instr::cmp, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x1000);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);
   
   assert(fl.result == disass_cmp);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   assert(ru.definitelyWritten.count() == 2);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
   assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_cmp(sparc_reg rs1, int val, const string &idisass_cmp) {
   sparc_instr i(sparc_instr::cmp, rs1, val);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x1000);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   string disass_cmp = idisass_cmp;
   if (val < 0) {
      disass_cmp += '-';
      disass_cmp += tohex((unsigned)-val);
   }
   else
      disass_cmp += tohex((unsigned)val);

   assert(fl.result == disass_cmp);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   assert(ru.definitelyWritten.count() == 2);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
   assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));

   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_cmp() {
   const string disass_cmp = "cmp ";
   
   for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
      sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);

      const string disass_cmp_rs1 = disass_cmp + rs1.disass() + ", ";
      
      for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
         sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);

         test_building_cmp(rs1, rs2, disass_cmp_rs1 + rs2.disass());
      }

      const int min_allowable_simm13 = simmediate<13>::getMinAllowableValue();
      const int max_allowable_simm13 = simmediate<13>::getMaxAllowableValue();

      for (unsigned lcv=0; lcv < 100; ++lcv)
         test_building_cmp(rs1, min_allowable_simm13 + lcv, disass_cmp_rs1);
   
      for (int lcv=-50; lcv < 50; ++lcv)
         test_building_cmp(rs1, lcv, disass_cmp_rs1);
   
      for (unsigned lcv=0; lcv < 100; ++lcv)
         test_building_cmp(rs1, max_allowable_simm13 - lcv, disass_cmp_rs1);
   }
}
