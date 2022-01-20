// test_movcc.C

#include "test_movcc.h"
#include "sparc_instr.h"

static void test_building_movcc(sparc_reg rd, sparc_reg rs2,
                                sparc_instr::IntCondCodes cond,
                                bool xcc) {
   sparc_instr i(sparc_instr::movcc, cond, xcc, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   const bool always = (cond == sparc_instr::iccAlways);
   const bool never = (cond == sparc_instr::iccNever);
   
   if (always) {
      assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
      if (rd != sparc_reg::g0)
         assert(ru.definitelyWritten.exists(rd));

      assert(ru.maybeWritten.count() == 0);
   }
   else if (never) {
      assert(ru.maybeWritten.count() == 0);
      assert(ru.definitelyWritten.count() == 0);
   }
   else {
      assert(ru.definitelyWritten.count() == 0);

      assert(ru.maybeWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
      if (rd != sparc_reg::g0)
         assert(ru.maybeWritten.exists(rd));
   }

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(xcc ? sparc_reg::reg_xcc : sparc_reg::reg_icc));

   assert(ru.maybeUsedBeforeWritten.count() == 1);
   assert(ru.maybeUsedBeforeWritten.exists(rs2));

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);
}


static void test_building_movcc(sparc_reg rd, sparc_reg rs2,
                                sparc_instr::IntCondCodes cond) {
   test_building_movcc(rd, rs2, cond, true);
   test_building_movcc(rd, rs2, cond, false);
}

static void test_building_movcc(sparc_reg /* rd */,
                                int /* val */,
                                sparc_instr::IntCondCodes /* cond */,
                                bool /* xcc */) {
   // no sparc_instr ctor yet for this!
}

static void test_building_movcc(sparc_reg rd, int val,
                                sparc_instr::IntCondCodes cond) {
   test_building_movcc(rd, val, cond, true);
   test_building_movcc(rd, val, cond, false);
}


static void test_building_movcc(sparc_reg rd, sparc_reg rs2,
                                sparc_instr::FloatCondCodes cond,
                                sparc_reg fcc_reg) {
   sparc_instr i(sparc_instr::movcc, cond, fcc_reg.getFloatCondCodeNum(), rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);

   const bool always = (cond == sparc_instr::fccAlways);
   const bool never = (cond == sparc_instr::fccNever);
   
   if (always) {
      assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
      if (rd != sparc_reg::g0)
         assert(ru.definitelyWritten.exists(rd));

      assert(ru.maybeWritten.count() == 0);
   }
   else if (never) {
      assert(ru.maybeWritten.count() == 0);
      assert(ru.definitelyWritten.count() == 0);
   }
   else {
      assert(ru.definitelyWritten.count() == 0);

      assert(ru.maybeWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
      if (rd != sparc_reg::g0)
         assert(ru.maybeWritten.exists(rd));
   }

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(fcc_reg));

   assert(ru.maybeUsedBeforeWritten.count() == 1);
   assert(ru.maybeUsedBeforeWritten.exists(rs2));

   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);
}


static void test_building_movcc(sparc_reg rd, sparc_reg rs2,
                                sparc_instr::FloatCondCodes cond) {
   test_building_movcc(rd, rs2, cond, sparc_reg::reg_fcc0);
   test_building_movcc(rd, rs2, cond, sparc_reg::reg_fcc1);
   test_building_movcc(rd, rs2, cond, sparc_reg::reg_fcc2);
   test_building_movcc(rd, rs2, cond, sparc_reg::reg_fcc3);
}

static void test_building_movcc(sparc_reg /* rd */,
                                int /* val */,
                                sparc_instr::FloatCondCodes /* cond */,
                                sparc_reg /* fcc_reg */) {
   // no sparc_instr ctor yet for this!
}

static void test_building_movcc(sparc_reg rd, int val,
                                sparc_instr::FloatCondCodes cond) {
   test_building_movcc(rd, val, cond, sparc_reg::reg_fcc0);
   test_building_movcc(rd, val, cond, sparc_reg::reg_fcc1);
   test_building_movcc(rd, val, cond, sparc_reg::reg_fcc2);
   test_building_movcc(rd, val, cond, sparc_reg::reg_fcc3);
}

// ----------------------------------------------------------------------

void test_building_movcc() {
   for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
         sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
         
         for (unsigned condlcv=0; condlcv < 16; ++condlcv) {
            sparc_instr::IntCondCodes intCondCode = (sparc_instr::IntCondCodes)condlcv;
            test_building_movcc(rd, rs2, intCondCode);
         }
         for (unsigned condlcv=0; condlcv < 16; ++condlcv) {
            sparc_instr::FloatCondCodes floatCondCode = (sparc_instr::FloatCondCodes)condlcv;
            test_building_movcc(rd, rs2, floatCondCode);
         }
      }
      
      int minAllowableValue = simmediate<11>::getMinAllowableValue();
      int maxAllowableValue = simmediate<11>::getMaxAllowableValue();
   
      for (unsigned lcv=0; lcv < 100; ++lcv) {
         for (unsigned condlcv=0; condlcv < 16; ++condlcv) {
            sparc_instr::IntCondCodes intCondCode = (sparc_instr::IntCondCodes)condlcv;
            test_building_movcc(rd, minAllowableValue + lcv, intCondCode);
         }
         for (unsigned condlcv=0; condlcv < 16; ++condlcv) {
            sparc_instr::FloatCondCodes floatCondCode = (sparc_instr::FloatCondCodes)condlcv;
            test_building_movcc(rd, minAllowableValue + lcv, floatCondCode);
         }
      }
      
      for (int lcv=-50; lcv < 50; ++lcv) {
         for (unsigned condlcv=0; condlcv < 16; ++condlcv) {
            sparc_instr::IntCondCodes intCondCode = (sparc_instr::IntCondCodes)condlcv;
            test_building_movcc(rd, lcv, intCondCode);
         }
         for (unsigned condlcv=0; condlcv < 16; ++condlcv) {
            sparc_instr::FloatCondCodes floatCondCode = (sparc_instr::FloatCondCodes)condlcv;
            test_building_movcc(rd, lcv, floatCondCode);
         }
      }
      
      for (unsigned lcv=0; lcv < 100; ++lcv) {
         for (unsigned condlcv=0; condlcv < 16; ++condlcv) {
            sparc_instr::IntCondCodes intCondCode = (sparc_instr::IntCondCodes)condlcv;
            test_building_movcc(rd, maxAllowableValue - lcv, intCondCode);
         }
         for (unsigned condlcv=0; condlcv < 16; ++condlcv) {
            sparc_instr::FloatCondCodes floatCondCode = (sparc_instr::FloatCondCodes)condlcv;
            test_building_movcc(rd, maxAllowableValue - lcv, floatCondCode);
         }
      }
   }
}

