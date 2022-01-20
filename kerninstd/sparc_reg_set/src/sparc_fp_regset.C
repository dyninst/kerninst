// sparc_fp_regset.C
// There are 64 single-precision regs, 32 double-precision regs, and 16 quad-precision
// regs.  Saying that there are 64 single-precision regs is kinda controversial,
// because there are no single-precision instructions which read or write just
// one register numbered 32-63.  But that's not to say that those regs don't
// exist; they do exist!  It's just that they can only be modified indirectly
// by double or quad instructions, which modify several registers at a time.

#include "sparc_fp_regset.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include <stdlib.h> // random()
#include "util/h/xdr_send_recv.h"

sparc_fp_regset::Empty sparc_fp_regset::empty;
sparc_fp_regset::Full sparc_fp_regset::full;
sparc_fp_regset::Raw sparc_fp_regset::raw;
sparc_fp_regset::Random sparc_fp_regset::random;
sparc_fp_regset::SingleFloatReg sparc_fp_regset::singleFloatReg;

sparc_fp_regset::sparc_fp_regset(XDR *xdr) {
   if (!P_xdr_recv(xdr, float_bits))
      throw xdr_recv_fail();
}

bool sparc_fp_regset::send(XDR *xdr) const {
   return P_xdr_send(xdr, float_bits);
}

sparc_fp_regset::sparc_fp_regset(SingleFloatReg, sparc_reg r) {
   // single-precision
   const unsigned floatNum = r.getFloatNum(); // 0 to 63
   // reminder: we're not going to assert that a single-precision register
   // num is from 0 to 31; single-precision regs 32 to 63 *do* exists; the
   // fact that it's a little clumsy to read them (except from dbl or quad
   // precision instructions) is not reason enough to get all dogmatic about it.

   float_bits = 1ULL << floatNum;

   assert(ari_popc(float_bits) == 1);
}

sparc_fp_regset::sparc_fp_regset(SingleFloatReg, unsigned precision, sparc_reg r) {
   assert(r.isFloatReg());
   const unsigned floatNum = r.getFloatNum();
      // from 0 to 63
      
   float_bits = 0; // for now...
      
   assert(floatNum < 64);
   assert(precision == 1 ||
          precision == 2 && floatNum % 2 == 0 ||
          precision == 4 && floatNum % 4 == 0);
   
   for (unsigned lcv=0; lcv < precision; ++lcv) {
      float_bits |= (1ULL << (floatNum + lcv));
   }
   assert(ari_popc(float_bits) == precision);
}

sparc_reg sparc_fp_regset::expand1Reg() const {
   // If single precision, then just 1 bit will be set in float_bits.
   // If dbl precision, then 2 consecutive even-aligned bits will be set.
   // If quad precision, then 4 consecutive quad-aligned bits will be set.
   // In any event, we just want to return the *first* bit set as the result.
   // For example, if it's a quad-precision register encompassing
   // %f4 thru %f7, then yes, we actually want to return only %f4.
   
   unsigned first_bit_set = ari_ffs(float_bits);
   assert(first_bit_set < 64);

   // Now for some assertions
   unsigned precision = ari_popc(float_bits);
   switch (precision) {
      case 1:
         assert(existsSinglePrecision(first_bit_set));
         break;
      case 2:
         assert(first_bit_set % 2 == 0);
         assert(existsSinglePrecision(first_bit_set));
         assert(existsSinglePrecision(first_bit_set+1));
         break;
      case 4:
         assert(first_bit_set % 4 == 0);
         assert(existsSinglePrecision(first_bit_set));
         assert(existsSinglePrecision(first_bit_set+1));
         assert(existsSinglePrecision(first_bit_set+2));
         assert(existsSinglePrecision(first_bit_set+3));
         break;
      default:
         assert(false);
   }

   return sparc_reg(sparc_reg::f, first_bit_set);
}

unsigned sparc_fp_regset::countAsSinglePrecision() const {
   return ari_popc(float_bits);
}
