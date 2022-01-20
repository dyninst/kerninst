// test_shift.h

#include "test_shift.h"
#include "sparc_instr.h"

static void test_building_shift_2reg(sparc_instr::ShiftKind kind,
                                     bool extended,
                                     sparc_reg rd, sparc_reg rs1, sparc_reg rs2) {
   sparc_instr i(sparc_instr::shift, kind, extended, rd, rs1, rs2);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
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

static void test_building_shift_1reg(sparc_instr::ShiftKind kind,
                                     bool extended,
                                     sparc_reg rd, sparc_reg rs1, unsigned shiftamt) {
   sparc_instr i(sparc_instr::shift, kind, extended, rd, rs1, shiftamt);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}
   
static void test_building_shift(sparc_instr::ShiftKind kind,
                                bool extended,
                                sparc_reg rd, sparc_reg rs1) {
   // 2 register style of shift
   for (unsigned rs2lcv=0; rs2lcv < 32; ++rs2lcv) {
      sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
      test_building_shift_2reg(kind, extended, rd, rs1, rs2);
         // if extended true --> use 6 bits of rs2 to determine shift amount
         // if extended false --> use just 5 bits of rs2 to determine shift amount
   }

   // 1 register style of shift (shift amt is an embedded constant)
   for (unsigned shiftamt=0; shiftamt < 64; ++shiftamt) {
      if (shiftamt >= 32 && !extended)
         continue;
      
      test_building_shift_1reg(kind, extended, rd, rs1, shiftamt);
         // if extended true --> treat 'shiftamt' as shcnt64 (new style)
         // if extended true --> treat 'shiftamt' as shcnt32 (old style)
   }
}

static void test_building_shift(sparc_instr::ShiftKind kind,
                                bool extended) {
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);

         test_building_shift(kind, extended, rd, rs1);
      }
   }
   
}

static void test_building_shift(sparc_instr::ShiftKind kind) {
   test_building_shift(kind, true); // extended
   test_building_shift(kind, false); // not extended
}

void test_building_shift() {
   test_building_shift(sparc_instr::leftLogical);
   test_building_shift(sparc_instr::rightLogical);
   test_building_shift(sparc_instr::rightArithmetic);
}

