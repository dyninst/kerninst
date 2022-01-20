// outliningMisc.C

#include "outliningMisc.h"
#include "util/h/rope-utils.h" // num2string()
#include "moduleMgr.h"

static const pdstring groupPrefix("group");

pdstring outliningMisc::getOutlinedFnModName(unsigned groupUniqId) {
   // a static method
   return groupPrefix + num2string(groupUniqId);
}
pdstring outliningMisc::getOutlinedFnModDescription(unsigned groupUniqId) {
   // a static method
   static const pdstring result("outlined code");

   return result + num2string(groupUniqId);
}
pdstring outliningMisc::getOutlinedFnName(const pdstring &origModName,
                                        const function_t &origFn) {
   // a static method
   
   // If origModName shows that the function was already outlined (has a prefix
   // "group") then we simply return origFn's name, which is already
   // (origMod + ':' + origFnName), as desired.

   if (origModName.prefixed_by(groupPrefix))
      return origFn.getPrimaryName().c_str();
   else {
      const pdstring result = origModName + ":" + origFn.getPrimaryName().c_str();
         // we use a colon instead of '/' since '/'
         // may be assumed to be a module separator
         // in some circumstances...

      return result;
   }
}

kptr_t outliningMisc::getOutlinedFnOriginalFn(const pdstring &modName,
                                              const function_t &fn) {
   if (modName.prefixed_by(groupPrefix)) {
      // This fn is within a group.  Calculate its original name
      
      const char *currFnName = fn.getPrimaryName().c_str();
      
      const char *str = strchr(currFnName, ':');
      assert(str);
      
      const unsigned len = str - currFnName;
      assert(len > 0);
      
      const pdstring resultModName(currFnName, len);
      const pdstring resultFnName(str+1);

      extern moduleMgr *global_moduleMgr;
      const moduleMgr &theModuleMgr = *global_moduleMgr;
      
      const loadedModule *mod = theModuleMgr.find(resultModName);
      assert(mod);
      
      kptr_t resultFnEntryAddr =
         mod->find_1fn_by_name(StaticString(resultFnName.c_str()));
         // NOTE: chokes if there are duplicates due to aliasing

      return resultFnEntryAddr;
   }
   else
      // no changes are needed
      return fn.getEntryAddr();
}

kptr_t outliningMisc::getOutlinedFnOriginalFn(kptr_t fnEntryAddr) {
   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;
   
   pair<pdstring, const function_t*> modAndFn =
      theModuleMgr.findModAndFn(fnEntryAddr, true);
      // barfs if not found

   return getOutlinedFnOriginalFn(modAndFn.first, *modAndFn.second);
}

unsigned outliningMisc::
parseOutlinedLevel(const pdstring &modName,
                   const function_t &) {
   if (modName.prefixed_by(groupPrefix)) {
      // yes, we're in a group
      const char *str = modName.c_str() + groupPrefix.length();
      const unsigned result = atoi(str);
      
      assert(modName == groupPrefix + num2string(result));
      
      return result;
   }
   else
      // not outlined
      return UINT_MAX;
}

kptr_t outliningMisc::
getSameOutlinedLevelFunctionAs(const pdstring &guideModName,
                               const function_t &guideFn,
                               kptr_t desiredFnOrigEntryAddr
                               ) {
   // using guideModName/guideFn as a guide to determine outlined 'depth', if any,
   // this function returns a same-outlined-depth version of the function
   // represented by desiredFnModName/desiredFnFnName.

   const unsigned groupUniqueId = parseOutlinedLevel(guideModName, guideFn);
   if (groupUniqueId == UINT_MAX)
      // the guide fn wasn't outlined to begin with, so return
      // a plain version of desiredFnOrigModName/desiredFnOrigFnName
      return desiredFnOrigEntryAddr;
   else {
      extern moduleMgr *global_moduleMgr;
      const moduleMgr &theModuleMgr = *global_moduleMgr;
      
      pair<pdstring, const function_t*> origModAndFn =
         theModuleMgr.findModAndFn(desiredFnOrigEntryAddr, true);
         
      const pdstring resultModName = getOutlinedFnModName(groupUniqueId);

      const pdstring desiredFnOrigModName = origModAndFn.first;
      const function_t &desiredFnOrigFn = *origModAndFn.second;

      const pdstring resultFnName = getOutlinedFnName(desiredFnOrigModName,
                                                    desiredFnOrigFn);

      const loadedModule *mod = theModuleMgr.find(resultModName);
      assert(mod);
      
      return mod->find_1fn_by_name(StaticString(resultFnName.c_str()));
   }
}
