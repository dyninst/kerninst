// test_save_restore.C

#include "test_save_restore.h"
#include "sparc_instr.h"

/* ---------------------------------------------------------------------- */

static void test_building_save(int offset) {
   sparc_instr i(sparc_instr::save, offset);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(i.isSave());
   assert(ru.sr.isSave);
   assert(!ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == 1);
   assert(ru.definitelyWritten.exists(sparc_reg::sp));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::sp));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_save(sparc_reg rs1, sparc_reg rs2, sparc_reg rd) {
   sparc_instr i(sparc_instr::save, rd, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(i.isSave());
   assert(ru.sr.isSave);
   assert(!ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_save(sparc_reg rs1, int simm13, sparc_reg rd) {
   sparc_instr i(sparc_instr::save, rd, rs1, simm13);

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(i.isSave());
   assert(ru.sr.isSave);
   assert(!ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_save() {
   const int minAllowableValue = simmediate<13>::getMinAllowableValue();
   const int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
   // First, the commonly-used form of the save ctor:
   for (int lcv=minAllowableValue; lcv <= maxAllowableValue; ++lcv) {
      test_building_save(lcv);
   }
   
   for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
      const sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
         const sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);

         for (unsigned rs2lcv=0; rs1lcv<32; ++rs1lcv) {
            const sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
            
            test_building_save(rs1, rs2, rd);
         }

         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_save(rs1, minAllowableValue + lcv, rd);

         for (int lcv=-50; lcv < 50; ++lcv)
            test_building_save(rs1, lcv, rd);

         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_save(rs1, maxAllowableValue - lcv, rd);
      }
   }
}

static void test_building_restore_common_form() {
   sparc_instr i(sparc_instr::restore);
   
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(i.isRestoreInstr());
   sparc_reg cmprd=sparc_reg::g0;
   assert(i.isRestoreInstr(cmprd) && cmprd == sparc_reg::g0);
   assert(!ru.sr.isSave && !ru.sr.isV9Return);
   assert(ru.sr.isRestore);

   assert(ru.definitelyWritten.count() == 0);
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::g0));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_restore(sparc_reg rs1, sparc_reg rs2, sparc_reg rd) {
   sparc_instr i(false, sparc_instr::restore, rs1, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   sparc_reg cmp_rs1 = sparc_reg::g0;
   sparc_reg cmp_rs2 = sparc_reg::g0;
   sparc_reg cmp_rd = sparc_reg::g0;
   int cmp_simm13 = -1;
   assert(i.isRestoreUsing2RegInstr(cmp_rs1, cmp_rs2, cmp_rd) &&
          cmp_rs1 == rs1 && cmp_rs2 == rs2 && cmp_rd == rd);
   assert(!i.isRestoreUsingSimm13Instr(cmp_rs1, cmp_simm13, cmp_rd));
   
   assert(i.isRestoreInstr());
   sparc_reg cmprd=sparc_reg::g0;
   assert(i.isRestoreInstr(cmprd) && cmprd == rd);
   assert(ru.sr.isRestore);
   assert(!ru.sr.isSave && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_restore(sparc_reg rs1, int simm13, sparc_reg rd) {
   sparc_instr i(sparc_instr::restore, rs1, simm13, rd);
   sparc_reg cmp_rs1 = sparc_reg::g0;
   sparc_reg cmp_rd = sparc_reg::g0;
   int cmp_simm13 = -1;
   assert(i.isRestoreUsingSimm13Instr(cmp_rs1, cmp_simm13, cmp_rd) &&
          cmp_rs1 == rs1 && cmp_simm13 == simm13 && cmp_rd == rd);
   sparc_reg cmp_rs2 = sparc_reg::g0;
   assert(!i.isRestoreUsing2RegInstr(cmp_rs1, cmp_rs2, cmp_rd));

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(i.isRestoreInstr());
   sparc_reg cmprd=sparc_reg::g0;
   assert(i.isRestoreInstr(cmprd) && cmprd == rd);
   assert(ru.sr.isRestore);
   assert(!ru.sr.isSave && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_restore() {
   const int minAllowableValue = simmediate<13>::getMinAllowableValue();
   const int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
   // First, the commonly-used form of the restore ctor:
   test_building_restore_common_form();
   
   for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
      const sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
         const sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);

         for (unsigned rs2lcv=0; rs1lcv<32; ++rs1lcv) {
            const sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
            
            test_building_restore(rs1, rs2, rd);
         }

         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_restore(rs1, minAllowableValue + lcv, rd);

         for (int lcv=-50; lcv < 50; ++lcv)
            test_building_restore(rs1, lcv, rd);

         for (unsigned lcv=0; lcv < 100; ++lcv)
            test_building_restore(rs1, maxAllowableValue - lcv, rd);
      }
   }
}
