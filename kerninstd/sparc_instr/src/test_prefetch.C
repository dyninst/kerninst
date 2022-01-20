// test_prefetch.C

#include "test_prefetch.h"
#include "sparc_instr.h"

static void test_building_prefetch(bool altSpace, sparc_instr::PrefetchFcn fcn,
                                   sparc_reg rs1, sparc_reg rs2) {
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;

   if (altSpace) {
      sparc_instr i(sparc_instr::prefetchAltSpace, fcn, rs1, rs2);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   else {
      sparc_instr i(sparc_instr::prefetch, fcn, rs1, rs2);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }

   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_prefetch(bool altSpace, sparc_instr::PrefetchFcn fcn,
                                   sparc_reg rs1, int offset) {
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;

   if (altSpace) {
      sparc_instr i(sparc_instr::prefetchAltSpace, fcn, rs1, offset);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   else {
      sparc_instr i(sparc_instr::prefetch, fcn, rs1, offset);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }

   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1U + (altSpace ? 1U : 0U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   if (altSpace)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::reg_asi));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_prefetch(bool altSpace) {
   for (sparc_instr::PrefetchFcn fcn=sparc_instr::prefetchForSeveralReads;
        fcn <= sparc_instr::prefetchPage;
        fcn=(sparc_instr::PrefetchFcn)((int)fcn + 1)) {
      
      
      for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
         
         
         for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
            sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);

            test_building_prefetch(altSpace, fcn, rs1, rs2);
         }

         int minAllowableDisp = simmediate<13>::getMinAllowableValue();
         int maxAllowableDisp = simmediate<13>::getMaxAllowableValue();

         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_prefetch(altSpace, fcn, rs1, minAllowableDisp + lcv);
   
         for (int lcv=-50; lcv < 50; ++lcv)
            test_building_prefetch(altSpace, fcn, rs1, lcv);
   
         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_prefetch(altSpace, fcn, rs1, maxAllowableDisp - lcv);
      }
   }
   
}

void test_building_prefetch() {
   test_building_prefetch(false);
}

void test_building_prefetch_altSpace() {
   test_building_prefetch(true);
}

