// parseNewFunction.h
// not to be confused with "downloadToKernel.h", which supplies a relocatableCode,
// without knowing where it'll reside.  Here, we know where things reside.

#ifndef _PARSE_NEW_FUNCTION_H_
#define _PARSE_NEW_FUNCTION_H_

#include "funkshun.h"

struct XDR;

typedef pair<kptr_t, unsigned> chunkRange;
   // same as typedef'd in kerninstdClient.I file;
   // we'd like to just #include "kerninstdClient.xdr.h"
   // but that doesn't work, leading to a recursive #include

struct parseNewFunction {
   typedef function_t::fnCode_t fnCode_t;
   
   pdstring modName;
   pdstring modDescriptionIfNew;
   pdstring fnName;
   pdvector<chunkRange> chunkAddrs;
   unsigned entry_chunk_ndx;
   unsigned chunk_ndx_containing_data_if_any; // -1U if none

   parseNewFunction(XDR *);
   parseNewFunction(const pdstring &imodName, const pdstring &imodDescriptionIfNew,
                    const pdstring &ifnName,
                    const pdvector<chunkRange> &ichunkAddrs,
                    unsigned ientry_chunk_ndx,
                    unsigned ichunk_ndx_containing_data_if_any) :
      modName(imodName), modDescriptionIfNew(imodDescriptionIfNew),
      fnName(ifnName),
      chunkAddrs(ichunkAddrs),
      entry_chunk_ndx(ientry_chunk_ndx),
      chunk_ndx_containing_data_if_any(ichunk_ndx_containing_data_if_any) {
   }
   
   bool send(XDR *) const;
};
typedef pdvector<parseNewFunction> parseNewFunctions; // plural

bool P_xdr_send(XDR *, const parseNewFunction &);
bool P_xdr_recv(XDR *, parseNewFunction &);

#endif
