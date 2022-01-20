// test_v9return.C

#include "test_v9return.h"
#include "sparc_instr.h"

static void test_building_v9return(sparc_reg rs1, sparc_reg rs2) {
   sparc_instr i(sparc_instr::v9return, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == 1);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   
   assert(!ru.sr.isSave && !ru.sr.isRestore);
   assert(ru.sr.isV9Return);
   
   assert(cfi.fields.controlFlowInstruction);
   assert(!cfi.fields.isCall && !cfi.fields.isRet && !cfi.fields.isRetl &&
          !cfi.fields.isJmpl && !cfi.fields.isBicc &&
          !cfi.fields.isBPcc && !cfi.fields.isBPr && !cfi.fields.isFBfcc &&
          !cfi.fields.isFBPfcc &&
          !cfi.fields.isDoneOrRetry);
   assert(cfi.fields.isV9Return);

   assert(cfi.delaySlot == sparc_instr::dsAlways);
   assert(cfi.destination == sparc_instr::controlFlowInfo::doubleregister);
   assert(cfi.destreg1 == rs1);
   assert(cfi.destreg2 == rs2);
}

static void test_building_v9return(sparc_reg rs1, int offset) {
   sparc_instr i(sparc_instr::v9return, rs1, offset);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyWritten.count() == 1);
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(!ru.sr.isSave && !ru.sr.isRestore);
   assert(ru.sr.isV9Return);
   
   assert(cfi.fields.controlFlowInstruction);
   assert(!cfi.fields.isCall && !cfi.fields.isRet && !cfi.fields.isRetl &&
          !cfi.fields.isJmpl && !cfi.fields.isBicc &&
          !cfi.fields.isBPcc && !cfi.fields.isBPr && !cfi.fields.isFBfcc &&
          !cfi.fields.isFBPfcc &&
          !cfi.fields.isDoneOrRetry);
   assert(cfi.fields.isV9Return);

   assert(cfi.delaySlot == sparc_instr::dsAlways);
   assert(cfi.destination == sparc_instr::controlFlowInfo::registerrelative);
   assert(cfi.destreg1 == rs1);
   assert(cfi.offset_nbytes == offset);
}


/* ---------------------------------------------------------------------- */

void test_building_v9return() {
   for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
      const sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
      
      for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
         const sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);

         test_building_v9return(rs1, rs2);
      }
      
      const int minAllowableValue = simmediate<13>::getMinAllowableValue();
      const int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
      for (unsigned lcv=0; lcv < 100; ++lcv)
         test_building_v9return(rs1, minAllowableValue + lcv);

      for (int lcv=-50; lcv < 50; ++lcv)
         test_building_v9return(rs1, lcv);

      for (unsigned lcv=0; lcv < 100; ++lcv)
         test_building_v9return(rs1, maxAllowableValue - lcv);
   }
}


