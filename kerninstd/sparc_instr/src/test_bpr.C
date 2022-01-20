// test_bpr.C

#include "test_bpr.h"
#include "sparc_instr.h"
#include "util/h/rope-utils.h"

static void test_building_bprs(sparc_instr::RegCondCodes cond, bool annul, bool predict,
                               sparc_reg rs1,
                               int displacement_nbytes,
                               const string &disass_cmp) {
   sparc_instr i(sparc_instr::bpr, cond, annul, predict, rs1,
                 displacement_nbytes);

   const uint32_t pretendPC = 0x40000000;
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(pretendPC);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   {
      uint32_t brancheeAddr = 0;
      assert(i.isBranchToFixedAddress(pretendPC, brancheeAddr) &&
             brancheeAddr == pretendPC + displacement_nbytes);
   }

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   // register usage: rs1 should be used (no others).
   // pc should be maybe-written (since no unconditional bprs).
   assert(ru.definitelyUsedBeforeWritten.countIntRegs() == 1);
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(ru.definitelyWritten.count() == 0);

   assert(ru.maybeWritten.count() == 1); // pc
   assert(ru.maybeWritten.countIntRegs() == 0);
   assert(ru.maybeWritten.countMiscRegs() == 1);
   assert(ru.maybeWritten.exists(sparc_reg::reg_pc));

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   // control flow info
   assert(cfi.fields.controlFlowInstruction);
   assert(cfi.regCondition == cond);
   assert(cfi.fields.predict == predict);
   assert(cfi.fields.annul == annul);
   assert(!cfi.fields.isCall && !cfi.fields.isRet && !cfi.fields.isRetl &&
          !cfi.fields.isJmpl && !cfi.fields.isBicc &&
          !cfi.fields.isBPcc && !cfi.fields.isV9Return && !cfi.fields.isFBfcc &&
          !cfi.fields.isFBPfcc && !cfi.fields.isDoneOrRetry);
   assert(cfi.fields.isBPr);

   if (annul)
      assert(cfi.delaySlot == sparc_instr::dsWhenTaken);
   else
      assert(cfi.delaySlot == sparc_instr::dsAlways);
   
   assert(cfi.destination == sparc_instr::controlFlowInfo::pcrelative);
   
   assert(cfi.offset_nbytes == displacement_nbytes);
}

static void test_building_bprs(sparc_instr::RegCondCodes cond, bool annul, bool predict,
                               sparc_reg rs1,
                               const string &idisass_cmp) {
   for (unsigned lcv=0; lcv < 100; ++lcv) {
      int disp_ninsns = simmediate<16>::getRandomValue();
      int disp_nbytes = 4*disp_ninsns;
      
      const uint32_t newpc = 0x40000000 + disp_nbytes;

      string disass_cmp = idisass_cmp + addr2hex(newpc);
      
      test_building_bprs(cond, annul, predict, rs1, disp_nbytes, disass_cmp);
   }
}

static void test_building_bprs(sparc_instr::RegCondCodes cond,
                               bool annul, bool predict,
                               const string &idisass_cmp) {
   string disass_cmp = idisass_cmp;
   
   if (annul)
      disass_cmp += ",a";

   if (predict)
      disass_cmp += ",pt";
   else
      disass_cmp += ",pn";

   disass_cmp += ' ';

   for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
      sparc_reg rs1 = sparc_reg(sparc_reg::rawIntReg, rs1lcv);

      test_building_bprs(cond, annul, predict, rs1,
                         disass_cmp + rs1.disass() + ", ");
   }
}

static void test_building_bprs(sparc_instr::RegCondCodes cond,
                               const string &idisass_cmp) {
   string disass_cmp = idisass_cmp + sparc_instr::disass_reg_cond(cond);

   test_building_bprs(cond, false, false, disass_cmp);
   test_building_bprs(cond, false, true, disass_cmp);
   test_building_bprs(cond, true, false, disass_cmp);
   test_building_bprs(cond, true, true, disass_cmp);
}

void test_building_bprs() {
   string disass_cmp("br");
   
   test_building_bprs(sparc_instr::regZero, disass_cmp);
   test_building_bprs(sparc_instr::regLessOrEqZero, disass_cmp);
   test_building_bprs(sparc_instr::regLessZero, disass_cmp);
   test_building_bprs(sparc_instr::regNotZero, disass_cmp);
   test_building_bprs(sparc_instr::regGrZero, disass_cmp);
   test_building_bprs(sparc_instr::regGrOrEqZero, disass_cmp);
}
