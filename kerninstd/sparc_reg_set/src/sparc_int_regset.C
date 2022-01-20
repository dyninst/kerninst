// sparc_int_regset.C

#include "sparc_int_regset.h"
#include "util/h/popc.h"
#include "util/h/ffs.h"
#include <stdlib.h> // random()
#include "util/h/xdr_send_recv.h"

sparc_int_regset::Raw sparc_int_regset::raw;
sparc_int_regset::Random sparc_int_regset::random;
sparc_int_regset::Empty sparc_int_regset::empty;
sparc_int_regset::Full sparc_int_regset::full;
sparc_int_regset::Save sparc_int_regset::save;
sparc_int_regset::SingleIntReg sparc_int_regset::singleIntReg;

sparc_int_regset::sparc_int_regset(XDR *xdr) {
   if (!P_xdr_recv(xdr, int_bits))
      throw xdr_recv_fail();
}

bool sparc_int_regset::send(XDR *xdr) const {
   return P_xdr_send(xdr, int_bits);
}

sparc_reg sparc_int_regset::expand1() const {
   assert(!isempty());
   unsigned first = ari_ffs(int_bits);
      // undefined if int_bits==0, but we've taken care of that.

   return sparc_reg(sparc_reg::rawIntReg, first);
}

unsigned sparc_int_regset::count() const {
   return ari_popc(int_bits);
}
