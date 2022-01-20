// test_bicc.C

#include "test_bicc.h"
#include "sparc_instr.h"
#include "util/h/rope-utils.h"

void test_building_biccs(sparc_instr::IntCondCodes cond, bool annul,
                         int offset_ninsns, const string &idisass_cmp) {
   int offset_nbytes = 4*offset_ninsns;
   const uint32_t pretendPC = 0x40000000;
   const uint32_t newPC = pretendPC + offset_nbytes;

   string disass_cmp = idisass_cmp;
   disass_cmp += addr2hex(newPC);
   
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(pretendPC);
      
   sparc_instr i(sparc_instr::bicc, cond, annul, offset_nbytes);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);
   
   {
      uint32_t brancheeAddr = 0;
      assert(i.isBranchToFixedAddress(pretendPC, brancheeAddr) &&
             brancheeAddr == pretendPC + offset_nbytes);
   }

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   if (cond == sparc_instr::iccAlways) {
      assert(ru.definitelyWritten.count() == 1);
      assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));
      
      assert(ru.maybeWritten.count() == 0);
      
      assert(ru.definitelyUsedBeforeWritten.count() == 0);
      
      assert(ru.maybeUsedBeforeWritten.count() == 0);
   }
   else if (cond == sparc_instr::iccNever) {
      assert(ru.definitelyWritten.count() == 0);
      assert(ru.maybeWritten.count() == 0);
      assert(ru.definitelyUsedBeforeWritten.count() == 0);
      assert(ru.maybeUsedBeforeWritten.count() == 0);
   }
   else {
      // conditional
      assert(ru.definitelyWritten.count() == 0);

      assert(ru.maybeWritten.count() == 1);
      assert(ru.maybeWritten.exists(sparc_reg::reg_pc));
      
      assert(ru.definitelyUsedBeforeWritten.count() == 1);
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::reg_icc));
      
      assert(ru.maybeUsedBeforeWritten.count() == 0);
   }
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   // control flow info
   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.iccCondition == cond);
   assert(cfi.xcc == false);
   assert(cfi.fields.annul == annul);
   
   assert(!cfi.fields.isCall && !cfi.fields.isRet && !cfi.fields.isRetl &&
          !cfi.fields.isJmpl &&
          !cfi.fields.isBPcc && !cfi.fields.isBPr && !cfi.fields.isV9Return &&
          !cfi.fields.isFBfcc &&
          !cfi.fields.isFBPfcc && !cfi.fields.isDoneOrRetry);
   assert(cfi.fields.isBicc);
   
   if (!annul)
      assert(cfi.delaySlot == sparc_instr::dsAlways);
   else if (cond == sparc_instr::iccAlways)
      assert(cfi.delaySlot == sparc_instr::dsNone);
   else if (cond == sparc_instr::iccNever)
      assert(cfi.delaySlot == sparc_instr::dsNever);
   else
      assert(cfi.delaySlot == sparc_instr::dsWhenTaken);
   
   assert(cfi.destination == sparc_instr::controlFlowInfo::pcrelative);
   assert(cfi.offset_nbytes == offset_nbytes);
}

void test_building_biccs(sparc_instr::IntCondCodes cond, bool annul,
                         const string &idisass_cmp) {
   string disass_cmp = idisass_cmp;
   if (annul)
      disass_cmp += ",a";
   disass_cmp += ' ';

   for (unsigned lcv=0; lcv < 100; ++lcv) {
      const int val = simmediate<22>::getRandomValue();
      
      test_building_biccs(cond, annul, val, disass_cmp);
   }
}

void test_building_biccs(sparc_instr::IntCondCodes cond,
                         const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp + sparc_instr::disass_int_cond_code(cond);

   test_building_biccs(cond, true, disass_cmp);
   test_building_biccs(cond, false, disass_cmp);
}

void test_building_biccs() {
   const string disass_cmp = "b";
   
   test_building_biccs(sparc_instr::iccAlways, disass_cmp);
   test_building_biccs(sparc_instr::iccNever, disass_cmp);
   test_building_biccs(sparc_instr::iccNotEq, disass_cmp);
   test_building_biccs(sparc_instr::iccEq, disass_cmp);
   test_building_biccs(sparc_instr::iccGr, disass_cmp);
   test_building_biccs(sparc_instr::iccLessOrEq, disass_cmp);
   test_building_biccs(sparc_instr::iccGrOrEq, disass_cmp);
   test_building_biccs(sparc_instr::iccLess, disass_cmp);
   test_building_biccs(sparc_instr::iccGrUnsigned, disass_cmp);
   test_building_biccs(sparc_instr::iccLessOrEqUnsigned, disass_cmp);
   test_building_biccs(sparc_instr::iccGrOrEqUnsigned, disass_cmp);
   test_building_biccs(sparc_instr::iccLessUnsigned, disass_cmp);
   test_building_biccs(sparc_instr::iccPositive, disass_cmp);
   test_building_biccs(sparc_instr::iccNegative, disass_cmp);
   test_building_biccs(sparc_instr::iccOverflowClear, disass_cmp);
   test_building_biccs(sparc_instr::iccOverflowSet, disass_cmp);
}

