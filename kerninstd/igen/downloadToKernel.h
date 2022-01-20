// downloadToKernel.h

#ifndef _DOWNLOAD_TO_KERNEL_H_
#define _DOWNLOAD_TO_KERNEL_H_

#include "common/h/String.h"
#include "relocatableCode.h"
struct XDR;

struct downloadToKernelFn {
   pdstring modName;
   pdstring modDescriptionIfNew;
   pdstring fnName;
   const relocatableCode_t *theCode;
   unsigned entry_chunk_ndx;
   unsigned chunkNdxContainingDataIfAny;
      // -1U if none.  Unused if 'parseAndAddToModule' is false
   bool parseAndAddToModule;

   downloadToKernelFn(XDR *);
   downloadToKernelFn(const pdstring &imodName, const pdstring &imodDescriptionIfNew,
                      const pdstring &ifnName, const relocatableCode_t *iCode,
                      unsigned ientry_chunk_ndx,
                      unsigned ichunkNdxContainingDataIfAny,
                      bool iparseAndAddToModule) :
      modName(imodName), modDescriptionIfNew(imodDescriptionIfNew),
      fnName(ifnName), theCode(iCode), entry_chunk_ndx(ientry_chunk_ndx),
      chunkNdxContainingDataIfAny(ichunkNdxContainingDataIfAny),
      parseAndAddToModule(iparseAndAddToModule) {
   }
   bool send(XDR *) const;
   unsigned numChunks() const {
      return theCode->numChunks();
   }
   unsigned chunkNumBytes(unsigned ndx) const {
      return theCode->chunkNumBytes(ndx);
   }
   unsigned totalNumBytes() const {
      unsigned result = 0;
      unsigned nchunks = theCode->numChunks();
      for (unsigned c=0; c < nchunks; ++c) {
         result += theCode->chunkNumBytes(c);
      }
      return result;
   }
};

typedef pdvector<downloadToKernelFn> downloadToKernelFns;

// ----------------------------------------------------------------------

typedef pdvector< std::pair<unsigned,unsigned> > emitOrdering;
  // .first: fn ndx   .second: chunk ndx w/in that fn

bool P_xdr_send(XDR *xdr, const downloadToKernelFn &f);
bool P_xdr_recv(XDR *xdr, downloadToKernelFn &f);

#endif
