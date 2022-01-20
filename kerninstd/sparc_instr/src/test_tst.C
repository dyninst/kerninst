// test_tst.C

#include "test_tst.h"
#include "sparc_instr.h"

static void test_building_tst(sparc_reg rs1) {
   sparc_instr i(sparc_instr::tst, rs1);

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == 2);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));
   assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == sparc_reg::g0 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::g0));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_tst() {
   for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
      sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
      test_building_tst(rs1);
   }
   
}
