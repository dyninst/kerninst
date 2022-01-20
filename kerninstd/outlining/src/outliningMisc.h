// outliningMisc.h

#ifndef _OUTLINING_MISC_H_
#define _OUTLINING_MISC_H_

#include "common/h/String.h"
#include "funkshun.h"

class outliningMisc {
 public:
   // The next 3 functions give you the module name, module description, and
   // function name of a function, after it has been outlined (or re-outlined).
   static pdstring getOutlinedFnModName(unsigned groupUniqId);
   static pdstring getOutlinedFnModDescription(unsigned groupUniqId);
   static pdstring getOutlinedFnName(const pdstring &origModName,
                                   const function_t &origFn);

   // The next fn converts from a possibly-outlined function (mod/fn pair) to
   // the entryAddr of its original function.
   static kptr_t getOutlinedFnOriginalFn(const pdstring &modName, const function_t &fn);
   static kptr_t getOutlinedFnOriginalFn(kptr_t fnEntryAddr);
      // converts fnEntryAddr to modName/fn and then calls the first version
      // of getOutlinedFnOriginalFn()

   // The next fn is complex, so pay attention: it uses a modName/fnName pair of
   // some function (the first 2 args) to figure out what group that function
   // belonged to, if any.  With that info in mind, it then returns the name
   // of a similarly-outlined function representing the last 2 args.
   static kptr_t getSameOutlinedLevelFunctionAs(const pdstring &guideModName,
                                                const function_t &guideFn,
                                                kptr_t desiredFnOrigEntryAddr);

   // Convert a function into its outlined group id (UINT_MAX if none)
   static unsigned parseOutlinedLevel(const pdstring &modName, const function_t &fn);
};

#endif
