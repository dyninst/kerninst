// kernelResolver.h
// Ariel Tamches

// A "resolverOracle" suitable for the template argument to method
// "obtainFullyResolvedCode()" of class relocatableCode (see relocatableCode.h).
// Use for downloading code into the kernel (not into kerninstd)

#ifndef _KERNEL_RESOLVER_H_
#define _KERNEL_RESOLVER_H_

#include "common/h/String.h"
#include <pair.h>
#include "util/h/Dictionary.h"
#include "util/h/kdrvtypes.h"

class kernelResolver {
 private:
   dictionary_hash<pdstring, kptr_t> knownDownloadedCodeAddrs;
      // This is very useful when outlining multiple functions at a time, and when
      // one outlined piece of code has to make an interprocedural branch to, or a call
      // to, other outlined code.  Such branches/calls are emitted via symname, not
      // not destAddr (since destAddr isn't known until the last moment), and thus
      // require some smarts from us to resolve.

   // private to ensure they're not called:
   kernelResolver(const kernelResolver &);
   kernelResolver &operator=(const kernelResolver &);
   
 public:
   kernelResolver(const dictionary_hash<pdstring, kptr_t> &iknownDownloadedCodeAddrs) :
      knownDownloadedCodeAddrs(iknownDownloadedCodeAddrs) {
   }

   // first: true iff found; second: the address (undefined if not found)
   pair<bool, kptr_t> obtainFunctionEntryAddr(const pdstring &fnName) const;
      // "fnName" is of the form "modName/fnName", right?
   pair<bool, kptr_t> obtainImmAddr(const pdstring &immName) const;
};

#endif
