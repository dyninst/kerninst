// test_call.C

#include "test_call.h"
#include "sparc_instr.h"
#include "util/h/rope-utils.h"

static void test_building_calls(int displacement_ninsns,
                                const string &idisass_cmp) {
   const bool displacementIsNegative = (displacement_ninsns < 0);
   
   int displacement_nbytes = 4*displacement_ninsns;

   if (displacementIsNegative)
      // avoid wraparound
      assert(sparc_instr::inRangeOfCall(-displacement_nbytes, 0));
   else
      assert(sparc_instr::inRangeOfCall(0, 0+displacement_nbytes));
   
   const uint32_t pretendPC = displacementIsNegative ? -displacement_nbytes : 0;
   const uint32_t newPC = displacementIsNegative ? 0 : displacement_nbytes;

   const string disass_cmp = idisass_cmp + addr2hex(newPC);
   
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(pretendPC);
   
   sparc_instr i(sparc_instr::callandlink, displacement_nbytes);
   assert(i.isCallInstr());
   int displacement_cmp = -1;
   assert(i.isCallInstr(displacement_cmp) && displacement_cmp == displacement_nbytes);

   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   assert(ru.definitelyUsedBeforeWritten.count() == 0);
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.definitelyWritten.count() == 2);
   assert(ru.definitelyWritten.exists(sparc_reg::o7));
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));
   assert(ru.maybeWritten.count() == 0);

   assert(cfi.fields.controlFlowInstruction);
   assert(!cfi.fields.annul);
   assert(cfi.fields.isCall);
   assert(!cfi.fields.isRet && !cfi.fields.isRetl && !cfi.fields.isJmpl &&
          !cfi.fields.isBicc && !cfi.fields.isBPcc &&
          !cfi.fields.isBPr && !cfi.fields.isV9Return && !cfi.fields.isFBfcc &&
          !cfi.fields.isFBPfcc &&
          !cfi.fields.isDoneOrRetry);
   
   assert(cfi.delaySlot == sparc_instr::dsAlways);
   
   assert(cfi.destination == sparc_instr::controlFlowInfo::pcrelative);
   assert(cfi.offset_nbytes == displacement_nbytes);
}

static void test_building_calls(uint32_t destPC, uint32_t currPC,
                                const string &idisass_cmp) {
   sparc_instr i(sparc_instr::callandlink, destPC, currPC);
   
   long long destPC_ll = (long long)destPC;
   long long currPC_ll = (long long)currPC;
   long long offset_numbytes_ll = destPC_ll - currPC_ll;
   long offset_numbytes = (long)offset_numbytes_ll;
   assert(i.getRaw() == sparc_instr(sparc_instr::callandlink, offset_numbytes).getRaw());

   const string disass_cmp = idisass_cmp + addr2hex(destPC);
   
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(currPC);

   i.getInformation(&ru, NULL, &fl, &cfi, NULL);
   
   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   assert(ru.definitelyUsedBeforeWritten.count() == 0);
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.definitelyWritten.count() == 2);
   assert(ru.definitelyWritten.exists(sparc_reg::o7));
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));
   assert(ru.maybeWritten.count() == 0);

   assert(cfi.fields.controlFlowInstruction);
   assert(!cfi.fields.annul);
   assert(cfi.fields.isCall);
   assert(!cfi.fields.isRet && !cfi.fields.isRetl && !cfi.fields.isJmpl &&
          !cfi.fields.isBicc && !cfi.fields.isBPcc &&
          !cfi.fields.isBPr && !cfi.fields.isV9Return && !cfi.fields.isFBfcc &&
          !cfi.fields.isFBPfcc &&
          !cfi.fields.isDoneOrRetry);
   
   assert(cfi.delaySlot == sparc_instr::dsAlways);
   
   assert(cfi.destination == sparc_instr::controlFlowInfo::pcrelative);
   assert(cfi.offset_nbytes == offset_numbytes);
}

void test_building_calls() {
   const string disass_cmp("call ");

   for (unsigned lcv=0; lcv < 100; ++lcv) {
      // number of instructions displacement:
      int32_t numinsns_displacement = simmediate<30>::getRandomValue();
      
      test_building_calls(numinsns_displacement, disass_cmp);
   }
   
   // Style 2 of call instructions: we supply both the start addr and the dest addr
   test_building_calls(0x10077a88, 0x61288ba0,
                       disass_cmp); // had explicitly failed
}

