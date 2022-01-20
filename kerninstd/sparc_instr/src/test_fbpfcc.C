// test_fbpfcc.C

#include "test_fbpfcc.h"
#include "sparc_instr.h"
#include "util/h/rope-utils.h"

static void test_building_fbpfccs(sparc_instr::FloatCondCodes cond,
                                  bool annul, bool predict,
                                  unsigned fccnum,
                                  int displacement_ninsns,
                                  const string &idisass_cmp) {
   const int displacement_nbytes = 4*displacement_ninsns;
   const uint32_t pretendPC = 0x40000000;
   const uint32_t newPC = pretendPC + displacement_nbytes;

   string disass_cmp = idisass_cmp + ' ' +
                      sparc_reg(sparc_reg::fcc_type, fccnum).disass() + ", " +
                      addr2hex(newPC);
   
   sparc_instr i(sparc_instr::fbpfcc, cond, annul, predict,
                 fccnum, displacement_nbytes);
   
   {
      uint32_t brancheeAddr = 0;
      assert(i.isBranchToFixedAddress(pretendPC, brancheeAddr) &&
             brancheeAddr == pretendPC + displacement_nbytes);
   }

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(pretendPC);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   if (cond == sparc_instr::fccAlways) {
      assert(ru.definitelyWritten.count() == 1);
      assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));
      assert(ru.maybeWritten.count() == 0);
      assert(ru.definitelyUsedBeforeWritten.count() == 0);
   }
   else if (cond == sparc_instr::fccNever) {
      assert(ru.definitelyWritten.count() == 0);
      assert(ru.maybeWritten.count() == 0);
      assert(ru.definitelyUsedBeforeWritten.count() == 0);
   }
   else {
      // something conditional
      assert(ru.definitelyWritten.count() == 0);
      assert(ru.maybeWritten.count() == 1); // pc
      assert(ru.maybeWritten.exists(sparc_reg::reg_pc));

      assert(ru.definitelyUsedBeforeWritten.count() == 1);
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg(sparc_reg::fcc_type,
                                                             fccnum)));
   }
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   // control flow info:
   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.fccCondition == cond);
   assert(cfi.fcc_num == fccnum);

   assert(cfi.fields.annul == annul);
   assert(!cfi.fields.isCall && !cfi.fields.isRet && !cfi.fields.isRetl &&
          !cfi.fields.isJmpl && !cfi.fields.isBicc &&
          !cfi.fields.isBPcc && !cfi.fields.isBPr && !cfi.fields.isV9Return &&
          !cfi.fields.isFBfcc &&
          !cfi.fields.isDoneOrRetry);
   assert(cfi.fields.isFBPfcc);

   if (!annul)
      assert(cfi.delaySlot == sparc_instr::dsAlways);
   else if (cond == sparc_instr::fccAlways)
      assert(cfi.delaySlot == sparc_instr::dsNone);
   else if (cond == sparc_instr::fccNever)
      assert(cfi.delaySlot == sparc_instr::dsNever);
   else
      assert(cfi.delaySlot == sparc_instr::dsWhenTaken);

   assert(cfi.destination == sparc_instr::controlFlowInfo::pcrelative);
   assert(cfi.offset_nbytes == displacement_nbytes);
}

static void test_building_fbpfccs(sparc_instr::FloatCondCodes cond,
                                  bool annul, bool predict,
                                  const string &idisass_cmp) {
   string disass_cmp = idisass_cmp;
   if (annul)
      disass_cmp += ",a";
   if (predict)
      disass_cmp += ",pt";
   else
      disass_cmp += ",pn";

   for (unsigned fccnum=0; fccnum < 3; ++fccnum) {
      for (unsigned lcv=0; lcv < 100; ++lcv) {
         const int val = simmediate<19>::getRandomValue();
      
         test_building_fbpfccs(cond, annul, predict, fccnum, val, disass_cmp);
      }
   }
}

static void test_building_fbpfccs(sparc_instr::FloatCondCodes cond,
                                  const string &idisass_cmp) {
   const string disass_cmp = idisass_cmp + sparc_instr::disass_float_cond_code(cond);

   test_building_fbpfccs(cond, true, true, disass_cmp);
   test_building_fbpfccs(cond, true, false, disass_cmp);
   test_building_fbpfccs(cond, false, true, disass_cmp);
   test_building_fbpfccs(cond, false, false, disass_cmp);
}

void test_building_fbpfccs() {
   const string disass_cmp("fb");
   
   test_building_fbpfccs(sparc_instr::fccAlways, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccNever, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccUnord, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccGr, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccUnordOrGr, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccLess, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccUnordOrLess, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccLessOrGr, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccNotEq, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccEq, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccUnordOrEq, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccGrOrEq, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccUnordOrGrOrEq, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccLessOrEq, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccUnordOrLessOrEq, disass_cmp);
   test_building_fbpfccs(sparc_instr::fccOrd, disass_cmp);
}

