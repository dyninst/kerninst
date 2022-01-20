// test_subtract.C

#include "test_subtract.h"
#include "sparc_instr.h"

static void test_building_subtract(bool cc, bool carry, sparc_reg rs1,
                                   int offset, sparc_reg rd) {
   sparc_instr i(sparc_instr::sub, cc, carry, rd, rs1, offset);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   unsigned count = 0;
   if (rd != sparc_reg::g0)
      count++;
   if (cc)
      count += 2;
   
   assert(ru.definitelyWritten.count() == count);
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   if (cc) {
      assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));
      assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
   }

   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == carry ? 2U : 1U);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   if (carry)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::reg_icc));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_subtract(bool cc, bool carry, sparc_reg rs1, sparc_reg rs2,
                                   sparc_reg rd) {
   sparc_instr i(sparc_instr::sub, cc, carry, rd, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U) +
                                          (cc ? 2U : 0U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   if (cc) {
      assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));
      assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
   }

   assert(ru.maybeWritten.count() == 0);

   unsigned usedCount = (rs1 == rs2 ? 1U : 2U);
   if (carry)
      usedCount++;
   assert(ru.definitelyUsedBeforeWritten.count() == usedCount);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   if (carry)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::reg_icc));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}

// Versions of test_building_subtract without the cc or carry fields specified:
static void test_building_subtract(sparc_reg rs1,
                                   int offset, sparc_reg rd) {
   sparc_instr i(sparc_instr::sub, rs1, offset, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   unsigned count = 0;
   if (rd != sparc_reg::g0)
      count++;
   
   assert(ru.definitelyWritten.count() == count);
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));

   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1U);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_subtract(sparc_reg rs1, sparc_reg rs2,
                                   sparc_reg rd) {
   sparc_instr i(sparc_instr::sub, rs1, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));

   assert(ru.maybeWritten.count() == 0);

   unsigned usedCount = (rs1 == rs2 ? 1U : 2U);

   assert(ru.definitelyUsedBeforeWritten.count() == usedCount);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}


static void test_building_subtract(bool cc, bool carry) {
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
         for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
            sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
            test_building_subtract(cc, carry, rs1, rs2, rd);
         }

         const int minAllowableValue = simmediate<13>::getMinAllowableValue();
         const int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
         
         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_subtract(cc, carry, rs1, minAllowableValue + lcv, rd);

         for (int lcv=-50; lcv < 50; ++lcv)
            test_building_subtract(cc, carry, rs1, lcv, rd);

         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_subtract(cc, carry, rs1, maxAllowableValue - lcv, rd);
      }
   }
   
}

void test_building_subtract() {
   test_building_subtract(false, false); // no cc, no carry
   test_building_subtract(false, true); // no cc, carry
   test_building_subtract(true, false); // cc, no carry
   test_building_subtract(true, true); // cc, carry

   // Test the "new" subtract ctors: without cc or carry flags specified
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
         for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
            sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
            test_building_subtract(rs1, rs2, rd);
         }

         const int minAllowableValue = simmediate<13>::getMinAllowableValue();
         const int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
         
         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_subtract(rs1, minAllowableValue + lcv, rd);

         for (int lcv=-50; lcv < 50; ++lcv)
            test_building_subtract(rs1, lcv, rd);

         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_subtract(rs1, maxAllowableValue - lcv, rd);
      }
   }
   
   
}
