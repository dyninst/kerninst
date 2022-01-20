// moduleMgr.h
// Ariel Tamches

#ifndef _MODULE_MGR_H_
#define _MODULE_MGR_H_

#include "util/h/mrtime.h"
#include "callGraph.h"
#include "loadedModule.h"
#include "bitwise_dataflow_fn.h"
#include <utility> // stl's pair<t1, t2>
#include <map> // STL's map<key, data, compare, alloc>
#include "LatticeInfo.h"
#include "hiddenFns.h"

class moduleMgr {
   friend class RegAnalysisHandleCall_FirstTime; // allow it access to iterator find()
   friend class RegAnalysisHandleCall_AlreadyKnown;
   friend class RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion;
   
 public: // debatable whether the typedef's should be public, but I can't see the harm
   typedef function_t::fnCode_t fnCode_t;
   typedef monotone_bitwise_dataflow_fn dataflow_fn;
   typedef LatticeInfo lattice_t;

 private:
   //HomogenousHeapUni<function_t,1> fnHeap; // put on hold (easier for kperfmon)
   //HomogenousHeapUni<basicblock_t,1> bbHeap; // unused: fn used pdvector<basicblock_t>

   mrtime_t parseCFGsTotalTime;
   mrtime_t addToFunctionFinderTotalTime;
   mrtime_t addToCallGraphTotalTime;
   bbParsingStats totalBBParsingStats;
   
   dictionary_hash<pdstring, loadedModule*> modulesByName;
   // The modules are stored here.  We used to have a "module_vec" but that
   // turned out to be unnecessary since there's no longer any need to keep all
   // of the modules sorted.

   // Useful to pre-calculate these things:
   lattice_t liveRegAnalysisLattice;
      // optimistic (top of lattice) is set to "stop all regs", except for %g0,
      //    which, being a read-only register, cannot be stopped since it can't be
      //    written to.
      // pessimistic (bottom of lattice) is set to "start all regs".
      //    (actually, this is changing 12/2000...stay tuned for more details)
      // merge operator is set to: "union" (bitwise or)
   
 public:
   typedef dictionary_hash<pdstring, loadedModule*>::const_iterator const_iterator;
   const_iterator begin() const { return modulesByName.begin(); }
   const_iterator end() const { return modulesByName.end(); }

   struct chunkKey {
      kptr_t startAddr;
      unsigned nbytes;
      chunkKey(kptr_t istartAddr, unsigned inbytes) :
         startAddr(istartAddr), nbytes(inbytes) {
      }
   };
   struct chunkData {
      pdstring modName; // thank goodness for reference counted strings
      function_t *fn;
      // not const; some operations (e.g. reg analysis) make changes to the fn
      // (e.g. reg analysis dfs state)
      chunkData(const pdstring &imodName, function_t *ifn) :
         modName(imodName), fn(ifn) {
      }
   };

 private:
   class chunkKeyComparitor {
    public:
      bool operator()(const chunkKey &key1, const chunkKey &key2) const {
         // beware of keys with nbytes==0, which usually come coupled with
         // startAddr==0 also, which can lead to some unsigned underflow if you
         // subtract 1.  That's why we use '<=' instead of '<' here:
         return (key1.startAddr + key1.nbytes <= key2.startAddr);
         // no unsigned underflow possibilities
      }
   };
   typedef std::map<chunkKey, chunkData, chunkKeyComparitor> chunkmap_t;
   chunkmap_t functionFinder;

   callGraph theCallGraph;
   // Whenever a new function is parsed, we update this.
   // Whenever a function is fried, we also update this.

   // --------------------

   chunkmap_t::const_iterator addr2ChunkMapIter(kptr_t addr) const {
      // returns end() if not found of course.
      const chunkKey theKey(addr, 1); // 1 byte
      return functionFinder.find(theKey);
   }
   const chunkData &addr2ModAndFn(kptr_t addr) const {
      chunkmap_t::const_iterator iter = addr2ChunkMapIter(addr);
      assert(iter != functionFinder.end()); // assert found

      return iter->second;
   }
   void add1ChunkToFunctionFinder(const chunkKey &,
                                  const pdstring &modName,
                                  function_t &);
   void addToFunctionFinder(const pdstring &modName, function_t &fn);

   void removeFromFunctionFinder(const function_t &, bool assert_existed);
   void remove1ChunkFromFunctionFinder(const chunkKey &, bool assert_existed);

   void fry(loadedModule &, bool inProcessOfFryingEverything);
   // if inProcessOfFryingEverything is true, then removing entries from the
   // call graph should be handled with care.  In particular, if we blindly
   // remove both the calls made by, and the calls made to, a particular function,
   // that's fine as far as that function goes...but when we later do the same
   // with other functions, calls made to already-removed functions will have
   // already been removed from the call graph, perhaps triggering an assert.
   // So if the 2d flag is true, we only remove calls made *to* this fn.

   // private to ensure it's not used:
   moduleMgr &operator=(const moduleMgr &) const;
   moduleMgr(const moduleMgr &);

 public:
   moduleMgr();
  ~moduleMgr();

   mrtime_t getParseCFGsTotalTime() const {
      // updated on each parseCFG_forFn()
      return parseCFGsTotalTime;
   }
   mrtime_t getAddToFunctionFinderTotalTime() const {
      // updated on each parseCFG_forFn()
      return addToFunctionFinderTotalTime;
   }
   mrtime_t getAddToCallGraphTotalTime() const {
      // updated on each parseCFG_forFn()
      return addToCallGraphTotalTime;
   }
   
   unsigned numModules() const { return modulesByName.size(); }

   // --------------------

   void addEmptyModule(const pdstring &modName,
                       const pdstring &modDescription,
                       const char *pool,
                       unsigned long pool_nbytes,
                       kptr_t TOCptr);
   // does not yet re-sort (would crash since no fns in this module yet
   // so its startAddr is unknown).
   
   void addAllocatedModule(loadedModule *mod);

   void fryEmptyModule(const pdstring &modName);

   // --------------------

   const function_t *addFnToModuleButDontParseYet(const pdstring &modName,
                                                  kptr_t fnStartAddr,
                                                  const pdstring &fnName,
                                                  bool removeAliasesInModule);
   // adds "fnName" to staticStringHeap.  Does not add to functionFinder yet.

   const function_t *addFnToModuleButDontParseYet(const pdstring &modName,
                                                  kptr_t fnStartAddr,
                                                  const StaticString &fnName,
                                                  bool removeAliasesInModule);
   // Does not add to functionFinder yet.
   const function_t *addFnToModuleButDontParseYet(const pdstring &modName,
                                                  kptr_t fnStartAddr,
                                                  const pdvector<StaticString> &fnNames,
                                                  bool removeAliasesInModule);
   // Does not add to functionFinder yet.

   void addAFullyAllocatedFn(const pdstring &modName, function_t *fn);
   // adds to functionFinder.  The module must already exist.

   void removeAliasesForModule(const pdstring &modName);

   // Fries the function starting at fnEntryAddr. If the function happens
   // to be the last one in its module, fries the module as well.
   void fry1fnAndModuleIfNowEmpty(kptr_t fnEntryAddr,
                                  const fnCode_t &currCode);
   // WARNING: don't blindly pass getOrigCode() as currCode; we use the param
   // to see who this function is currently calling, so it MUST accurately reflect
   // the actual contents of memory, such as any replaced calls.

   // Same as above, but does not fry the module
   void fry1fn(loadedModule *mod, function_t *fn, const fnCode_t &currCode);
   
   // --------------------

   void fnHasSeveralAlternateNames(kptr_t fnStartAddr, const pdvector<StaticString> &);

#ifdef _KERNINSTD_
   void parseCFG_forFn(const pdstring &modName,
                       const function_t &,
                       const fnCode_t &,
                       unsigned preFnDataChunkNdxIfAny, // -1U if none
                          // we won't "trim" this chunk
                       dictionary_hash<kptr_t,bool> &interProcJumps,
                       pdvector<hiddenFn> &hiddenFns,
                       bool assertWhenAddingToCallgraph);

   void doNotParseCFG_forFn(const pdstring &modName,
                            const function_t &);
   // call this when function is in skips.txt file; we'll just
   // add to functionFinder as appropriate -- not much else will get done.
#endif
   
   void updateCallGraphForReplaceFunction(kptr_t oldFnStartAddr,
                                          kptr_t newFnStartAddr);

   void updateCallGraphForNewReplaceFunctionDest(kptr_t oldFnStartAddr,
                                                 kptr_t priorReplacementAddr,
                                                 kptr_t newReplacementAddr);

   void updateCallGraphForUnReplaceFunction(kptr_t fnAddr,
                                            kptr_t usedToBeReplacedByThisAddr);

   void updateCallGraphForReplaceCall(kptr_t callSiteAddr,
                                      kptr_t oldfnEntryAddr,
                                      kptr_t newfnEntryAddr);

   void doInterProcRegisterAnalysis1FnOnly_CallsAlreadyKnown_NoBailOnRecursion
   (kptr_t fnStartAddr, bool verbose);
   
   void doInterProcRegisterAnalysis1FnOnly_CallsAlreadyKnown_BailOnRecursion
   (kptr_t fnStartAddr, bool verbose);

   void addFunctionsWithKnownNumLevels(const pdvector<oneFnWithKnownNumLevelsByAddr> &);
      // Used to determine numLevels when returning a conservative result
      // for a given function
   
   // --------------------
   
   bool ensure_start_of_bb(kptr_t destAddr);
   // make sure whatever fn contains 'destAddr' has a basic block starting at that
   // address.  If it doesn't, then split() the appropriate bb to make it so.

   void getMemUsage(unsigned &forBasicBlocks,
                    unsigned &forFnOrigCodes,
                    unsigned &forFnStructAndSmallOther,
                    unsigned &forStringPools,
                    unsigned &forLiveRegAnalysisInfo,
                    unsigned &forLiveRegAnalysisInfo_1heap,
                    unsigned &forLiveRegAnalysisInfo_2heap,
                    unsigned &forLiveRegAnalysisInfo_num1heap,
                    unsigned &forLiveRegAnalysisInfo_num2heap,
                    unsigned &forLiveRegAnalysisInfo_numOtherheap,
                    unsigned &forCallGraph
                    ) const;

   const bbParsingStats &getTotalBBParsingStats() const {
      return totalBBParsingStats;
   }
   
#ifdef _KERNINSTD_
   void doGlobalRegisterAnalysis(bool verbose);
   // depth-first search thru all fns in all modules, calling
   // doGlobalRegisterAnalysis1Fn() for each fn.  Fills in data structures which are
   // currently stored in function_t (live regs at the top of each bb in that fn).

   // These routines can only be called after doGlobalRegisterAnalysis() has been
   // called.
   dataflow_fn*
   getLiveRegFnBeforeInsnAddr(const function_t &fn,
                              kptr_t insnAddr, bool verbose) const;
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.
      // caller be advised: doesn't automatically say that %sp and %fp are made live,
      // even though you should always treat them as live.

   dataflow_fn*
   getLiveRegFnAfterInsnAddr(const function_t &fn,
                             kptr_t insnAddr, bool verbose) const;
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.
      // caller be advised: doesn't automatically say that %sp and %fp are made live,
      // even though you should always treat them as live.

   dataflow_fn*
   getDeadRegFnBeforeInsnAddr(const function_t &fn,
                              kptr_t insnAddr, bool verbose) const;
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.
      // caller be advised: doesn't automatically say that %sp and %fp are made live,
      // even though you should always treat them as live.

   dataflow_fn*
   getDeadRegFnAfterInsnAddr(const function_t &fn,
                             kptr_t insnAddr, bool verbose) const;
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.
      // caller be advised: doesn't automatically say that %sp and %fp are made live,
      // even though you should always treat them as live.

   std::pair<dataflow_fn*, dataflow_fn*>
   getDeadRegFnsBeforeAndAfterInsnAddr(const function_t &fn,
                                       kptr_t insnAddr, bool verbose) const;
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.
      // caller be advised: doesn't automatically say that %sp and %fp are made live,
      // even though you should always treat them as live.

   regset_t* getLiveRegsBeforeInsnAddr(const function_t &fn,
				       kptr_t insnAddr,
				       bool verbose) const {
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.

      // For safety, this routine always returns %fp and %sp as live.

      // the use of regset_t::fullset implies we're making the most
      // conservative assumption about which regs are live.
      regset_t *result;

      try {
         result = getLiveRegFnBeforeInsnAddr(fn, insnAddr, verbose)->apply(regset_t::getFullSet());

         // One should always treat %sp and %fp as live, for safety, right?  %g7 too.
        *result += regset_t::getAlwaysLiveSet();
      }
      catch (monotone_bitwise_dataflow_fn_ll::CannotApply) {
         // conservative: all regs are live
         result = regset_t::getRegSet(regset_t::full);
      }
      catch (monotone_bitwise_dataflow_fn_ll::DisagreeingNumberOfPreSaveFramesInMerge) {
         // conservative: all regs are live
         result = regset_t::getRegSet(regset_t::full);
      }
      catch (...) {
         // conservative: no regs are dead
         result = regset_t::getRegSet(regset_t::empty);
      }

      return result;
   }

   regset_t* getDeadRegsBeforeInsnAddr(const function_t &fn,
                                      kptr_t insnAddr, bool verbose) const {
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.

      // For safety, this routine will never return %fp, %sp, or %g7 as dead.
      regset_t *result;

      try {
         result = getDeadRegFnBeforeInsnAddr(fn, insnAddr, verbose)->apply(regset_t::getEmptySet());
         // the use of regset_t::emptyset implies we're making the most
         // conservative assumption about which regs are dead after the fn
         *result -= regset_t::getAlwaysLiveSet();
      }
      catch (monotone_bitwise_dataflow_fn_ll::CannotApply) {
         // conservative: no regs are dead
         result = regset_t::getRegSet(regset_t::empty);
      }
      catch (monotone_bitwise_dataflow_fn_ll::DisagreeingNumberOfPreSaveFramesInMerge) {
         // conservative: no regs are dead
         result = regset_t::getRegSet(regset_t::empty);
      }
      catch (...) {
         // conservative: no regs are dead
         result = regset_t::getRegSet(regset_t::empty);
      }
      
      return result;
   }

   regset_t* getDeadRegsAfterInsnAddr(const function_t &fn,
				      kptr_t insnAddr, bool verbose) const {
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.

      // For safety, this routine will never return %fp, %sp, or %g7 as dead.
      regset_t *result;
      
      try {
         result = getDeadRegFnAfterInsnAddr(fn, insnAddr, verbose)->apply(regset_t::getEmptySet());
         // the use of sparc_reg_set::emptyset implies we're making the most
         // conservative assumption about which regs are dead after the fn

         // For safety, one should never say that %sp and %fp are dead, right?
         // and %g7 too, right?
         // XXX - The below code needs to be made un-sparc-specific:
         *result -= regset_t::getAlwaysLiveSet();
      }
      catch (monotone_bitwise_dataflow_fn_ll::CannotApply) {
         // conservative: no regs are dead
         result = regset_t::getRegSet(regset_t::empty);
      }
      catch (monotone_bitwise_dataflow_fn_ll::DisagreeingNumberOfPreSaveFramesInMerge) {
         // conservative: no regs are dead
         result = regset_t::getRegSet(regset_t::empty);
      }
      catch (...) {
         // conservative: no regs are dead
         result = regset_t::getRegSet(regset_t::empty);
      }
      
      return result;
   }

   void getDeadRegsBeforeAndAfterInsnAddr(const function_t &fn,
                                          kptr_t insnAddr,
                                          regset_t &before,
                                          regset_t &after, bool verbose) const;
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.
      // For safety, this routine will never return %fp, %sp, or %g7 as dead.

   // -------------------------------------------------------------------------
   //
   // Live/dead registers at a given "point" (you specify preInsn v. preReturn)
   //
   // -------------------------------------------------------------------------

   regset_t* getDeadRegsAtPoint(const function_t &,
				kptr_t addr, bool preReturn) const;
      // fn is passed as a time-saver; caller probably already has it; sending it
      // allows this routine to bypass re-searching for it.
      // Although this routine allocates an LRAndR to help give you an answer
      // e.g., if the point is preReturn at a tail call, studies show that the LRAndR
      // doesn't add much overhead; having to get dead regs both before & after
      // the instruction does (about doubling the time required).

#endif // _KERNINSTD_

   loadedModule *find(const pdstring &modName); // hash lookup
   // NOTE: I don't feel comfortable making this one public!  fn_misc.C
   // needs it, for now at least.

 public:
   const loadedModule *find(const pdstring &modName) const; // hash lookup

   // --------------------

 private:
   function_t &findFn(kptr_t addr, bool startOnly);
   // assert fails if either module or function are not found
   function_t *findFnOrNULL(kptr_t addr, bool startOnly);
   // like above but returns NULL instead of assert failing if not found
 public:
   const function_t &findFn(kptr_t addr, bool startOnly) const;
      // assert fails if are not found
   const function_t *findFnOrNULL(kptr_t addr, bool startOnly) const;
      // like above but returns NULL instead of assert failing if not found

   const function_t &findNextHighestFn(kptr_t addr) const;
      // assert fails if not found.  Find the function, then looks for the
      // one with the next highest starting address.  Useful to determine
      // the gap between functions.
   const function_t *findNextHighestFnOrNULL(kptr_t addr) const;
      // like above but returns NULL instead of assert fail if not found

   std::pair<pdstring, function_t*> findModAndFn(kptr_t addr, bool startOnly);
   std::pair<pdstring, const function_t*> findModAndFn(kptr_t addr, bool startOnly) const;
   // above 2 routine assert fail if not found
   std::pair<pdstring, function_t*> findModAndFnOrNULL(kptr_t addr, bool startOnly);
   std::pair<pdstring, const function_t*> findModAndFnOrNULL(kptr_t addr,
                                                      bool startOnly) const;
   // above 2 routines return the pair "", NULL if not found

   // --------------------

   const pdvector<kptr_t> &getCallSitesTo(kptr_t fnStartAddr,
					bool &isResultSorted) const;

   void change1CallSite(kptr_t callSiteAddr,
                        kptr_t oldCalleeAddr,
                        kptr_t newCalleeAddr);

   sampleStatistics getStatsOfCalledFunctions(unsigned &numZeros) const;
};

#endif
