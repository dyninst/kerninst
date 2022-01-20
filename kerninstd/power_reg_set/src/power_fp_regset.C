// power_fp_regset.C


#include "power_fp_regset.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include <stdlib.h> // random()
#include "util/h/xdr_send_recv.h"

power_fp_regset::Empty power_fp_regset::empty;
power_fp_regset::Full power_fp_regset::full;
power_fp_regset::Raw power_fp_regset::raw;
power_fp_regset::Random power_fp_regset::random;
power_fp_regset::SingleFloatReg power_fp_regset::singleFloatReg;

power_fp_regset::power_fp_regset(XDR *xdr) {
   if (!P_xdr_recv(xdr, float_bits))
      throw xdr_recv_fail();
}

bool power_fp_regset::send(XDR *xdr) const {
   return P_xdr_send(xdr, float_bits);
}

power_fp_regset::power_fp_regset(SingleFloatReg, const power_reg &r) {
 
   const unsigned floatNum = r.getFloatNum(); 

   float_bits = 1ULL << floatNum;

   assert(ari_popc(float_bits) == 1);
}
power_fp_regset::power_fp_regset(SingleFloatReg, unsigned floatNum) {
   assert(floatNum < 32);
   float_bits = 1ULL << floatNum;
   assert(ari_popc(float_bits) == 1);
}




power_reg power_fp_regset::expand1() const {
   assert(!isempty());
   unsigned first = ari_ffs(float_bits);
      // undefined if float_bits==0, but we've taken care of that.

   return power_reg(power_reg::f, first);
}

unsigned power_fp_regset::count() const {
   return ari_popc(float_bits);
}
