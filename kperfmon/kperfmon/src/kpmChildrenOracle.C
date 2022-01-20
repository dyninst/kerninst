// kpmChildrenOracle.C

#include "kpmChildrenOracle.h"
#include "abstractions.h"
#include "util/h/out_streams.h"
#include "util/h/Dictionary.h"
#include "util/h/rope-utils.h" // num2string()
#include "kapi.h"

#include <algorithm> // stl's sort()

// Maybe we should take over the management of these 2 vrbles:
extern unsigned codeWhereAxisId; // fixed in tclCommands.C: should be 1
extern dictionary_hash<unsigned, pdstring> xmoduleWhereAxisId2ModuleName;
extern dictionary_hash<pdstring, unsigned> moduleName2ModuleWhereAxisId;
extern dictionary_hash<unsigned, kptr_t> xfnWhereAxisId2FnAddr;
extern dictionary_hash<kptr_t, unsigned> fnAddr2FnWhereAxisId;
extern dictionary_hash<unsigned, unsigned> bbWhereAxisId2BBNum;

extern abstractions<kpmChildrenOracle> *global_abstractions;
extern kapi_manager kmgr;

bool kpmChildrenOracle::hasChildren(unsigned id) {
   return numChildren(id) > 0;
}

unsigned kpmChildrenOracle::numChildren(unsigned id) {
   // translate id to module num and fn-w/in-module num

   // First, we'll need the full path of this res id; e.g., translate
   // "trap" into "/WholeProg/Code/unix/trap"

   abstractions<kpmChildrenOracle> &theAbstractions = *global_abstractions;
   
   pdvector<unsigned> res_components = theAbstractions.expandResourceIdInCurrAbs(id);

   switch (res_components.size()) {
      case 0:
         assert(false);
         break;
      case 1:
         // # children of root: 1 ("/code")
         // Does "root" have any children?  Sure, it's '/code'.
         return 1;
      case 2:
         // # children of /code: num-modules
         return kmgr.getNumModules();
      case 3: {
         // # of children of /code/some-module: num-fns in that module
         const unsigned mod_resid = res_components[2];
         const pdstring &modname = xmoduleWhereAxisId2ModuleName.get(mod_resid);

	 kapi_module kMod;
	 if (kmgr.findModule(modname.c_str(), &kMod) != 0) {
	     assert(false);
	 }

         const unsigned result = kMod.getNumFunctions();
            // before we return 'result', we must assert no duplicate names,
            // otherwise the poor whereAxis, having weeded out duplicate names,
            // will keep calling addChildren() forever, wondering why the num children
            // never gets up to the oracle's numChildren().  In other words, since
            // addChildrenNow() below in this file weeds out duplicates, we must
            // assert that there are none here.

         // (presently no need to explicitly weed out duplicate names, since
         // class loadedModule has an internal dictionary of fn names to fn addrs
         // that inherently doesn't allow duplicates)

         return result;
      }
      case 4: {
         // # of children of /code/some-module/some-fn: num-bbs in that fn
         const unsigned mod_fnid = res_components[3];
         const kptr_t fnAddr = xfnWhereAxisId2FnAddr.get(mod_fnid);
	 kapi_function kFunc;
	 if (kmgr.findFunctionByAddress(fnAddr, &kFunc) != 0) {
	     assert(false);
	 }
	 return kFunc.getNumBasicBlocks();
      }
      case 5:
         // # of children of /code/some-module/some-fn/some-bb: zero
         return 0;

      default:
         assert(false);
         break;
   }
   
   assert(false);
}

class mn_comparitor {
 public:
   bool operator()(const std::pair<pdstring,pdstring> &us,
                   const std::pair<pdstring,pdstring> &them) const {
      // we sort by module name (excluding description), so sort by .first
      return us.first < them.first;
   }
};

class fnNamesCmp {
 public:
   bool operator()(const std::pair<kptr_t,pdstring> &us,
                   const std::pair<kptr_t,pdstring> &them) const {
      // NEW: we sort by name, not address
      return us.second < them.second;
   }
};

bool kpmChildrenOracle::
addChildrenNow(where4tree<whereAxisRootNode, kpmChildrenOracle> *parent,
               const where4TreeConstants &) {
   // Avoids adding duplicates.  Returns true iff anything changed.

   const unsigned parentId = parent->getNodeData().getUniqueId();
   extern unsigned nextUnusedUniqueWhereAxisId; // misc.C (for now)
   
   abstractions<kpmChildrenOracle> &theAbstractions = *global_abstractions;
   pdvector<unsigned> res_components = theAbstractions.expandResourceIdInCurrAbs(parentId);

   // To avoid n^2 behavior, we first check to see which items would not be
   // duplicates (accumulating itemsToAdd[])...then we add those items without
   // checking for duplicates.
   pdvector< std::pair<pdstring, unsigned> > itemsToAdd;
      // .first: new item's name
      // .second: parent's unique id
         
   switch (res_components.size()) {
      case 0:
         assert(false);
         break;
      case 1: {
         // They're asking us to add the children of 'root'.  It's simply "/code".
         const pdstring childName("Code");
         
         if (!theAbstractions.wouldAddingItemInCurrentBeADuplicate(childName,
                                                                   parentId)) {
            const unsigned whatTheChildIdWillBe =
               nextUnusedUniqueWhereAxisId + itemsToAdd.size();
            codeWhereAxisId = whatTheChildIdWillBe;
            dout << "WARNING: Setting codeWhereAxisId to " << codeWhereAxisId
                 << " as a child of Id " << parentId << endl;

            itemsToAdd += std::make_pair(childName, parentId);
         }

         break;
      }
      case 2: {
         // They're asking us to add the children of 'root/code'.  In other words,
         // add all of the modules.  We have a recent change: instead of trying
         // to put the modules up sorted by address, we now sort them by name.

         pdvector< std::pair<pdstring, pdstring> > theModuleNames;
         // .first: module->getName() [short]
         // .second: what shows up in where axis (includes description, fluff, etc.)
         
	 kapi_vector<kapi_module> allModules;
	 if (kmgr.getAllModules(&allModules) != 0) {
	     assert(false);
	 }
	 const unsigned max_len = 255;
	 char buffer[max_len];
	 kapi_vector<kapi_module>::const_iterator imod = allModules.begin();
	 for (; imod != allModules.end(); imod++) {
	     const kapi_module &mod = *imod;
	     if (mod.getName(buffer, max_len) < 0) {
		 assert(false && "Module name is too long");
	     }
	     extern pdstring module2ItsWhereAxisString(const kapi_module &mod);
	     pdstring name = pdstring(buffer);
	     pdstring fullName = module2ItsWhereAxisString(mod);
	     theModuleNames += std::make_pair(name, fullName);
	 }

         std::sort(theModuleNames.begin(), theModuleNames.end(), mn_comparitor());

         for (pdvector< std::pair<pdstring,pdstring> >::const_iterator iter=theModuleNames.begin();
              iter != theModuleNames.end(); ++iter) {
            const std::pair<pdstring,pdstring> &names = *iter;
            const pdstring &plainModName = names.first;
            const pdstring &prettyModName = names.second;
            
            if (!theAbstractions.wouldAddingItemInCurrentBeADuplicate(prettyModName,
                                                                      parentId)) {
               const unsigned whatTheChildIdWillBe = nextUnusedUniqueWhereAxisId + itemsToAdd.size();
               
               xmoduleWhereAxisId2ModuleName.set(whatTheChildIdWillBe, plainModName);

//                 cout << "Adding " << plainModName
//                      << " to moduleName2ModuleWhereAxisId" << endl;
               
               moduleName2ModuleWhereAxisId.set(plainModName, whatTheChildIdWillBe);
               
               itemsToAdd += std::make_pair(prettyModName, parentId);
            }
         }

         break;
      }
      case 3: {
         // They're asking us to add the children of 'root/code/some-module'.
         // In other words, all of its fns.  NEW feature: we sort them by name,
         // not address.

         const unsigned mod_resid = res_components[2];
         const pdstring &modname = xmoduleWhereAxisId2ModuleName.get(mod_resid);
         
	 kapi_module kMod;
	 if (kmgr.findModule(modname.c_str(), &kMod) < 0) {
	     assert(false && "Module not found");
	 }

         pdvector< std::pair<kptr_t,pdstring> > names;
            // .first: fn's entry addr
            // .second: pretty name (includes address, etc.)  We sort by this

	 kapi_vector<kapi_function> allFunctions;
	 if (kMod.getAllFunctions(&allFunctions) < 0) {
	     assert(false);
	 }
	 const unsigned max_len = 255;
	 char buffer[max_len];
	 kapi_vector<kapi_function>::const_iterator fn_iter =
	     allFunctions.begin();
	 for (; fn_iter != allFunctions.end(); fn_iter++) {
	     const kapi_function &fn = *fn_iter;
	     if (fn.getName(buffer, max_len) < 0) {
		 assert(false && "Function name is too long");
	     }
	     extern pdstring fn2ItsWhereAxisString(const kapi_function &);
	     names += std::make_pair(fn.getEntryAddr(),
				     fn2ItsWhereAxisString(fn));
         }

         std::sort(names.begin(), names.end(), fnNamesCmp());
         
         for (pdvector< std::pair<kptr_t,pdstring> >::const_iterator iter = names.begin();
              iter != names.end(); ++iter) {
            const kptr_t fnEntryAddr = iter->first;
            const pdstring &prettyName = iter->second;

            if (!theAbstractions.wouldAddingItemInCurrentBeADuplicate(prettyName,
                                                                      parentId)) {
               const unsigned whatTheChildIdWillBe =
                  nextUnusedUniqueWhereAxisId + itemsToAdd.size();
               xfnWhereAxisId2FnAddr.set(whatTheChildIdWillBe, fnEntryAddr);
               fnAddr2FnWhereAxisId.set(fnEntryAddr, whatTheChildIdWillBe);

               itemsToAdd += std::make_pair(prettyName, parentId);
            }
         }
         
         break;
      }
      case 4: {
         // They're asking us to add the children of 'root/code/some-mod/some-fn'.
         // In other words, its basic blocks.
         const unsigned mod_fnid = res_components[3];
         const kptr_t fnAddr = xfnWhereAxisId2FnAddr.get(mod_fnid);
	 
	 kapi_function kFunc;
	 if (kmgr.findFunctionByAddress(fnAddr, &kFunc) < 0) {
	     assert(false && "Function not found");
	 }
	 kapi_vector<kapi_basic_block> allBasicBlocks;
	 if (kFunc.getAllBasicBlocks(&allBasicBlocks) < 0) {
	     assert(false);
	 }
	 kapi_vector<kapi_basic_block>::const_iterator bbiter =
	     allBasicBlocks.begin();
	 unsigned bbnum = 0;
	 for (; bbiter != allBasicBlocks.end(); bbiter++, bbnum++) {
            const kptr_t startAddr = bbiter->getStartAddr();
            
            pdstring name = addr2hex(startAddr);

            if (!theAbstractions.wouldAddingItemInCurrentBeADuplicate(name, parentId)) {
               const unsigned whatTheChildIdWillBe = nextUnusedUniqueWhereAxisId + itemsToAdd.size();
               bbWhereAxisId2BBNum.set(whatTheChildIdWillBe, bbnum);

               itemsToAdd += std::make_pair(name, parentId);
            }
         }

         break;
      }
      case 5:
         // They're asking us to add the children of
         // 'root/code/some-mod/some-fn/some-bb'.  There are none; we should never
         // have gotten this far!
         assert(false);

      default:
         assert(false);
         break;
   }

   for (pdvector< std::pair<pdstring, unsigned> >::const_iterator iter=itemsToAdd.begin();
        iter != itemsToAdd.end(); ++iter) {
      const unsigned childId = nextUnusedUniqueWhereAxisId++;

      theAbstractions.addItemInCurrent(iter->first, iter->second,
                                       childId,
                                       false,
                                       false,
                                       false, // guaranteed not to already exist
                                       true // placeholder
                                       );
         // don't rethink graphics now, don't resort now, definitely not duplicate
   }

   const bool addedAnything = (itemsToAdd.size() > 0);
   
   if (addedAnything)
      theAbstractions.doneAddingInCurrent(true);
         // true --> resort now.

   return addedAnything;
}
