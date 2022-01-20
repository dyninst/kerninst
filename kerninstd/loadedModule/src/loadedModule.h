// loadedModule.h
// Ariel Tamches
// Manages a single module of functions (for the kernel, we have lots of modules;
// for a 'normal' ELF program file, there will be just one module).  Note that
// the term "module" here is used in the context of the solaris kernel, which considers
// a module the granularity of loadable elements...analagous to a shared library
// for user programs.  Therefore, don't confuse our term "module" with the common
// notion of a module as an individual .c or .C source code file.  If we had the
// ability to determine source code files for all functions (which we can't, because
// ELF doesn't associate a source code file for its global functions), then such info
// would be stored elsewhere, and could complement this class nicely.
//
// Properties of the functions we keep:
// -- You add a function by giving its start address and name via addButDontParse().
//    When done, you call removeAliases() to do exactly that.
//    Then you can fill in the contents by calling parseExisting().
// -- The functions are always sorted, so you can always search for a function
//    by its address.  Note, however, that a function doesn't define its end addr
//    until it's parsed.
// -- You can iterate through the functions (arbitrary ordering, however).

#ifndef _LOADED_MODULE_H_
#define _LOADED_MODULE_H_

#include "funkshun.h"
#include "util/h/StringPool.h"
#include "util/h/Dictionary.h"

struct XDR;

class loadedModule {
 public: // Making these typedef's public seems harmless enough:
   typedef monotone_bitwise_dataflow_fn dataflow_fn;

//     class vec_of_kptr_customalloc {
//      public:
//        static kptr_t *alloc(unsigned num) {
//           // not yet optimized for the common case of 1
//           return (kptr_t*)malloc(sizeof(kptr_t)*num);
//        }
//        static void free(kptr_t *ptr, unsigned /*num*/) {
//           // not yet optimized for the common case of 1
//           ::free(ptr);
//        }
//     };
//   typedef pdvector<kptr_t, vec_of_kptr_customalloc> FnAddrs;

 private:
//   HomogenousHeapUni<function_t,1> *fnHeap;

   pdstring modName;
   pdstring modDescription;

   StringPool theStringPool;

   // NOTE: There's now little or no meaning for sorting functions, since functions
   // can be made up of disjoint pieces.  It only makes sense to sort all of the
   // chunks, which class moduleMgr handles.  So while we still have fns_vec[], it's
   // NEVER sorted any more!
   pdvector<function_t*> fns_vec;
      // yes, we manage allocating & frying the fns (i.e. we do more than
      // just manage ptrs w/in the vector)

   pdvector<const char *> runtimeAddedStringPool;
      // This is a heap of strings, each allocated with new char[len+1].
      // It's meant to be used to hold function names that are added at runtime,
      // and thus don't fit well within the mold of "theStringPool", which isn't
      // easily resized without leading to dangling pointers.  Since functions are
      // not usually added at runtime, this "heap" is usually empty.  Consider
      // downloaded-to-kernel functions, however.

   dictionary_hash<StaticString, kptr_t> name2FuncEntryAddr_whenUnique;
   dictionary_hash<StaticString, pdvector<kptr_t> > name2SeveralFuncEntryAddrs;
   kptr_t TOCPtr; //only useful on power, for now

//     dictionary_hash_multi<StaticString, kptr_t, vec_of_kptr_customalloc> allFuncsByName;
//        // key: name of a function.  The same name can map to multiple addresses; this
//        //    is allowed so long as the name isn't global.
//        //    Note that it's important that the key be "StaticString", rather than
//        //    "string"; I'm sure we're saving quite a bit of memory this way!
//        // value: entry addr of that function.
//        // 
//        // This is a multimap: the same name can map to distinct fns (with distinct
//        // entryAddrs).  This is quite intentional; e.g. strlen() and strtoi() may be
//        // repeated twelve zillion times in a single kernel module, as long as at most
//        // one of them are "global" in scope.
//        // Anyway, the point being: when you search (pass in a StaticString),
//        // you'll get a *range* of matching function entry addrs.  If more than one
//        // entry in that range, then we have an ambiguity that you must deal with.

   void add1NameToAddrMapping(const StaticString &fnName, kptr_t fnEntryAddr);
   void remove1NameFromAddrMapping(const StaticString &fnName, kptr_t fnEntryAddr);

   // making these private ensures they ain't used:
   loadedModule(const loadedModule &);
   loadedModule& operator=(const loadedModule &src);

 public:
   loadedModule(XDR *);
   loadedModule(const pdstring &modName,
                const pdstring &modDescription,
                const char *pool, unsigned long pool_nbytes,
                kptr_t TOCPtr);
                //HomogenousHeapUni<function_t,1> *fnHeap,
                //HomogenousHeapUni<basicblock_t,1> *);
  ~loadedModule();

   void getMemUsage(unsigned &forBasicBlocks,
                    unsigned &forFnOrigCodes,
                    unsigned &forStringPools,
                    unsigned &forLiveRegAnalysisInfo,
                    unsigned &liveRegAnalysisInfo_num1Levels,
                    unsigned &liveRegAnalysisInfo_num2Levels,
                    unsigned &liveRegAnalysisInfo_numOtherLevels,
                    unsigned &forFnStructAndSmallOther
                    ) const;
   
   void fry1fn(function_t *fn);

   const StringPool &getStringPool() const { return theStringPool; }
   const char *addToRuntimeStringPool(const pdstring &);
      // returns the (copied) string that you can assume will remain static
      // from now until the module is fried.  You're welcome.
   
   const pdstring &getName() const { return modName; }
   const pdstring &getDescription() const { return modDescription; }
   
   unsigned numFns() const {
      // before sorting, count could be too high due to aliases not yet removed.
      return fns_vec.size();
   }
   kptr_t getTOCPtr() const { return TOCPtr; }

   function_t *addButDontParse(kptr_t startAddr,
                               const StaticString &fnName);
   function_t *addButDontParse(kptr_t startAddr,
                               const pdvector<StaticString> &fnNames);
   void addAFullyInitializedFn(function_t *);

   void removeAliases();

   template <class calcEffectOfCall>
   dataflow_fn *
   getLiveRegFnBeforeInsnAddr(const function_t *fn,
                              kptr_t iaddr, const calcEffectOfCall &) const;

   template <class calcEffectOfCall>
   dataflow_fn *
   getLiveRegFnAfterInsnAddr(const function_t *fn,
                             kptr_t iaddr, const calcEffectOfCall &) const;
   
   template <class calcEffectOfCall>
   pair<dataflow_fn*, dataflow_fn*>
   getLiveRegFnBeforeAndAfterInsnAddr(const function_t *fn,
                                      kptr_t iaddr,
                                      const calcEffectOfCall &) const;
   

   // Iteration, stl style.  Note that since fns_vec[] is UNSORTED, iteration should
   // be considered essentially "random" ordering.
   typedef pdvector<function_t*>::iterator iterator;
   typedef pdvector<function_t*>::const_iterator const_iterator;

   iterator begin() { return fns_vec.begin(); }
   const_iterator begin() const { return fns_vec.begin(); }
   iterator end() { return fns_vec.end(); }
   const_iterator end() const { return fns_vec.end(); }

   pdvector<kptr_t> find_fn_by_name(const StaticString &fnName) const;
      // has to return a vector since any one module can have several
      // different fns with the same name (e.g. strlen() can be defined as
      // a "static" fn a dozen times within krtld)
   kptr_t find_1fn_by_name(const StaticString &fnName) const;
      // Like the above, but barfs if there are any duplicates.  As such, this
      // version of the routine may return a kptr_t instead of a vector thereto.
      // Returns 0 if not found.
};

// templated methods go in the .h file so the compiler can find & automatically
// instantiate them:

template <class calcEffectOfCall>
loadedModule::dataflow_fn*
loadedModule::getLiveRegFnBeforeInsnAddr(const function_t *fn,
                                         kptr_t insnAddr,
                                         const calcEffectOfCall &c) const {
   return fn->getLiveRegFnBeforeInsnAddr(insnAddr, c, true);
      // true --> if applicable, use already-cached answer (at top of basic block)
}

template <class calcEffectOfCall>
loadedModule::dataflow_fn*
loadedModule::getLiveRegFnAfterInsnAddr(const function_t *fn,
                                        kptr_t insnAddr,
                                        const calcEffectOfCall &c) const {
   return fn->getLiveRegFnAfterInsnAddr(insnAddr, c);
}

template <class calcEffectOfCall>
pair<loadedModule::dataflow_fn*, loadedModule::dataflow_fn*>
loadedModule::getLiveRegFnBeforeAndAfterInsnAddr(const function_t *fn,
                                                 kptr_t insnAddr,
                                                 const calcEffectOfCall &c) const {
   return fn->getLiveRegFnBeforeAndAfterInsnAddr(insnAddr, c);
}

#endif
