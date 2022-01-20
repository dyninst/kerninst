// main.C
// sparc_reg test program
// Ariel Tamches

#include <iostream.h>
#include "sparc_reg.h"
#include "common/h/String.h"
#include "util/h/rope-utils.h"

void test_int() {
   for (unsigned num=0; num < 32; ++num) {
      sparc_reg r(sparc_reg::rawIntReg, num);
      assert(r.getIntNum() == num);
      assert(r.isIntReg());
      assert(!r.isFloatReg());
      assert(!r.isIntCondCode());
      assert(!r.isIcc());
      assert(!r.isXcc());
      assert(!r.isFloatCondCode());
      assert(!r.isFcc0());
      assert(!r.isFcc1());
      assert(!r.isFcc2());
      assert(!r.isFcc3());
      assert(!r.isPC());
      assert(!r.isGSR());
      assert(!r.isASI());
      assert(!r.isPIL());

      unsigned offset;
      if (num < 8) {
         assert(r.is_gReg(offset));
         assert(offset == num);
         assert(r == sparc_reg(sparc_reg::global, offset));
         assert(r.regLocationAfterASave() == r);
         assert(r.regLocationAfterARestore() == r);
      }
      else
         assert(!r.is_gReg(offset));

      if (num >= 8 && num < 16) {
         // %o reg
         assert(r.is_oReg(offset));
         assert(offset + 8 == num);
         assert(r == sparc_reg(sparc_reg::out, offset));
         assert(r.regLocationAfterASave() == sparc_reg(sparc_reg::in, offset));
         try {
            (void)r.regLocationAfterARestore();
            assert(false);
         }
         catch(sparc_reg::UnreachableAfterRestore) {
         }
      }
      else
         assert(!r.is_oReg(offset));

      if (num >= 16 && num < 24) { 
         assert(r.is_lReg(offset));
         assert(offset + 16 == num);
         assert(r == sparc_reg(sparc_reg::local, offset));
         try {
            (void)r.regLocationAfterASave();
            assert(false);
         }
         catch(sparc_reg::UnreachableAfterSave) {
         }
         try {
            (void)r.regLocationAfterARestore();
            assert(false);
         }
         catch(sparc_reg::UnreachableAfterRestore) {
         }
      }
      else
         assert(!r.is_lReg(offset));

      if (num >= 24) {
         // %i reg
         assert(r.is_iReg(offset));
         assert(offset + 24 == num);
         assert(r == sparc_reg(sparc_reg::in, offset));
         try {
            (void)r.regLocationAfterASave();
            assert(false);
         }
         catch(sparc_reg::UnreachableAfterSave) {
         }
         assert(r.regLocationAfterARestore() == sparc_reg(sparc_reg::out, offset));
      }
      else
         assert(!r.is_iReg(offset));
   }

   assert(sparc_reg::g0.disass() == pdstring("%g0"));
   assert(sparc_reg::g1.disass() == pdstring("%g1"));
   assert(sparc_reg::g2.disass() == pdstring("%g2"));
   assert(sparc_reg::g3.disass() == pdstring("%g3"));
   assert(sparc_reg::g4.disass() == pdstring("%g4"));
   assert(sparc_reg::g5.disass() == pdstring("%g5"));
   assert(sparc_reg::g6.disass() == pdstring("%g6"));
   assert(sparc_reg::g7.disass() == pdstring("%g7"));
   
   assert(sparc_reg::o0.disass() == pdstring("%o0"));
   assert(sparc_reg::o1.disass() == pdstring("%o1"));
   assert(sparc_reg::o2.disass() == pdstring("%o2"));
   assert(sparc_reg::o3.disass() == pdstring("%o3"));
   assert(sparc_reg::o4.disass() == pdstring("%o4"));
   assert(sparc_reg::o5.disass() == pdstring("%o5"));
   assert(sparc_reg::o6.disass() == pdstring("%sp"));
   assert(sparc_reg::o7.disass() == pdstring("%o7"));
   
   assert(sparc_reg::l0.disass() == pdstring("%l0"));
   assert(sparc_reg::l1.disass() == pdstring("%l1"));
   assert(sparc_reg::l2.disass() == pdstring("%l2"));
   assert(sparc_reg::l3.disass() == pdstring("%l3"));
   assert(sparc_reg::l4.disass() == pdstring("%l4"));
   assert(sparc_reg::l5.disass() == pdstring("%l5"));
   assert(sparc_reg::l6.disass() == pdstring("%l6"));
   assert(sparc_reg::l7.disass() == pdstring("%l7"));
   
   assert(sparc_reg::i0.disass() == pdstring("%i0"));
   assert(sparc_reg::i1.disass() == pdstring("%i1"));
   assert(sparc_reg::i2.disass() == pdstring("%i2"));
   assert(sparc_reg::i3.disass() == pdstring("%i3"));
   assert(sparc_reg::i4.disass() == pdstring("%i4"));
   assert(sparc_reg::i5.disass() == pdstring("%i5"));
   assert(sparc_reg::i6.disass() == pdstring("%fp"));
   assert(sparc_reg::i7.disass() == pdstring("%i7"));
}

void test_fp() {
   // There are 64 single-precision fp regs (yes, 64, even though the last 32 of them)
   // can only be touched by dbl and quad precision instructions.
   // There are 32 double-precision fp regs (0-63 step 2)
   // There are 16 quad-precision fp regs (0-63 step 4)

   // Single-precision test (unfortunately, in this test program, we can't
   // really do much of a torture test to see that single v. double v. quad
   // properties are maintained)
   for (unsigned num=0; num < 64; ++num) {
      sparc_reg r(sparc_reg::f, num);
      
      sparc_reg r_copy(r);
      assert(r_copy == r);
      
      sparc_reg r_copy2 = r;
      assert(r_copy2 == r);
      
      assert(!r.isIntReg());

      unsigned offset;
      assert(!r.is_gReg(offset));
      assert(!r.is_oReg(offset));
      assert(!r.is_lReg(offset));
      assert(!r.is_iReg(offset));

      assert(r.isFloatReg());
      
      assert(!r.isIntCondCode());
      assert(!r.isIcc());
      assert(!r.isXcc());
      assert(!r.isFloatCondCode());
      assert(!r.isFcc0());
      assert(!r.isFcc1());
      assert(!r.isFcc2());
      assert(!r.isFcc3());
      assert(!r.isPC());
      assert(!r.isGSR());

      assert(num == r.getFloatNum());
      assert(num == r_copy.getFloatNum());
      assert(num == r_copy2.getFloatNum());

      pdstring disass_cmp = pdstring("%f") + num2string(num);
      assert(r.disass() == disass_cmp);
   }

   // Double-precision test
   for (unsigned num=0; num < 64; num+=2) {
      sparc_reg r(sparc_reg::f, num);
      
      sparc_reg r_copy(r);
      assert(r_copy == r);
      
      sparc_reg r_copy2 = r;
      assert(r_copy2 == r);
      
      assert(!r.isIntReg());

      unsigned offset;
      assert(!r.is_gReg(offset));
      assert(!r.is_oReg(offset));
      assert(!r.is_lReg(offset));
      assert(!r.is_iReg(offset));

      assert(r.isFloatReg());
      
      assert(!r.isIntCondCode());
      assert(!r.isIcc());
      assert(!r.isXcc());
      assert(!r.isFloatCondCode());
      assert(!r.isFcc0());
      assert(!r.isFcc1());
      assert(!r.isFcc2());
      assert(!r.isFcc3());
      assert(!r.isPC());
      assert(!r.isGSR());

      assert(num == r.getFloatNum());
      assert(num == r_copy.getFloatNum());
      assert(num == r_copy2.getFloatNum());

      pdstring disass_cmp = pdstring("%f") + num2string(num);
      assert(r.disass() == disass_cmp);
   }

   // Quad-precision test
   for (unsigned num=0; num < 64; num+=4) {
      sparc_reg r(sparc_reg::f, num);
      
      sparc_reg r_copy(r);
      assert(r_copy == r);
      
      sparc_reg r_copy2 = r;
      assert(r_copy2 == r);
      
      assert(!r.isIntReg());

      unsigned offset;
      assert(!r.is_gReg(offset));
      assert(!r.is_oReg(offset));
      assert(!r.is_lReg(offset));
      assert(!r.is_iReg(offset));

      assert(r.isFloatReg());
      
      assert(!r.isIntCondCode());
      assert(!r.isIcc());
      assert(!r.isXcc());
      assert(!r.isFloatCondCode());
      assert(!r.isFcc0());
      assert(!r.isFcc1());
      assert(!r.isFcc2());
      assert(!r.isFcc3());
      assert(!r.isPC());
      assert(!r.isGSR());

      assert(num == r.getFloatNum());
      assert(num == r_copy.getFloatNum());
      assert(num == r_copy2.getFloatNum());

      pdstring disass_cmp = pdstring("%f") + num2string(num);
      assert(r.disass() == disass_cmp);
   }
}

static void test_misc_generic(sparc_reg r) { // some generic reg
   assert(!r.isIntReg());
   unsigned num;
   assert(!r.is_gReg(num));
   assert(!r.is_oReg(num));
   assert(!r.is_lReg(num));
   assert(!r.is_iReg(num));

   assert(!r.isFloatReg());
   
   assert(r.isIcc() || r.isXcc() || r.isFloatCondCode() || r.isPC() || r.isGSR() ||
          r.isASI() || r.isPIL());
   if (r.isFloatCondCode())
      assert(r.isFcc0() || r.isFcc1() || r.isFcc2() || r.isFcc3());
   else
      assert(!r.isFcc0() && !r.isFcc1() && !r.isFcc2() && !r.isFcc3());
}

static void test_misc() {
   test_misc_generic(sparc_reg::reg_icc);
   test_misc_generic(sparc_reg::reg_xcc);
   test_misc_generic(sparc_reg::reg_fcc0);
   test_misc_generic(sparc_reg::reg_fcc1);
   test_misc_generic(sparc_reg::reg_fcc2);
   test_misc_generic(sparc_reg::reg_fcc3);
   test_misc_generic(sparc_reg::reg_pc);
   test_misc_generic(sparc_reg::reg_gsr);
   test_misc_generic(sparc_reg::reg_asi);
   test_misc_generic(sparc_reg::reg_pil);

   assert(sparc_reg::reg_icc.isIcc());
   assert(sparc_reg::reg_xcc.isXcc());
   assert(sparc_reg::reg_fcc0.isFloatCondCode());
   assert(sparc_reg::reg_fcc0.isFcc0());
   assert(sparc_reg::reg_fcc1.isFloatCondCode());
   assert(sparc_reg::reg_fcc1.isFcc1());
   assert(sparc_reg::reg_fcc2.isFloatCondCode());
   assert(sparc_reg::reg_fcc2.isFcc2());
   assert(sparc_reg::reg_fcc3.isFloatCondCode());
   assert(sparc_reg::reg_fcc3.isFcc3());
   assert(sparc_reg::reg_pc.isPC());
   assert(sparc_reg::reg_gsr.isGSR());
   assert(sparc_reg::reg_asi.isASI());
   assert(sparc_reg::reg_pil.isPIL());

   assert(sparc_reg::reg_icc.disass() == pdstring("%icc"));
   assert(sparc_reg::reg_xcc.disass() == pdstring("%xcc"));
   assert(sparc_reg::reg_fcc0.disass() == pdstring("%fcc0"));
   assert(sparc_reg::reg_fcc1.disass() == pdstring("%fcc1"));
   assert(sparc_reg::reg_fcc2.disass() == pdstring("%fcc2"));
   assert(sparc_reg::reg_fcc3.disass() == pdstring("%fcc3"));
   assert(sparc_reg::reg_pc.disass() == pdstring("%pc"));
   assert(sparc_reg::reg_gsr.disass() == pdstring("%gsr"));
   assert(sparc_reg::reg_asi.disass() == pdstring("%asi"));
   assert(sparc_reg::reg_pil.disass() == pdstring("%pil"));
}

int main(int argc, char **argv) {
   test_int();
   test_fp();
   test_misc();
   
   cout << "success" << endl;
   
   return 0;
}
