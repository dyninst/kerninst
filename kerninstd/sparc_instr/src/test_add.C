// test_add.C

#include "test_add.h"
#include "sparc_instr.h"
#include "util/h/rope-utils.h"

static void test_building_adds(bool carry, bool cc,
                               sparc_reg rs1, sparc_reg rs2,
                               sparc_reg rd,
                               const string &disass_cmp) {
   sparc_instr i(sparc_instr::add,
                 cc, carry,
                 rd, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x10000);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   unsigned shouldBeUsed = (rs1 == rs2 ? 1 : 2);
   if (carry)
      shouldBeUsed++;
   
   assert(ru.definitelyUsedBeforeWritten.count() == shouldBeUsed);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));
   if (carry)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::reg_icc)); // but not xcc

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.maybeUsedBeforeWritten.countIntRegs() == 0);

   unsigned shouldBeWritten_ints = (rd == sparc_reg::g0 ? 0 : 1);
   unsigned shouldBeWritten = shouldBeWritten_ints + (cc ? 2 : 0);

   assert(ru.definitelyWritten.countIntRegs() == shouldBeWritten_ints);
   assert(ru.definitelyWritten.count() == shouldBeWritten);
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   if (cc) {
      assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
      assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));
   }
   
   assert(ru.maybeWritten.count() == 0);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_adds(bool carry, bool cc,
                               sparc_reg rs1,
                               int delta, sparc_reg rd,
                               const string &disass_cmp) {
   sparc_instr i(sparc_instr::add, cc, carry, rd, rs1, delta);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   sparc_instr::disassemblyFlags fl(0x10000);
   i.getInformation(&ru, NULL, &fl, &cfi, NULL);

   if (fl.result != disass_cmp) {
      cout << "expected \"" << disass_cmp << "\"" << endl;
      cout << "but got \"" << fl.result << "\"" << endl;
      assert(false);
   }
   
   unsigned shouldBeUsed = 1;
   if (carry)
      shouldBeUsed++;
   
   assert(ru.definitelyUsedBeforeWritten.count() == shouldBeUsed);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   if (carry)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::reg_icc)); // but not xcc

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.maybeUsedBeforeWritten.countIntRegs() == 0);

   unsigned shouldBeWritten_ints = (rd == sparc_reg::g0 ? 0 : 1);
   unsigned shouldBeWritten = shouldBeWritten_ints + (cc ? 2 : 0);

   assert(ru.definitelyWritten.countIntRegs() == shouldBeWritten_ints);
   assert(ru.definitelyWritten.count() == shouldBeWritten);
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   if (cc) {
      assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
      assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));
   }
   
   assert(ru.maybeWritten.count() == 0);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);

   if (rd != sparc_reg::g0 && !carry && !cc) {
      sparc_reg dummy_rs1 = sparc_reg::g0;
      sparc_reg dummy_rs2 = sparc_reg::g0;
      unsigned dummy_delta = 0;
      assert(i.isAddImmToDestReg(rd, dummy_rs1, dummy_delta));
      if (delta >= 0)
         // don't assert if negative delta, since isAddImmToDestReg() updates an
         // unsigned long.
         assert(dummy_delta == delta);

      assert(!i.isAdd2RegToDestReg(rd, dummy_rs1, dummy_rs2));
   }
}

static void test_building_adds(bool carry, bool cc,
                               sparc_reg rs1, sparc_reg rd,
                               const string &disass_cmp) {
   for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
      sparc_reg rs2 = sparc_reg(sparc_reg::rawIntReg, rs2lcv);
      
      test_building_adds(carry, cc, rs1, rs2, rd,
                         disass_cmp + ", " + rs2.disass() + ", " + rd.disass());
   }

   for (unsigned lcv=0; lcv < 100; ++lcv) {
      const int val = simmediate<13>::getRandomValue();
      const unsigned val_abs = val < 0 ? -val : val;

      if (val == 1 && !carry && rs1 == rd)
         // This is reserved for inc
         continue;
      
      test_building_adds(carry, cc, rs1, val, rd,
                         disass_cmp + ", " +
                         (val_abs != val ? "-" : "") + tohex(val_abs) +
                         ", " + rd.disass());
   }

   // And the immediate value "1" to stress "inc(cc)"

   if (rs1 == rd && !carry) {
      test_building_adds(carry, cc, rs1, +1, rd,
                         string("inc") +
                         (cc ? "cc" : "") +
                         " " + rd.disass());
   }
}

static void test_building_adds(bool carry, bool cc,
                               const string &disass_cmp) {
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
         sparc_reg rd = sparc_reg(sparc_reg::rawIntReg, rdlcv);
         sparc_reg rs1 = sparc_reg(sparc_reg::rawIntReg, rs1lcv);

         test_building_adds(carry, cc, rs1, rd, disass_cmp + rs1.disass());
      }
   }
}

void test_building_adds() {
   test_building_adds(false, false, "add ");
   test_building_adds(false, true, "addcc ");
   test_building_adds(true, false, "addc ");
   test_building_adds(true, true, "addccc ");
}

