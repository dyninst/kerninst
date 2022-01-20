// fn_misc.C
// The findModuleAndReturnStringPoolReference and
// findModuleAndAddToRuntimeStringPool (kludgy)
// routines.

#include "loadedModule.h"
#include "fn_misc.h"

const StringPool &
findModuleAndReturnStringPoolReference() {
   extern loadedModule *presentlyReceivingModule;
   assert(presentlyReceivingModule != NULL);
   
   // don't call when receiving a function in isolation!
   return presentlyReceivingModule->getStringPool();
}

const char *
findModuleAndAddToRuntimeStringPool(const pdstring &s) {
   // This version must always be called when receiving a particular module.
   extern loadedModule *presentlyReceivingModule;
   assert(presentlyReceivingModule != NULL);
   return presentlyReceivingModule->addToRuntimeStringPool(s);
}

const char *
findModuleAndAddToRuntimeStringPool(const pdstring &modName, const pdstring &fnName) {
   extern loadedModule *presentlyReceivingModule;
   extern loadedModule *mostRecentlyReceivedModule;

   if (presentlyReceivingModule != NULL) {
      // we're currently receiving a module, so finding its string pool is trivial
      assert(presentlyReceivingModule->getName() == modName);
      return findModuleAndAddToRuntimeStringPool(fnName); // the above version
   }
   else if (mostRecentlyReceivedModule != NULL) {
      // we're not presently receiving a module; go with
      // mostRecentlyReceivedModule.  As a check, make sure the module name matches

      assert(mostRecentlyReceivedModule->getName() == modName);
      return mostRecentlyReceivedModule->addToRuntimeStringPool(fnName);
   }
   else
      assert(false);
   return false; //placate compiler
}
