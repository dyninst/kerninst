// test_store_float.C

#include "test_store_float.h"
#include "sparc_instr.h"

static void test_building_store_float(sparc_instr::FPStoreOps stOp,
                                      sparc_reg fp_src_reg,
                                      sparc_reg rs1, sparc_reg rs2) {
   unsigned precision = (stOp == sparc_instr::stSingleReg ? 1 :
                         stOp == sparc_instr::stDoubleReg ? 2 : 4);

   sparc_instr i(sparc_instr::storefp, stOp, fp_src_reg, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() - precision == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   assert(ru.definitelyUsedBeforeWritten.exists(fp_src_reg));
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg_set(sparc_reg_set::singleFloatReg, precision, fp_src_reg)));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_store_float(sparc_instr::FPStoreOps,
                                      sparc_reg fp_src_reg,
                                      sparc_reg rs1, int offset) {
                                      
}

static void test_building_store_float(sparc_instr::FPStoreOps stOp, 
                                      sparc_reg fp_src_reg) {
   for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
      sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
      for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
         sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);

         test_building_store_float(stOp, fp_src_reg, rs1, rs2);
      }

      int minAllowableDisp = simmediate<13>::getMinAllowableValue();
      int maxAllowableDisp = simmediate<13>::getMaxAllowableValue();

      for (unsigned lcv=0; lcv < 100; ++lcv)
         test_building_store_float(stOp, fp_src_reg,
                                   rs1, minAllowableDisp + lcv);
      
      for (int lcv=-50; lcv < 50; ++lcv)
         test_building_store_float(stOp, fp_src_reg, rs1, lcv);
      
      for (unsigned lcv=0; lcv < 100; ++lcv)
         test_building_store_float(stOp, fp_src_reg, rs1, maxAllowableDisp - lcv);
   }
}

void test_building_store_float() {
   // Single precision:
   for (unsigned lcv=0; lcv < 32; ++lcv) {
      test_building_store_float(sparc_instr::stSingleReg,
                                sparc_reg(sparc_reg::f, lcv));
   }
   
   // Double precision:
   for (unsigned lcv=0; lcv < 64; lcv += 2) {
      test_building_store_float(sparc_instr::stDoubleReg,
                                sparc_reg(sparc_reg::f, lcv));
   }
   
   // Quad precision:
   for (unsigned lcv=0; lcv < 64; lcv += 4) {
      test_building_store_float(sparc_instr::stQuadReg,
                                sparc_reg(sparc_reg::f, lcv));
   }
}
