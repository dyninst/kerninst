// parseNewFunction.C

#include "parseNewFunction.h"
#include "util/h/xdr_send_recv.h" // xdr for string

template <class T>
static void destruct1(const T &t) {
   t.~T();
}

parseNewFunction::parseNewFunction(XDR *xdr) :
   modName(read1_string(xdr)),
   modDescriptionIfNew(read1_string(xdr)),
   fnName(read1_string(xdr)) {

   // still have to read in: chunkAddrs[] and entry_chunk_ndx

   destruct1(chunkAddrs);
   if (!P_xdr_recv(xdr, chunkAddrs) ||
       !P_xdr_recv(xdr, entry_chunk_ndx) ||
       !P_xdr_recv(xdr, chunk_ndx_containing_data_if_any))
      throw xdr_recv_fail();
}

bool parseNewFunction::send(XDR *xdr) const {
   return (P_xdr_send(xdr, modName) &&
           P_xdr_send(xdr, modDescriptionIfNew) &&
           P_xdr_send(xdr, fnName) &&
           P_xdr_send(xdr, chunkAddrs) &&
           P_xdr_send(xdr, entry_chunk_ndx) &&
           P_xdr_send(xdr, chunk_ndx_containing_data_if_any));
}

bool P_xdr_send(XDR *xdr, const parseNewFunction &pnf) {
   return pnf.send(xdr);
}

bool P_xdr_recv(XDR *xdr, parseNewFunction &pnf) {
   try {
      new((void*)&pnf)parseNewFunction(xdr);
      return true;
   }
   catch (xdr_recv_fail) {
      return false;
   }
}
