// fnAndBlockId.h

#ifndef _FN_AND_BLOCK_ID_H_
#define _FN_AND_BLOCK_ID_H_

#include "funkshun.h"

struct XDR;

struct fnAndBlockId {
   typedef function_t::bbid_t bbid_t;
   
   const function_t *fn; // a ptr, not a ref, so we can have a NULL sentinel
   bbid_t bbid; // of the above fn

   fnAndBlockId(const function_t &ifn, bbid_t ibbid) :
      fn(&ifn), bbid(ibbid) {
   }
   fnAndBlockId(XDR *);

   bool operator==(const fnAndBlockId &other) const {
      return fn == other.fn && bbid == other.bbid;
   }
   bool send(XDR *) const;
};

#endif
