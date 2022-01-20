// jumpTableCodeObject.C

#include "jumpTableCodeObject.h"
#include "util/h/xdr_send_recv.h" // the common xdr send & recv routines

jumpTableCodeObject::jumpTableCodeObject(XDR *xdr) {
   if (!P_xdr_recv(xdr, jumpTableDataStartAddr))
      throw xdr_recv_fail();
}

bool jumpTableCodeObject::send(bool, XDR *xdr) const {
   return P_xdr_send(xdr, jumpTableDataStartAddr);
}
