// kerninstdResolver.C

#include "kerninstdResolver.h"
#include "RTSymtab.h"

pair<bool, dptr_t>
kerninstdResolver::obtainFunctionEntryAddr(const pdstring &fnName) {
   return lookup1NameInSelf(fnName);
}

pair<bool, dptr_t>
kerninstdResolver::obtainImmAddr(const pdstring &symName) {
   return lookup1NameInSelf(symName);
}

