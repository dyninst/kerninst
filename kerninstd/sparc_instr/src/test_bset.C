// test_bset.C

#include "test_bset.h"
#include "sparc_instr.h"
#include "util/h/rope-utils.h"

static void test_building_bset(int val, sparc_reg rd, const string &disass_cmp) {
   // bset val, rd is actually or %rd, val, %rd

   sparc_instr i(false, sparc_instr::bset, val, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x10000);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rd)); // yes, rd

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(ru.definitelyWritten.count() == 1);
   assert(ru.definitelyWritten.exists(rd));

   assert(ru.maybeWritten.count() == 0);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_bset() {
   string disass_cmp("bset ");
   
   for (unsigned rdlcv=1; rdlcv < 32; ++rdlcv) { // skip %g0 as rd
      const sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned randomlcv=0; randomlcv < 500; ++randomlcv) {
         const int val = simmediate<13>::getRandomValue();

         unsigned abs_val = (val < 0 ? -val : val);
         
         string val_disass = (val < 0 ? "-" : "");
         val_disass += tohex(abs_val);
         
         test_building_bset(val, rd, disass_cmp + val_disass + ", " + rd.disass());
      }
   }
}
