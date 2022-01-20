// test_popc.C

#include "test_popc.h"
#include "sparc_instr.h"

void test_building_popc(sparc_reg rs2, sparc_reg rd) {
   sparc_instr i(sparc_instr::popc, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_popc() {
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
         sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
      
         test_building_popc(rs2, rd);
      }
   }
}

