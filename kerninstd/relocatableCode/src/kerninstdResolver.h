// kerninstdResolver.h
// Ariel Tamches

// A "resolverOracle" suitable for the template argument to method
// "obtainFullyResolvedCode()" of class relocatableCode (see relocatableCode.h).
// Use for downloading code into kerninstd (not into the kernel)

#ifndef _KERNINSTD_RESOLVER_H_
#define _KERNINSTD_RESOLVER_H_

#include "common/h/String.h"

#include <pair.h>
#include "util/h/kdrvtypes.h"

class kerninstdResolver {
 public:

   // .first: true iff found
   // .second: the address (undefined if .first is false)
   static pair<bool, dptr_t> obtainFunctionEntryAddr(const pdstring &fnName);
   static pair<bool, dptr_t> obtainImmAddr(const pdstring &immName);
};

#endif
