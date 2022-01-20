// test_sethi.C

#include "test_sethi.h"
#include "sparc_instr.h"
#include <stdlib.h> // random()

void test_building_sethi(uint32_t val, sparc_reg rd) {
   sparc_instr i(sparc_instr::sethi, sparc_instr::HiOf(), val, rd);
   
   sparc_reg check_rd=sparc_reg::g0;
   assert(i.isSetHi(check_rd) && check_rd == rd);
   uint32_t check_val = 0;
   assert(i.isSetHiToDestReg(rd, check_val) && check_val == sparc_instr::hi_of_32(val) << 10);

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));

   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 0);
   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_sethi(unsigned long val) {
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      const sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      test_building_sethi(val, rd);
   }
}

void test_building_sethi() {
   for (unsigned lcv=0; lcv < 5000; ++lcv) {
      unsigned long val = (unsigned long)random();
      
      test_building_sethi(val);
   }
}
