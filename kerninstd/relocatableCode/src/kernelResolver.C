// kernelResolver.C

#include "kernelResolver.h"
#include "moduleMgr.h"
#include "RTSymtab.h"

extern moduleMgr *global_moduleMgr;

pair<bool, kptr_t>
kernelResolver::obtainFunctionEntryAddr(const pdstring &fnName) const {
   // First try to find it within "knownDownloadedCodeAddrs[]"
   kptr_t result = 0;
   if (knownDownloadedCodeAddrs.find(fnName, result))
      return make_pair(true, result);

   const StaticString fnNameAsStaticString(fnName.c_str());
      // as long as fnName.c_str() doesn't "move" for the remainder of this fn
      // (which I think is reasonable), then we're OK here.

   const moduleMgr &theModuleMgr = *global_moduleMgr;

   pdvector<kptr_t> fnEntryAddrs; // vec of kptr_t
   
   moduleMgr::const_iterator moditer = theModuleMgr.begin();
   moduleMgr::const_iterator modfinish = theModuleMgr.end();
   for (; moditer != modfinish; ++moditer) {
      const loadedModule *mod = moditer.currval();

      pdvector<kptr_t> fnEntryAddrsThisMod = // a vec of kptr_t
         mod->find_fn_by_name(fnNameAsStaticString);

      if (fnEntryAddrsThisMod.size() > 1) {
         cout << "Sorry, there are " << fnEntryAddrs.size()
              << " functions matching the name \""
              << fnNameAsStaticString.c_str() << "\""
              << " in module \"" << mod->getName() << "\"" << endl;
         assert(false && "ambiguous");
      }
      else if (fnEntryAddrsThisMod.size() > 0)
         fnEntryAddrs += fnEntryAddrsThisMod;
   }

   if (fnEntryAddrs.size() == 0) {
      // Couldn't find in our structures...try /dev/ksyms
      cout << "NOTE: kernelResolver couldn't find fn " << fnName
           << " in moduleMgr; checking /dev/ksyms slowly" << endl;

      pair<bool, kptr_t> result = lookup1NameInKernel(fnName);

      if (result.first)
         cout << "...found " << fnName << " in /dev/ksyms" << endl;
      else
         cout << "...no, didn't find " << fnName << " in /dev/ksyms either" << endl;

      return result;
   }
   else if (fnEntryAddrs.size() == 1)
      return make_pair(true, fnEntryAddrs[0]);
   else {
      cout << "Sorry, there are several possible matches for the fn name \""
           << fnName << "\":" << endl;

      pdvector<kptr_t>::const_iterator iter = fnEntryAddrs.begin();
      pdvector<kptr_t>::const_iterator finish = fnEntryAddrs.end();
      for (; iter != finish; ++iter)
         cout << addr2hex(*iter) << ' ';
      cout << endl;

      assert(false);
   }
}

pair<bool, kptr_t>
kernelResolver::obtainImmAddr(const pdstring &immName) const {
   // imm32's (assumed to be objects) are not stored in our structures...try 
   // /dev/ksyms.

   // First try to find it within "knownDownloadedCodeAddrs[]"
   kptr_t addr = 0;
   if (knownDownloadedCodeAddrs.find(immName, addr))
      return make_pair(true, addr);

   cout << "NOTE: kernelResolver couldn't find immName " << immName
        << " in moduleMgr; checking /dev/ksyms slowly..." << endl;

   pair<bool, kptr_t> result = lookup1NameInKernel(immName);

   if (result.first)
      cout << "...found " << immName << " in /dev/ksyms" << endl;
   else
      cout << "...no, didn't find " << immName << " in /dev/ksyms either" << endl;

   return result;
}

