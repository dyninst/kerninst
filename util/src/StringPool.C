// StringPool.C

#include "util/h/StringPool.h"
#include "pdutil/h/rpcUtil.h"

struct XDR;

StringPool::StringPool(XDR *xdr) {
   // note: sending/receiving num_bytes appears to be redundant; xdr_bytes appears to
   // send that already...

   if (!P_xdr_recv(xdr, num_bytes))
      throw xdr_recv_fail();

   pool = new char[num_bytes];
   assert(pool);

   unsigned actual_num_bytes = -1U;
   if (!xdr_bytes(xdr, &pool, &actual_num_bytes, num_bytes))
      throw xdr_recv_fail();
   assert(actual_num_bytes == num_bytes);
}
