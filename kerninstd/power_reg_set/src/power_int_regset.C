// power_int_regset.C

//#define rs6000_ibm_aix4_1
#include <rpc/xdr.h>
#include "power_int_regset.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include <stdlib.h> // random()
#include "util/h/xdr_send_recv.h"

power_int_regset::Raw power_int_regset::raw;
power_int_regset::Random power_int_regset::random;
power_int_regset::Empty power_int_regset::empty;
power_int_regset::Full power_int_regset::full;

power_int_regset::SingleIntReg power_int_regset::singleIntReg;


power_int_regset::power_int_regset(XDR *xdr) {
   if (!P_xdr_recv(xdr, int_bits))
      throw xdr_recv_fail();
}

bool power_int_regset::send(XDR *xdr) const {
   return P_xdr_send(xdr, int_bits);
} 

power_reg power_int_regset::expand1() const {
   assert(!isempty());
   unsigned first = ari_ffs(int_bits);
      // undefined if int_bits==0, but we've taken care of that.

   return power_reg(power_reg::rawIntReg, first);
}

unsigned power_int_regset::count() const {
   return ari_popc(int_bits);
}
