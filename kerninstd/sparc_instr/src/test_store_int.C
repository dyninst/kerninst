// test_store_int.C

#include "test_store_int.h"
#include "sparc_instr.h"

static void test_building_store_int(bool /* alternateSpace */,
                                    sparc_instr::StoreOps stOp,
                                    sparc_reg rd, sparc_reg rs1, sparc_reg rs2) {
   sparc_instr i(sparc_instr::store, stOp, rd, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == 0);

   // add rd:
   unsigned count = 1;

   // add rs1, if appropriate:
   if (rs1 != rd) ++count;

   // add rs2, if appropriate:
   if (rs2 != rs1 && rs2 != rd) ++ count;
   
   assert(ru.definitelyUsedBeforeWritten.count() == count);
   assert(ru.definitelyUsedBeforeWritten.exists(rd));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_store_int(bool altSpace,
                                    sparc_instr::StoreOps stOp,
                                    sparc_reg rd, sparc_reg rs1, int offset) {
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   if (altSpace) {
      sparc_instr i(sparc_instr::storealtspace, stOp, rd, rs1, offset);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   else {
      sparc_instr i(sparc_instr::store, stOp, rd, rs1, offset);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == (rd == rs1 ? 1U : 2U) + altSpace ? 1U : 0U);
   assert(ru.definitelyUsedBeforeWritten.exists(rd));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   if (altSpace)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::reg_asi));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

      
static void test_building_store_int(bool altSpace,
                                    sparc_instr::StoreOps stOp) {
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);

         for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
            sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
            test_building_store_int(altSpace, stOp, rd, rs1, rs2);
         }

         const int minAllowableValue = simmediate<13>::getMinAllowableValue();
         const int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
         
         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_store_int(altSpace, stOp, rd, rs1, minAllowableValue + lcv);

         for (int lcv=-50; lcv < 50; ++lcv)
            test_building_store_int(altSpace, stOp, rd, rs1, lcv);

         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_store_int(altSpace, stOp, rd, rs1, maxAllowableValue - lcv);
      }
   }
}

static void test_building_store_int(bool alternateSpace) {
   test_building_store_int(alternateSpace, sparc_instr::stByte);
   test_building_store_int(alternateSpace, sparc_instr::stHalfword);
   test_building_store_int(alternateSpace, sparc_instr::stWord);
   test_building_store_int(alternateSpace, sparc_instr::stExtended);
}

void test_building_store_int() {
   test_building_store_int(false);
}

void test_building_store_int_altSpace() {
   test_building_store_int(true);
}
