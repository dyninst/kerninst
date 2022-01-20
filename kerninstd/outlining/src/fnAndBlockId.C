// fnAndBlockId.C

#include "fnAndBlockId.h"
#include "util/h/xdr_send_recv.h"
#include "moduleMgr.h"

fnAndBlockId::fnAndBlockId(XDR *xdr) {
   kptr_t fnEntryAddr;
   if (!P_xdr_recv(xdr, fnEntryAddr) || !P_xdr_recv(xdr, this->bbid))
      throw xdr_recv_fail();
   
   extern moduleMgr *global_moduleMgr;
   const pair<pdstring, const function_t *> modAndFn =
      global_moduleMgr->findModAndFn(fnEntryAddr, true); // true --> startOnly

   this->fn = modAndFn.second;
}

bool fnAndBlockId::send(XDR *xdr) const {
   return P_xdr_send(xdr, fn->getEntryAddr() && P_xdr_send(xdr, bbid));
}

