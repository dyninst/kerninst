// downloadToKernel.C

#include "downloadToKernel.h"
#include "util/h/xdr_send_recv.h" // xdr ops for string

downloadToKernelFn::downloadToKernelFn(XDR *xdr) :
   modName(read1_string(xdr)),
   modDescriptionIfNew(read1_string(xdr)),
   fnName(read1_string(xdr)),
   theCode(relocatableCode_t::getRelocatableCode(xdr)),
   entry_chunk_ndx(read1_unsigned(xdr)),
   chunkNdxContainingDataIfAny(read1_unsigned(xdr)),
   parseAndAddToModule(read1_bool(xdr))
{
}

bool downloadToKernelFn::send(XDR *xdr) const {
   return (P_xdr_send(xdr, modName) &&
           P_xdr_send(xdr, modDescriptionIfNew) &&
           P_xdr_send(xdr, fnName) &&
           theCode->send(xdr) &&
           P_xdr_send(xdr, entry_chunk_ndx) &&
           P_xdr_send(xdr, chunkNdxContainingDataIfAny) &&
           P_xdr_send(xdr, parseAndAddToModule));
}

// ----------------------------------------------------------------------

bool P_xdr_send(XDR *xdr, const downloadToKernelFn &f) {
   return f.send(xdr);
}

bool P_xdr_recv(XDR *xdr, downloadToKernelFn &f) {
   try {
      new((void*)&f)downloadToKernelFn(xdr);
      return true;
   }
   catch (xdr_recv_fail) {
      return false;
   }
}
