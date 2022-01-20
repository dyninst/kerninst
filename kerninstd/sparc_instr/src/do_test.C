// do_test.C

#include <assert.h>
#include <stdlib.h>
#include "sparc_instr.h"
#include "test_add.h"
#include "test_bpr.h"
#include "test_fbfcc.h"
#include "test_fbpfcc.h"
#include "test_bicc.h"
#include "test_bpcc.h"
#include "test_call.h"
#include "test_cas.h"
#include "test_movcc.h"
#include "test_movr.h"

#include "test_v9return.h"
#include "test_save_restore.h"
#include "test_saved_restored.h"
#include "test_sethi.h"
#include "test_shift.h"
#include "test_store_float.h"
#include "test_store_int.h"
#include "test_subtract.h"
#include "test_prefetch.h"
#include "test_popc.h"
#include "test_fmovcc.h"
#include "test_fmovr.h"
#include "test_fadd_fsub.h"
#include "test_fcmp.h"
#include "test_float2int.h"
#include "test_float2float.h"
#include "test_int2float.h"
#include "test_fmov_fneg_fabs.h"
#include "test_fmul.h"
#include "test_fdiv.h"
#include "test_fsqrt.h"
#include "test_mulx_sdivx_udivx.h"
#include "test_cmp.h"
#include "test_jmp.h"
#include "test_signx.h"
#include "test_not.h"
#include "test_tst.h"
#include "test_neg.h"
#include "test_mov.h"
#include "test_bset.h"
#include "test_flush.h"
#include "test_ret_retl.h"
#include "util/h/mrtime.h"


// only execute test_that_ulls_are_still_in_2_regs for sparc, hence this ifdef:
#ifdef sparc_sun_solaris2_4

static uint32_t random32() {
   uint32_t result = random(); // actually returns a long
   return result;
}

extern "C" uint64_t ull_identity(uint64_t); // in .s file

void test_that_ulls_are_still_in_2_regs() {
   // Presently, egcs/g++ doesn't put "uint64_t" values into a single
   // register, even though they could (since we're 64 bits).  Assert that this
   // is still the case, lest routines such as ari_get_tick_raw in /dev/kerninst
   // return unexpected values.
   
   uint64_t a = 1;
   uint64_t b = 1;
   assert(b == a);
   b = ull_identity(a);
   assert(b == a);
   
   a = 1;
   a <<= 32;
   assert(a == 1ULL << 32);
   assert((a >> 32) == 1);
   b = ull_identity(a);
   assert(b == a);

   for (unsigned lcv=0; lcv < 100000; ++lcv) {
      uint32_t hibits = random32();
      uint32_t lobits = random32();
      
      uint64_t x = hibits;
      x <<= 32;
      x |= lobits;

      assert(x >> 32 == hibits);
      assert((x << 32) >> 32 == lobits);
      
      uint64_t y = ull_identity(x);
      assert(y == x);
   }
}
#endif

/* ---------------------------------------------------------------------- */

static void test_building_divide() {
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_retry() {
   // not yet implemented since there's no ctor in sparc_instr to build a done!!!
   cout << "nyi";
}

static void test_building_done() {
   // not yet implemented since there's no ctor in sparc_instr to build a done!!!
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_flushw() {
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_jmpl_2reg(sparc_reg rd, sparc_reg rs1) {
   for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
      sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
      
      sparc_instr i(sparc_instr::jumpandlink, rd, rs1, rs2);
      sparc_instr::registerUsage ru;
      sparc_instr::controlFlowInfo cfi;
      
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
      
      assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
      assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
      assert(ru.definitelyUsedBeforeWritten.exists(rs1));
      assert(ru.definitelyUsedBeforeWritten.exists(rs2));

      assert(ru.maybeUsedBeforeWritten.count() == 0);
      
      assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 1U : 2U));
      if (rd != sparc_reg::g0)
         assert(ru.definitelyWritten.exists(rd));
      assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));
      
      assert(ru.maybeWritten.count() == 0);
      
      assert(cfi.fields.controlFlowInstruction);
      if (rd == sparc_reg::o7)
         assert(cfi.fields.isCall);
      else
         assert(!cfi.fields.isCall);

      assert(!cfi.fields.isRet);
      assert(!cfi.fields.isRetl);

      assert(cfi.fields.isJmpl);
      assert(!cfi.fields.isBicc && !cfi.fields.isBPcc && !cfi.fields.isBPr &&
             !cfi.fields.isV9Return &&
             !cfi.fields.isFBfcc && !cfi.fields.isFBPfcc && !cfi.fields.isDoneOrRetry);

      assert(cfi.delaySlot == sparc_instr::dsAlways);

      assert(cfi.destination == sparc_instr::controlFlowInfo::doubleregister);
      assert(cfi.destreg1 == rs1);
      assert(cfi.destreg2 == rs2);
   }
}

static void test_building_jmpl_simm13(sparc_reg rd, sparc_reg rs1,
                                           int offset_numbytes) {
   sparc_instr i(sparc_instr::jumpandlink, rd, rs1, offset_numbytes);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
      
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
      
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
      
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 1U : 2U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   assert(ru.definitelyWritten.exists(sparc_reg::reg_pc));
      
   assert(ru.maybeWritten.count() == 0);
      
   assert(cfi.fields.controlFlowInstruction);
   if (rd == sparc_reg::o7)
      assert(cfi.fields.isCall);
   else
      assert(!cfi.fields.isCall);

   bool isRet = false;
   bool isRetl = false;
   if (rd == sparc_reg::g0 && (offset_numbytes == 8 || offset_numbytes == 12)) {
      if (rs1 == sparc_reg::i7)
         isRet = true;
      else if (rs1 == sparc_reg::o7)
         isRetl = true;
   }
   assert(cfi.fields.isRet == isRet);
   assert(cfi.fields.isRetl == isRetl);

   assert(cfi.fields.isJmpl);
   assert(!cfi.fields.isBicc && !cfi.fields.isBPcc && !cfi.fields.isBPr &&
          !cfi.fields.isV9Return &&
          !cfi.fields.isFBfcc && !cfi.fields.isFBPfcc && !cfi.fields.isDoneOrRetry);

   assert(cfi.delaySlot == sparc_instr::dsAlways);

   assert(cfi.destination == sparc_instr::controlFlowInfo::registerrelative);
   assert(cfi.destreg1 == rs1);
   assert(cfi.offset_nbytes == offset_numbytes);
}

static void test_building_jmpl_simm13(sparc_reg rd, sparc_reg rs1) {
   // Strangely, jmpl's simm13 doesn't implicitly get multiplied by 4.
   // (Unlike, say, a conditional branch).
   // Go figure.
   // At first glance, we might think that we "must" only pass multiples of 4 for
   // the simm13 in practice.
   // But that's not necessarily so; it's the result of [rs1 + simm13] that must
   // be a multiple of 4...rs1 may not be a multiple of 4 and simm13 may not either,
   // but the sum might be.

   int minAllowableValue = simmediate<13>::getMinAllowableValue();
   int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
   for (unsigned lcv=0; lcv < 100; lcv++)
      test_building_jmpl_simm13(rd, rs1, minAllowableValue + lcv);
   for (int lcv=-50; lcv < 50; lcv++)
      test_building_jmpl_simm13(rd, rs1, lcv);
   for (unsigned lcv=0; lcv < 100; lcv++)
      test_building_jmpl_simm13(rd, rs1, maxAllowableValue - lcv);
}

static void test_building_jmpl(bool) {
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
         
         test_building_jmpl_2reg(rd, rs1);
         test_building_jmpl_simm13(rd, rs1);
      }
   }
}

static void test_building_jmpl() {
   test_building_jmpl(false);
}

/* ---------------------------------------------------------------------- */

static void test_building_load_float_2reg(sparc_reg rs1, sparc_reg rs2,
                                          sparc_instr::FPLoadOps theFPLoadOp,
                                          sparc_reg rd) {
   // rd is a floating point reg; rs1 and rs2 are integer regs
   unsigned precision = (theFPLoadOp == sparc_instr::ldSingleReg ? 1 :
                         theFPLoadOp == sparc_instr::ldDoubleReg ? 2 :
                         theFPLoadOp == sparc_instr::ldQuadReg ? 4 : 0);
   assert(precision);

   sparc_instr i(false, sparc_instr::loadfp, theFPLoadOp, rs1, rs2, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);
   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));

   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(ru.definitelyWritten.count() == precision);
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.definitelyWritten.exists(sparc_reg(sparc_reg::f, rd.getFloatNum() + lcv)));
   assert(ru.definitelyWritten.exists(rd)); // redudant w/ above but what the heck
   assert(ru.definitelyWritten.expand1() == rd); // redundant w/ above but what the heck
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_load_float_2reg(sparc_reg rs1) {
   for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
      sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);

      // There are 32 32-bit fp regs, numbered %f0 thru %f31
      for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
         sparc_reg rd(sparc_reg::f, rdlcv);
         test_building_load_float_2reg(rs1, rs2, sparc_instr::ldSingleReg, rd);
      }

      // There are 32 64-bit fp regs, numbered %f0 thru %f62 step 2
      for (unsigned rdlcv=0; rdlcv<64; rdlcv+=2) {
         sparc_reg rd(sparc_reg::f, rdlcv);
         test_building_load_float_2reg(rs1, rs2, sparc_instr::ldDoubleReg, rd);
      }
      
      // There are 16 128-bit fp regs, numbered %f0 thru %f62 step 4
      for (unsigned rdlcv=0; rdlcv<64; rdlcv+=4) {
         sparc_reg rd(sparc_reg::f, rdlcv);
         test_building_load_float_2reg(rs1, rs2, sparc_instr::ldQuadReg, rd);
      }
   }
}

static void test_building_load_float_simm13(sparc_reg rs1, int offset,
                                            sparc_instr::FPLoadOps theFPLoadOp,
                                            sparc_reg rd) {
   // rd is a floating point reg; rs1 and rs2 are integer regs
   unsigned precision = (theFPLoadOp == sparc_instr::ldSingleReg ? 1 :
                         theFPLoadOp == sparc_instr::ldDoubleReg ? 2 :
                         theFPLoadOp == sparc_instr::ldQuadReg ? 4 : 0);
   assert(precision);

   sparc_instr i(false, sparc_instr::loadfp, theFPLoadOp, rs1, offset, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(ru.definitelyWritten.count() == precision);
   for (unsigned lcv=0; lcv < precision; ++lcv)
      assert(ru.definitelyWritten.exists(sparc_reg(sparc_reg::f, rd.getFloatNum() + lcv)));
   assert(ru.definitelyWritten.exists(rd)); // redudant w/ above but what the heck
   assert(ru.definitelyWritten.expand1() == rd); // redundant w/ above but what the heck
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_load_float_simm13(sparc_reg rs1, int offset) {
   // There are 32 32-bit fp regs, numbered %f0 thru %f31
   for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
      sparc_reg rd(sparc_reg::f, rdlcv);
      test_building_load_float_simm13(rs1, offset, sparc_instr::ldSingleReg, rd);
   }

   // There are 32 64-bit fp regs, numbered %f0 thru %f62 step 2
   for (unsigned rdlcv=0; rdlcv<64; rdlcv+=2) {
      sparc_reg rd(sparc_reg::f, rdlcv);
      test_building_load_float_simm13(rs1, offset, sparc_instr::ldDoubleReg, rd);
   }
      
   // There are 16 128-bit fp regs, numbered %f0 thru %f62 step 4
   for (unsigned rdlcv=0; rdlcv<64; rdlcv+=4) {
      sparc_reg rd(sparc_reg::f, rdlcv);
      test_building_load_float_simm13(rs1, offset, sparc_instr::ldQuadReg, rd);
   }
}

static void test_building_load_float_simm13(sparc_reg rs1) {
   int minAllowableValue = simmediate<13>::getMinAllowableValue();
   int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_load_float_simm13(rs1, minAllowableValue + lcv);
   for (int lcv=-50; lcv < 50; ++lcv)
      test_building_load_float_simm13(rs1, lcv);
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_load_float_simm13(rs1, maxAllowableValue - lcv);
}

static void test_building_load_float() {
   for (unsigned rs1lcv=0; rs1lcv < 32; ++rs1lcv) {
      sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
         
      test_building_load_float_2reg(rs1);
      test_building_load_float_simm13(rs1);
   }
}

/* ---------------------------------------------------------------------- */

static void test_building_load_floatstatereg() {
   // ld [addr], %fsr   (deprecated; loads into low 32 bits of %fsr)
   // ldx [addr], %fsr  (loads into the 64 bit register: %fsr)
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_load_float_altspace() {
   // lda [regaddr] imm_asi, freg(rd)
   // lda [reg+imm] %asi, freg(rd)
   // And double and quad versions, too.

   cout << "nyi";
}

/* ---------------------------------------------------------------------- */


static void test_building_load_int_2reg(sparc_instr::LoadOps theLoadOp,
                                        sparc_reg rd, sparc_reg rs1, sparc_reg rs2) {
   // alternate space variant: not yet implemented!!!
   sparc_instr i(sparc_instr::load, theLoadOp, rs1, rs2, rd);
   
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

static void test_building_load_int_simm13(bool altSpace, sparc_instr::LoadOps theLoadOp,
                                          sparc_reg rd, sparc_reg rs1,
                                          int offset) {
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;

   if (altSpace) {
      sparc_instr i(false, sparc_instr::loadaltspace, theLoadOp, rs1, offset, rd);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   else {
      sparc_instr i(sparc_instr::load, theLoadOp, rs1, offset, rd);
      i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   }
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == altSpace ? 2U : 1U);
   if (altSpace)
      assert(ru.definitelyUsedBeforeWritten.exists(sparc_reg::reg_asi));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);

   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_load_int(bool altSpace,
                                   sparc_instr::LoadOps theLoadOp,
                                   sparc_reg rd, sparc_reg rs1) {
   for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
      sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
      
      // alt space 2-reg load variant isn't yet implemented!
      test_building_load_int_2reg(theLoadOp, rd, rs1, rs2);
   }

   int minAllowableValue = simmediate<13>::getMinAllowableValue();
   int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_load_int_simm13(altSpace, theLoadOp, rd, rs1,
                                    minAllowableValue + lcv);
   for (int lcv=-50; lcv < 50; ++lcv)
      test_building_load_int_simm13(altSpace, theLoadOp, rd, rs1, lcv);
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_load_int_simm13(altSpace, theLoadOp, rd, rs1,
                                    maxAllowableValue - lcv);
}

static void test_building_load_int(bool altSpace, sparc_instr::LoadOps theLoadOp) {
   for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
         
         test_building_load_int(altSpace, theLoadOp, rd, rs1);
      }
   }
}

static void test_building_load_int(bool altSpace) {
   test_building_load_int(altSpace, sparc_instr::ldSignedByte);
   test_building_load_int(altSpace, sparc_instr::ldSignedHalfword);
   test_building_load_int(altSpace, sparc_instr::ldSignedWord);
   test_building_load_int(altSpace, sparc_instr::ldUnsignedByte);
   test_building_load_int(altSpace, sparc_instr::ldUnsignedHalfword);
   test_building_load_int(altSpace, sparc_instr::ldUnsignedWord);
   test_building_load_int(altSpace, sparc_instr::ldExtendedWord);
//   test_building_load_int(altSpace, sparc_instr::ldDoubleWord);
}

static void test_building_load_int() {
   test_building_load_int(false);
}
static void test_building_load_int_altSpace() {
   test_building_load_int(true);
}


/* ---------------------------------------------------------------------- */

static void test_building_ldstub_2reg(sparc_reg rd, sparc_reg rs1, sparc_reg rs2) {
   sparc_instr i(sparc_instr::loadstoreub, rd, rs1, rs2);
   
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

static void test_building_ldstub_simm13(sparc_reg rd, sparc_reg rs1, int offset) {
   sparc_instr i(sparc_instr::loadstoreub, rd, rs1, offset);
   
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   
   assert(ru.maybeWritten.count() == 0);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1U);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_ldstub(sparc_reg rd, sparc_reg rs1) {
   for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
      sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
      
      test_building_ldstub_2reg(rd, rs1, rs2);
   }
   
   int minAllowableValue = simmediate<13>::getMinAllowableValue();
   int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_ldstub_simm13(rd, rs1, minAllowableValue + lcv);
   for (int lcv=-50; lcv < 50; ++lcv)
      test_building_ldstub_simm13(rd, rs1, lcv);
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_ldstub_simm13(rd, rs1, maxAllowableValue - lcv);
}

static void test_building_ldstub() {
   for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
         
         test_building_ldstub(rd, rs1);
      }
   }
}

/* ---------------------------------------------------------------------- */

static void test_building_ldstub_altspace() {
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_logical(sparc_instr::logical_types theLogicType,
                                  bool cc,
                                  sparc_reg rd, sparc_reg rs1, sparc_reg rs2) {
   sparc_instr i(sparc_instr::logic, theLogicType, cc,
                 rd, rs1, rs2);

   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U) + (cc ? 2 : 0));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   if (cc) {
      assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
      assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));
   }
   
   assert(ru.maybeWritten.count() == 0U);
   
   assert(ru.definitelyUsedBeforeWritten.count() == (rs1 == rs2 ? 1U : 2U));
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));
   assert(ru.definitelyUsedBeforeWritten.exists(rs2));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_logical(sparc_instr::logical_types theLogicType,
                                  bool cc,
                                  sparc_reg rd, sparc_reg rs1, int offset) {
   sparc_instr i(sparc_instr::logic, theLogicType, cc,
                 rd, rs1, offset);
   
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U) +
      (cc ? 2 : 0));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   if (cc) {
      assert(ru.definitelyWritten.exists(sparc_reg::reg_xcc));
      assert(ru.definitelyWritten.exists(sparc_reg::reg_icc));
   }
   
   assert(ru.maybeWritten.count() == 0U);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 1);
   assert(ru.definitelyUsedBeforeWritten.exists(rs1));

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   
   assert(!cfi.fields.controlFlowInstruction);
}

static void test_building_logical(sparc_instr::logical_types theLogicType,
                                  bool cc,
                                  sparc_reg rd, sparc_reg rs1) {
   for (unsigned rs2lcv=0; rs2lcv<32; ++rs2lcv) {
      sparc_reg rs2(sparc_reg::rawIntReg, rs2lcv);
      
      test_building_logical(theLogicType, cc, rd, rs1, rs2);
   }
   
   int minAllowableValue = simmediate<13>::getMinAllowableValue();
   int maxAllowableValue = simmediate<13>::getMaxAllowableValue();
   
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_logical(theLogicType, cc, rd, rs1, minAllowableValue + lcv);
   for (int lcv=-50; lcv < 50; ++lcv)
      test_building_logical(theLogicType, cc, rd, rs1, lcv);
   for (unsigned lcv=0; lcv < 100; ++lcv)
      test_building_logical(theLogicType, cc, rd, rs1, maxAllowableValue - lcv);
}

static void test_building_logical(sparc_instr::logical_types theLogicType,
                                  bool cc) {
   for (unsigned rdlcv=0; rdlcv<32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);
      
      for (unsigned rs1lcv=0; rs1lcv<32; ++rs1lcv) {
         sparc_reg rs1(sparc_reg::rawIntReg, rs1lcv);
         
         test_building_logical(theLogicType, cc, rd, rs1);
      }
   }
}

static void test_building_and() {
   test_building_logical(sparc_instr::logic_and, false);
}
static void test_building_andcc() {
   test_building_logical(sparc_instr::logic_and, true);
}
static void test_building_andn() {
   test_building_logical(sparc_instr::logic_andn, false);
}
static void test_building_andncc() {
   test_building_logical(sparc_instr::logic_andn, true);
}

static void test_building_or() {
   test_building_logical(sparc_instr::logic_or, false);
}
static void test_building_orcc() {
   test_building_logical(sparc_instr::logic_or, true);
}
static void test_building_orn() {
   test_building_logical(sparc_instr::logic_orn, false);
}
static void test_building_orncc() {
   test_building_logical(sparc_instr::logic_orn, true);
}

static void test_building_xor() {
   test_building_logical(sparc_instr::logic_xor, false);
}
static void test_building_xorcc() {
   test_building_logical(sparc_instr::logic_xor, true);
}
static void test_building_xnor() {
   test_building_logical(sparc_instr::logic_xnor, false);
}
static void test_building_xnorcc() {
   test_building_logical(sparc_instr::logic_xnor, true);
}

/* ---------------------------------------------------------------------- */

static void test_building_membar() {
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_umul_smul_cc(bool /* is_signed */, bool /* cc */) {
   cout << "nyi";
}
static void test_building_umul() {
   test_building_umul_smul_cc(false, false);
}
static void test_building_umulcc() {
   test_building_umul_smul_cc(false, true);
}
static void test_building_smul() {
   test_building_umul_smul_cc(true, false);
}
static void test_building_smulcc() {
   test_building_umul_smul_cc(true, true);
}

static void test_building_mulscc() {
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_nop() {
   sparc_instr i(sparc_instr::nop);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;
   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   
   assert(ru.definitelyUsedBeforeWritten.count() == 0);
   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(ru.definitelyWritten.count() == 0);
   assert(ru.maybeWritten.count() == 0);
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   assert(!cfi.fields.controlFlowInstruction);

   assert(i.disassemble(0x1000) == pdstring("nop"));
}

/* ---------------------------------------------------------------------- */

static void test_building_rdpr() {
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_rd(sparc_reg rd, unsigned rs1num, const char *disassMatch,
                             const sparc_reg_set &usedRegMatch) {
   sparc_instr i(sparc_instr::readstatereg, rs1num, rd);
   sparc_instr::registerUsage ru;
   sparc_instr::controlFlowInfo cfi;

   i.getInformation(&ru, NULL, NULL, &cfi, NULL);
   assert(ru.definitelyWritten.count() == (rd == sparc_reg::g0 ? 0U : 1U));
   if (rd != sparc_reg::g0)
      assert(ru.definitelyWritten.exists(rd));
   assert(ru.maybeWritten.count() == 0);

   assert(ru.definitelyUsedBeforeWritten == usedRegMatch);

   assert(ru.maybeUsedBeforeWritten.count() == 0);
   assert(!ru.sr.isSave && !ru.sr.isRestore && !ru.sr.isV9Return);

   pdstring disassStr = i.disassemble(1000); // 1000 --> dummy instruction address

   pdstring disassCmp = "rd %";
   disassCmp += disassMatch;
   disassCmp += ", ";
   disassCmp += rd.disass();

   assert(disassStr == disassCmp);
}

static void test_building_rd() {
   // read ancillary state register
   for (unsigned rdlcv=0; rdlcv < 32; ++rdlcv) {
      sparc_reg rd(sparc_reg::rawIntReg, rdlcv);

      sparc_reg_set regsUsed = sparc_reg_set::empty;
      regsUsed += sparc_reg::reg_icc;
      regsUsed += sparc_reg::reg_xcc;
      test_building_rd(rd, 2, "ccr", regsUsed);

      test_building_rd(rd, 3, "asi", sparc_reg_set(sparc_reg::reg_asi));

      //test_building_rd(rd, 4, "tick");
      test_building_rd(rd, 5, "pc", sparc_reg_set(sparc_reg::reg_pc));

      regsUsed = sparc_reg_set::empty;
      regsUsed += sparc_reg::reg_fcc0;
      regsUsed += sparc_reg::reg_fcc1;
      regsUsed += sparc_reg::reg_fcc2;
      regsUsed += sparc_reg::reg_fcc3;
      test_building_rd(rd, 6, "fprs", regsUsed);

//        for (unsigned asrlcv=16; asrlcv <= 31; ++asrlcv) {
//           char buffer[20];
//           sprintf(buffer, "asr%d", asrlcv);
         
//           test_building_rd(rd, asrlcv, buffer);
//        }
   }
}

/* ---------------------------------------------------------------------- */

static void test_building_stbar() { // deprecated
   cout << "nyi";
}

static void test_building_sir() {
   cout << "nyi";
}

/* ---------------------------------------------------------------------- */

static void test_building_store_float_statusreg() {
   // stfsr and stxfsr
   cout << "nyi";
}

static void test_building_store_float_altSpace() {
   // stfsr and stxfsr
   cout << "nyi";
}

static void test_building_swap() {
   cout << "nyi";
}

static void test_building_swapa() {
   cout << "nyi";
}

static void test_building_tagged_add() {
   cout << "nyi";
}

static void test_building_tagged_sub() {
   cout << "nyi";
}

static void test_building_trap() {
   cout << "nyi";
}

static void test_building_wrpr() {
   cout << "nyi";
}

static void test_building_wr() {
   cout << "nyi";
}

// ----------------------------

static void test_building_call_addr() {
   cout << "nyi";
}

static void test_building_iprefetch() {
   cout << "nyi";
}

static void test_building_restore_trivial() {
   cout << "nyi";
}

static void test_building_save_trivial() {
   cout << "nyi";
}

static void test_building_inc() {
   cout << "nyi";
}

static void test_building_inccc() {
   cout << "nyi";
}

static void test_building_dec() {
   cout << "nyi";
}

static void test_building_deccc() {
   cout << "nyi";
}

static void test_building_btst() {
   cout << "nyi";
}

static void test_building_bclr() {
   cout << "nyi";
}

static void test_building_btog() {
   cout << "nyi";
}

static void test_building_clr_reg() {
   cout << "nyi";
}

static void test_building_clr_byte() {
   cout << "nyi";
}

static void test_building_clr_half() {
   cout << "nyi";
}

static void test_building_clr_word() {
   cout << "nyi";
}

static void test_building_clr_x() {
   cout << "nyi";
}

static void test_building_clruw() {
   cout << "nyi";
}

static void test_building_random_insns() {
   unsigned seed = getmrtime() & UINT_MAX;
   srandom(seed);
   
   for (unsigned lcv=0; lcv < 50000; ++lcv) {
      long raw_long = random();
      unsigned raw_unsigned = (unsigned)raw_long;
      
      sparc_instr i(raw_unsigned);
      
      sparc_instr::registerUsage ru;
      sparc_instr::disassemblyFlags fl(0x100000);
      sparc_instr::controlFlowInfo cfi;

      try {
         i.getInformation(&ru, NULL, &fl, &cfi, NULL);
      }
      catch (sparc_instr::unimplinsn) {
         assert(false && "unimplemented instruction exception");
      }
      catch (sparc_instr::unknowninsn) {
         // think kind of exception is OK
      }
   }
   
}

struct one_insn_test {
   const char *testName;
   void (*test_func)();
};

const one_insn_test allTests[] = {
   // The ordering: as in the SPARC v9 manual, appendix A
   {"random insns", test_building_random_insns},
   // The ordering: as in the SPARC v9 manual, appendix A
   {"add", test_building_adds},
   {"BPr", test_building_bprs},
   {"FBfcc", test_building_fbfccs},
   {"FBPfcc", test_building_fbpfccs},
   {"Bicc", test_building_biccs},
   {"BPcc", test_building_bpccs},
   {"call", test_building_calls},
   {"cas", test_building_cas},
   {"divide", test_building_divide},
   {"done", test_building_done},
   {"retry", test_building_retry},
   {"fadd", test_building_fadd},
   {"fsub", test_building_fsub},
   {"fcmp", test_building_fcmp},
   {"float2int", test_building_float2int},
   {"float2float", test_building_float2float},
   {"int2float", test_building_int2float},
   {"fmov", test_building_fmov},
   {"fneg", test_building_fneg},
   {"fabs", test_building_fabs},
   {"fmul", test_building_fmul},
   {"fdiv", test_building_fdiv},
   {"fsqrt", test_building_fsqrt},
   {"flush", test_building_flush},
   {"flushw", test_building_flushw},
   {"jmpl", test_building_jmpl},
   {"ldf", test_building_load_float},
   {"ldfsr", test_building_load_floatstatereg},
   {"ldfa", test_building_load_float_altspace},
   {"ld", test_building_load_int},
   {"lda", test_building_load_int_altSpace},
   {"ldstub", test_building_ldstub},
   {"ldstuba", test_building_ldstub_altspace},
   {"and", test_building_and},
   {"andcc", test_building_andcc},
   {"andn", test_building_andn},
   {"andncc", test_building_andncc},
   {"or", test_building_or},
   {"orcc", test_building_orcc},
   {"orn", test_building_orn},
   {"orncc", test_building_orncc},
   {"xor", test_building_xor},
   {"xorcc", test_building_xorcc},
   {"xnor", test_building_xnor},
   {"xnorcc", test_building_xnorcc},
   {"membar", test_building_membar},
   {"fmovcc", test_building_fmovcc},
   {"fmovr", test_building_fmovr},
   {"movcc", test_building_movcc},
   {"movr", test_building_movr},
   {"mulx", test_building_mulx},
   {"sdivx", test_building_sdivx},
   {"udivx", test_building_udivx},
   {"umul", test_building_umul},
   {"smul", test_building_smul},
   {"umulcc", test_building_umulcc},
   {"smulcc", test_building_smulcc},
   {"mulscc", test_building_mulscc},
   {"nop", test_building_nop},
   {"popc", test_building_popc},
   {"prefetch", test_building_prefetch},
   {"prefetcha", test_building_prefetch_altSpace},
   {"rdpr", test_building_rdpr},
   {"rd", test_building_rd},
   {"v9return", test_building_v9return},
   {"save", test_building_save},
   {"restore", test_building_restore},
   {"saved", test_building_saved},
   {"restored", test_building_restored},
   {"sethi", test_building_sethi},
   {"shift", test_building_shift},
   {"sir", test_building_sir},
   {"stbar", test_building_stbar},
   {"stf", test_building_store_float},
   {"stfsr", test_building_store_float_statusreg},
   {"stfa", test_building_store_float_altSpace},
   {"st", test_building_store_int},
   {"sta", test_building_store_int_altSpace},
   {"sub", test_building_subtract},
   {"swap", test_building_swap},
   {"swapa", test_building_swapa},
   {"tadd", test_building_tagged_add},
   {"tsub", test_building_tagged_sub},
   {"trap", test_building_trap},
   {"wrpr", test_building_wrpr},
   {"wr", test_building_wr},
   // And now some synthetic instructions:
   {"cmp", test_building_cmp},
   {"jmp", test_building_jmp},
   {"call_addr", test_building_call_addr},
   {"iprefetch", test_building_iprefetch},
   {"tst", test_building_tst},
   {"ret", test_building_ret},
   {"retl", test_building_retl},
   {"restore_trivial", test_building_restore_trivial},
   {"save_trivial", test_building_save_trivial},
   {"signx", test_building_signx},
   {"not", test_building_not},
   {"neg", test_building_neg},
   {"inc", test_building_inc},
   {"inccc", test_building_inccc},
   {"dec", test_building_dec},
   {"deccc", test_building_deccc},
   {"btst", test_building_btst},
   {"bset", test_building_bset},
   {"bclr", test_building_bclr},
   {"btog", test_building_btog},
   {"clr_reg", test_building_clr_reg},
   {"clr_byte", test_building_clr_byte},
   {"clr_half", test_building_clr_half},
   {"clr_word", test_building_clr_word},
   {"clr_x", test_building_clr_x},
   {"clruw", test_building_clruw}, // clear upper word
   {"mov", test_building_mov}, // clear upper word
   {NULL, NULL}
};

void do_all_tests() {
   for (const one_insn_test *testptr = allTests; testptr->testName != NULL;
        ++testptr) {
      cout << testptr->testName << "..." << flush;
      
      testptr->test_func();

      cout << "done" << endl;
   }
}

bool do_1_interactive_test(const char *testname) {
   for (const one_insn_test *testptr = allTests; testptr->testName != NULL;
        ++testptr) {
      if (0 != strcmp(testptr->testName, testname))
         continue;

      cout << testptr->testName << "..." << flush;
      
      testptr->test_func();

      cout << "done" << endl;

      return true;
   }
   
   return false;
}
