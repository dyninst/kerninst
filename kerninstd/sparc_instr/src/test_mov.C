// test_mov.C

#include "test_mov.h"
#include "sparc_instr.h"
#include "util/h/rope-utils.h"

static void test_building_mov(sparc_reg rs1, sparc_reg rd) {
   string disass_cmp = "mov ";
   disass_cmp += rs1.disass();
   disass_cmp += ", ";
   disass_cmp += rd.disass();
   
   sparc_instr i(sparc_instr::mov, rs1, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x10000);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == sparc_reg::g0 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::g0));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.maybeUsedBeforeWritten.countIntRegs() == 0);

   assert(ru.definitelyWritten.count() == ru.definitelyWritten.countIntRegs());
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_mov(int val, sparc_reg rd) {
   string disass_cmp = "mov ";
   const unsigned val_abs = (val < 0 ? -val : val);
   disass_cmp += (val_abs != val ? "-" : "");
   disass_cmp += tohex(val_abs);
   disass_cmp += ", ";
   disass_cmp += rd.disass();

   sparc_instr i(sparc_instr::mov, val, rd);
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
   assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::g0));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.maybeUsedBeforeWritten.countIntRegs() == 0);

   assert(ru.definitelyWritten.count() == ru.definitelyWritten.countIntRegs());
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(!cfi.fields.controlFlowInstruction);
}

void test_building_mov() {
   for (unsigned rdlcv=1; rdlcv < 32; ++rdlcv) { // skip %g0 as a destination
      const sparc_reg rd = sparc_reg(sparc_reg::rawIntReg, rdlcv);

      for (unsigned rs1lcv=1; rs1lcv < 32; ++rs1lcv) {
         // skip %g0 as a source; that's reserved for the "clr" pseudo-instruction
         sparc_reg rs1 = sparc_reg(sparc_reg::rawIntReg, rs1lcv);

         test_building_mov(rs1, rd);
      }

      for (unsigned lcv=0; lcv < 100; ++lcv) {
         const int val = simmediate<13>::getRandomValue();
         if (val == 0)
            // arguably, 0 should be reserved for the "clr" pseudo-instruction
            // (I think so, even though sparc arch manual doesn't say so)
            continue;
         
         test_building_mov(val, rd);
      }
   }
}
