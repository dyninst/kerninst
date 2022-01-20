// moduleMgr.C

#include "moduleMgr.h"
#include "util/h/Dictionary.h"
#include "util/h/hashFns.h"
#include "util/h/mrtime.h"
#include "util/h/out_streams.h"
#include "regAnalysisOracles.h"

#ifdef _KERNINSTD_

#include "launchSite.h"

#include "regAnalysisOracles.h"
#include "interProcRegAnalysis.h" // doInterProcRegisterAnalysis1Fn()

extern bool verbose_callGraphStats;

#else // _KPERFMON

bool verbose_callGraphStats = false;

#endif // _KERNINSTD_

#ifdef sparc_sun_solaris2_7
#include "sparc_bitwise_dataflow_fn.h"
#endif

moduleMgr::moduleMgr() :
   //fnHeap(512),
   //bbHeap(2048),
   parseCFGsTotalTime(0),
   addToFunctionFinderTotalTime(0),
   addToCallGraphTotalTime(0),
   modulesByName(stringHash),
   liveRegAnalysisLattice(dataflow_fn::mergeopBitwiseOr,
                          &(dataflow_fn::getDataflowFn(dataflow_fn::stopall)->passSet2(regset_t::getRegsThatIgnoreWrites())),
                             // optimistic dataflow value (top of lattice).  We're
                             // going to set it to "stop-all-regs", with one exception:
                             // %g0 cannot be stopped since it can't be written to.
                             // First we make a copy of "stopAllRegs", necessary since
                             // pass() modifies its "this" argument; hence, the explicit
                             // ctor call.
                          dataflow_fn::getDataflowFn(dataflow_fn::startall)
                             // pessimistic dataflow value (bottom of lattice)
                          ),
   theCallGraph(*this)
{
}

void moduleMgr::fry(loadedModule &mod, bool inProcessOfFryingEverything) {
   // call delete on &mod, but first, remove entries from theCallGraph, since dtor
   // will assert that it is empty.

   // if inProcessOfFryingEverything is true, then removing entries from the
   // call graph should be handled with care.  In particular, if we blindly
   // remove both the calls made by, and the calls made to, a particular function,
   // that's fine as far as that function goes...but when we later do the same
   // with other functions, calls made to already-removed functions will have
   // already been removed from the call graph, perhaps triggering an assert.
   // So if the 2d flag is true, we only remove calls made *to* this fn.

   loadedModule::iterator fniter = mod.begin();
   loadedModule::iterator fnfinish = mod.end();
   for (; fniter != fnfinish; ++fniter) {
      function_t *fn = *fniter;

      // remove from 'functionFinder'
      removeFromFunctionFinder(*fn, true); // true --> assert existed

      if (inProcessOfFryingEverything)
         theCallGraph.removeAllCallsToAFunction(fn->getEntryAddr());
      else {
         pdvector< std::pair<kptr_t, kptr_t> > regularCalls;
         pdvector< std::pair<kptr_t, kptr_t> > interProcBranches;
         pdvector<kptr_t> ignore_unAnalyzableCalls;
         fn->getCallsMadeBy_AsOrigParsed(regularCalls,
                                         true, interProcBranches,
                                         false, ignore_unAnalyzableCalls);
            // true --> include interproc branches
         
         theCallGraph.removeAFunction(fn->getEntryAddr(),
                                      regularCalls, interProcBranches,
                                      true // with beefy assertion checks
                                      );
         }

      bool ignore_issorted;
      assert(theCallGraph.getCallsMadeToAFn(fn->getEntryAddr(),
                                            ignore_issorted).size() == 0);
   }

   fniter = mod.begin();
   for (; fniter != fnfinish; ++fniter) {
      const function_t *fn = *fniter;
      bool ignore_issorted;
      assert(theCallGraph.getCallsMadeToAFn(fn->getEntryAddr(),
                                            ignore_issorted).size() == 0);
   }
   
   delete &mod; // frees memory for the individual functions
}

moduleMgr::~moduleMgr() {
   // loop thru modules; frying them all
   dictionary_hash<pdstring, loadedModule*>::const_iterator moditer =
      modulesByName.begin();
   dictionary_hash<pdstring, loadedModule*>::const_iterator modfinish =
      modulesByName.end();
   for (; moditer != modfinish; ++moditer) {
      loadedModule *mod = moditer.currval();
      fry(*mod, true); // true --> in the process of frying everything
   }

   // Check that functionFinder is empty
   assert(functionFinder.empty() && "functionFinder not empty; should be");
   
   // Check that the call graph is empty
   const pdvector< std::pair<kptr_t, kptr_t> > callSitesLeft = 
      theCallGraph.getEverything();

   if ((callSitesLeft.size() > 0) && verbose_callGraphStats) {
      dout << "WARNING:" << endl;
      dout << "The call graph is not empty" << endl;
      dout << "Here is a listing of remaining items, but note that we" << endl
           << "expect calls/interprocedural branches to *unparsed* functions"
           << endl << "to be harmlessly present." << endl;

      pdvector< std::pair<kptr_t, kptr_t> >::const_iterator iter = callSitesLeft.begin();
      pdvector< std::pair<kptr_t, kptr_t> >::const_iterator finish = callSitesLeft.end();
      for (; iter != finish; ++iter) {
         const kptr_t callSiteAddr = iter->first;
         const kptr_t calleeAddr = iter->second;
         
         dout << addr2hex(callSiteAddr) << " --> " << addr2hex(calleeAddr) << endl;
      }
   }
}

// --------------------

void moduleMgr::addEmptyModule(const pdstring &modName,
                               const pdstring &modDescription,
                               const char *pool,
                               unsigned long pool_nbytes,
                               kptr_t TOCptr) {
   loadedModule *mod = new loadedModule(modName, modDescription,
                                        pool, pool_nbytes, TOCptr);
                                        //&fnHeap,
                                        //&bbHeap);
   assert(mod);

   modulesByName.set(modName, mod); // no duplicate module names allowed
}

void moduleMgr::add1ChunkToFunctionFinder(const chunkKey &theKey,
                                          const pdstring &modName, function_t &fn) {
   assert(theKey.nbytes > 0 &&
          "adding 0-length key to functionFinder is not permitted");
      // was that too strong an assert?  Perhaps we should just return harmlessly
      // if that is detected.

   std::pair<const chunkKey, chunkData> theValue(theKey, chunkData(modName, &fn));
   
   const std::pair<chunkmap_t::iterator, bool> result =
      functionFinder.insert(theValue);
   if (!result.second) {
      // already existed!
      cout << "Tried to add to functionFinder ";
      cout << modName << "/" << fn.getPrimaryName().c_str() << endl;
      cout << "addr="
           << addr2hex(theKey.startAddr) << " len=" << theKey.nbytes
           << endl;
      cout << "But the following already existed:" << endl;
      const std::pair<const chunkKey, chunkData> &keyAndData = *(result.first);
         
      cout << keyAndData.second.modName << "/"
           << keyAndData.second.fn->getPrimaryName().c_str();
      cout << " startAddr=" << addr2hex(keyAndData.first.startAddr)
           << " nbytes=" << keyAndData.first.nbytes << endl;

      assert(false);
   }
}

void moduleMgr::remove1ChunkFromFunctionFinder(const chunkKey &theKey,
                                               bool assert_existed) {
   unsigned num_erased = functionFinder.erase(theKey);
   assert(num_erased <= 1);
   
   if (assert_existed)
      assert(num_erased == 1);
}

void moduleMgr::addToFunctionFinder(const pdstring &modName, function_t &fn) {
   // Update "functionFinder" map<>, using fn.origCode(), which'll be properly
   // "trimmed" to reflect what actually got parsed.

   // unparsed functions get only a token 1-byte entry in functionFinder
   if (fn.isUnparsed()) {
      const chunkKey theKey(fn.getEntryAddr(), 1); // 1 byte token entry
      
      add1ChunkToFunctionFinder(theKey, modName, fn);
      return;
   }

   assert(!fn.isUnparsed());
   function_t::fnCode_t::const_iterator chunkiter = fn.getOrigCode().begin();
   function_t::fnCode_t::const_iterator chunkfinish = fn.getOrigCode().end();
   for (; chunkiter != chunkfinish; ++chunkiter) {
      const function_t::fnCode_t::codeChunk &theChunk = *chunkiter;
      const unsigned chunkNumBytes = theChunk.theCode->numInsnBytes();

      if (chunkNumBytes > 0) { // yes, zero-sized chunks do happen in practice (jmptabs)
         const chunkKey theKey(theChunk.startAddr, chunkNumBytes);
         add1ChunkToFunctionFinder(theKey, modName, fn);
      }
   }
}

void moduleMgr::removeFromFunctionFinder(const function_t &fn, bool assert_existed) {
   // If the function was unparsed, then remove a 1-byte entry; otherwise, remove
   // the actual code chunks

   if (fn.isUnparsed()) {
      const chunkKey theKey(fn.getEntryAddr(), 1);
      remove1ChunkFromFunctionFinder(theKey, assert_existed);
      return;
   }

   assert(!fn.isUnparsed());
   function_t::fnCode_t::const_iterator chunkiter = fn.getOrigCode().begin();
   function_t::fnCode_t::const_iterator chunkfinish = fn.getOrigCode().end();
   for (; chunkiter != chunkfinish; ++chunkiter) {
      const function_t::fnCode_t::codeChunk &theChunk = *chunkiter;
      const unsigned chunk_numbytes = theChunk.theCode->numInsnBytes();

      if (chunk_numbytes > 0) { // yes, 0-size chunks occur in practice (jmp tables)
         const chunkKey theKey(theChunk.startAddr, chunk_numbytes);
         remove1ChunkFromFunctionFinder(theKey, assert_existed);
      }
   }
}

void moduleMgr::addAllocatedModule(loadedModule *imod) {
   // Not called by kerninstd, just kperfmon.
   modulesByName.set(imod->getName(), imod);

   const loadedModule &mod = *imod;
   
   // Update functionFinder for each function within the module:
   loadedModule::const_iterator fniter = mod.begin();
   loadedModule::const_iterator fnfinish = mod.end();
   for (; fniter != fnfinish; ++fniter) {
      const function_t *ifn = *fniter;
      function_t *fn = const_cast<function_t*>(ifn);

      addToFunctionFinder(mod.getName(), *fn);

      // Update the call graph:
      pdvector< std::pair<kptr_t, kptr_t> > regularCallsThisFnMakes;
      pdvector< std::pair<kptr_t, kptr_t> > interProcBranchesThisFnMakes;
      pdvector<kptr_t> ignore_unanalyzableCalls;
      fn->getCallsMadeBy_AsOrigParsed(regularCallsThisFnMakes,
                                      true, interProcBranchesThisFnMakes,
                                      false, ignore_unanalyzableCalls);
         // true --> include interproc branches as well

      pdvector< std::pair<kptr_t, kptr_t> >::const_iterator iter =
         regularCallsThisFnMakes.begin();
      pdvector< std::pair<kptr_t, kptr_t> >::const_iterator finish =
         regularCallsThisFnMakes.end();
      for (; iter != finish; ++iter)
         theCallGraph.addCallSite(iter->first, iter->second, false);
            // false --> go easy on the assertions, unfortunately (expensive timewise)

      iter = interProcBranchesThisFnMakes.begin();
      finish = interProcBranchesThisFnMakes.end();
      for (; iter != finish; ++iter)
         theCallGraph.addCallSite(iter->first, iter->second, false);
            // false --> go easy on the assertions, unfortunately (expensive timewise)
   }
}

void moduleMgr::fryEmptyModule(const pdstring &modName) {
   loadedModule *mod = modulesByName.get_and_remove(modName);
   assert(mod->getName() == modName);
   
   delete mod;
   mod = NULL;
}

// --------------------

const function_t *
moduleMgr::addFnToModuleButDontParseYet(const pdstring &modName,
                                        kptr_t fnEntryAddr,
                                        const pdstring &fnName, // static str heap
                                        bool removeAliasesInModule) {
   loadedModule *modptr = find(modName);
   assert(modptr);

   const char *staticFnNameToUse = modptr->addToRuntimeStringPool(fnName);
   function_t *result = 
      modptr->addButDontParse(fnEntryAddr, StaticString(staticFnNameToUse));

   if (removeAliasesInModule)
      modptr->removeAliases();

   return result;
}

const function_t *
moduleMgr::addFnToModuleButDontParseYet(const pdstring &modName,
                                        kptr_t fnEntryAddr,
                                        const StaticString &fnName,
                                        bool removeAliasesInModule) {
   loadedModule *modptr = find(modName);
   assert(modptr);

   function_t *result = modptr->addButDontParse(fnEntryAddr, fnName);

   if (removeAliasesInModule)
      modptr->removeAliases();

   return result;
}

const function_t *
moduleMgr::addFnToModuleButDontParseYet(const pdstring &modName,
                                        kptr_t fnEntryAddr,
                                        const pdvector<StaticString> &fnNames,
                                        bool removeAliasesInModule) {
   loadedModule *modptr = find(modName);
   assert(modptr);

   function_t *result = modptr->addButDontParse(fnEntryAddr, fnNames);

   if (removeAliasesInModule)
      modptr->removeAliases();

   return result;
}

void moduleMgr::addAFullyAllocatedFn(const pdstring &modName, function_t *fn) {
   // The module must already exist
   loadedModule *mod = find(modName);
   assert(mod);

   mod->addAFullyInitializedFn(fn);

   addToFunctionFinder(modName, *fn);
}

void moduleMgr::removeAliasesForModule(const pdstring &modName) {
   return find(modName)->removeAliases();
}

void moduleMgr::fry1fn(loadedModule *mod, function_t *fn,
		       const fnCode_t &currentCode) {
   kptr_t fnEntryAddr = fn->getEntryAddr();
   // Remove the code chunks of this function from "functionFinder".  Remember that
   // if the function was unparsed, there will only have been a 1-byte 'dummy' entry
   // for it in functionFinder.
   removeFromFunctionFinder(*fn, true); // true --> assert that it existed

   // WARNING: do not use "theChunkData" any more; removeFromFunctionFinder() has
   // made it dangling memory!

   // Remove fnEntryAddr from the call graph:
   pdvector< std::pair<kptr_t, kptr_t> > regularCallsMadeByFn;
   pdvector< std::pair<kptr_t, kptr_t> > interProcBranchesMadeByFn;
   pdvector<kptr_t> ignore_unanalyzableCalls;
   fn->getCallsMadeBy_WithSuppliedCode(currentCode,
				       regularCallsMadeByFn,
				       true, interProcBranchesMadeByFn,
				       false, ignore_unanalyzableCalls);
      // true --> include interproc branches as well
      // asOrigParsed()...would NOT be correct if, e.g.,
      // the fn has, since parse time, been changed up
      // due to, say, a replaceFn causing a call site
      // to be rerouted.
   
   theCallGraph.removeAFunction(fnEntryAddr,
                                regularCallsMadeByFn, interProcBranchesMadeByFn,
                                true // with assertion checks
                                );

   mod->fry1fn(fn);
   // don't use "fn" any more; its contents have been delete'd!
}

void moduleMgr::fry1fnAndModuleIfNowEmpty(kptr_t fnEntryAddr,
					  const fnCode_t &currCode)
{
    // currentCode is passed in because getOrigCode() will not reflect
    // changes made to this code since it was parsed!  Changes such as having
    // some its call sites changed up because a callee was replaceFn'd.

    const chunkData &theChunkData = addr2ModAndFn(fnEntryAddr);
    // converts an address to module and fn

    const pdstring modName = theChunkData.modName;
      // we mustn't make this a reference to a pdstring, since "theChunkData"
      // will become dangling after the call to fry1fn(), below!
    loadedModule *mod = find(modName);
    assert(mod);

    assert(theChunkData.fn != NULL);
    function_t *fn = theChunkData.fn;
    // not const since we're about to fry its contents

    fry1fn(mod, fn, currCode);

    if (mod->numFns() == 0) {
	fryEmptyModule(modName);
    }
}
   

// --------------------

void moduleMgr::fnHasSeveralAlternateNames(kptr_t fnEntryAddr,
                                           const pdvector<StaticString> &names) {
   findFn(fnEntryAddr, true).fnHasSeveralAlternateNames(names);
}

#ifdef _KERNINSTD_
void moduleMgr::doNotParseCFG_forFn(const pdstring &modName,
                                    const function_t &ifn) {
   function_t &fn = const_cast<function_t &>(ifn);

   assert(fn.isUnparsed());

   const mrtime_t addToFunctionFinderStartTime = getmrtime();
   
   addToFunctionFinder(modName, fn);
      // when fn is unparsed, addToFunctionFinder adds a "dummy" 1-byte entry
      // to functionFinder.

   addToFunctionFinderTotalTime += getmrtime() - addToFunctionFinderStartTime;
}

void moduleMgr::parseCFG_forFn(const pdstring &modName,
                               const function_t &ifn,
                               const fnCode_t &iCode,
                               unsigned preFnDataChunkNdxIfAny, // -1U if none
                                  // we won't "trim" this chunk
                               dictionary_hash<kptr_t,bool> &interProcJumps,
                               pdvector<hiddenFn> &hiddenFns,
                               bool assertWhenAddingToCallgraph) {
   function_t &fn = const_cast<function_t &>(ifn);

   const mrtime_t thisFnCFGParseStartTime = getmrtime();
   
   try {
      pdvector<unsigned> chunkNdxsNotToTrim;
      if (preFnDataChunkNdxIfAny != -1U)
         chunkNdxsNotToTrim += preFnDataChunkNdxIfAny;

      bbParsingStats bbParsingStatsThisFn; // all stats zero

      fn.parseCFG(iCode, chunkNdxsNotToTrim, interProcJumps, hiddenFns,
                  bbParsingStatsThisFn);
      
      totalBBParsingStats += bbParsingStatsThisFn;
   }
   catch (...) {
      // Even functions that don't parse must have a pro forma entry within the
      // functionFinder map.  We'll make it a 1-byte length, so we'll only be able
      // to search for this function when given its entry addr.  Should be fine.

      parseCFGsTotalTime += getmrtime() - thisFnCFGParseStartTime;
      
      doNotParseCFG_forFn(modName, fn);

      throw; // re-throw
   }

   parseCFGsTotalTime += getmrtime() - thisFnCFGParseStartTime;

   const mrtime_t thisFnAddToFunctionFinderStartTime = getmrtime();
   addToFunctionFinder(modName, fn);
   addToFunctionFinderTotalTime += getmrtime() - thisFnAddToFunctionFinderStartTime;
   
   // Update the call graph.
   const mrtime_t thisFnCallGraphCreationStartTime = getmrtime();
   
   pdvector< std::pair<kptr_t, kptr_t> > regularCallsThisFnMakes;
   pdvector< std::pair<kptr_t, kptr_t> > interProcBranchesThisFnMakes;
   pdvector<kptr_t> ignore_unanalyzableCalls;
   fn.getCallsMadeBy_AsOrigParsed(regularCallsThisFnMakes,
                                  true, interProcBranchesThisFnMakes,
                                  false, ignore_unanalyzableCalls);
      // true --> include interproc branches as well
      // .first: insn addr   .second: callee

   pdvector< std::pair<kptr_t, kptr_t> >::const_iterator iter =
      regularCallsThisFnMakes.begin();
   pdvector< std::pair<kptr_t, kptr_t> >::const_iterator finish =
      regularCallsThisFnMakes.end();
   for (; iter != finish; ++iter)
      theCallGraph.addCallSite(iter->first, iter->second, assertWhenAddingToCallgraph);

   iter = interProcBranchesThisFnMakes.begin();
   finish = interProcBranchesThisFnMakes.end();
   for (; iter != finish; ++iter)
      theCallGraph.addCallSite(iter->first, iter->second, assertWhenAddingToCallgraph);

   addToCallGraphTotalTime += getmrtime() - thisFnCallGraphCreationStartTime;
}
#endif

// --------------------

void moduleMgr::
updateCallGraphForReplaceFunction(kptr_t oldfnEntryAddr,
                                  kptr_t newfnEntryAddr) {
   theCallGraph.updateCallGraphForReplaceFunction(oldfnEntryAddr, newfnEntryAddr);
   // changes calls to "oldfnEntryAddr" to be calls to "newfnEntryAddr".
}

void moduleMgr::
updateCallGraphForNewReplaceFunctionDest(kptr_t oldfnEntryAddr,
                                         kptr_t priorReplacementAddr,
                                         kptr_t newReplacementAddr) {
   theCallGraph.updateCallGraphForNewReplaceFunctionDest(oldfnEntryAddr,
                                                         priorReplacementAddr,
                                                         newReplacementAddr);
}

void moduleMgr::
updateCallGraphForUnReplaceFunction(kptr_t fnAddr,
                                    kptr_t usedToBeReplacedByThisAddr) {
   theCallGraph.updateCallGraphForUnReplaceFunction(fnAddr,
                                                    usedToBeReplacedByThisAddr);
}

void moduleMgr::
updateCallGraphForReplaceCall(kptr_t callSiteAddr,
                              kptr_t oldfnEntryAddr,
                              kptr_t newfnEntryAddr) {
   theCallGraph.updateCallGraphForReplaceCall(callSiteAddr, 
                                              oldfnEntryAddr, 
                                              newfnEntryAddr);
}

// ------------------------------------------------------------
// --------- find variant: module name to module --------------
// --------------- (quick hash table lookup) ------------------
// ------------------------------------------------------------

loadedModule *moduleMgr::find(const pdstring &modName) {
   // hash lookup followed by binary search
   loadedModule *mod = NULL;
   if (!modulesByName.find(modName, mod))
      return NULL;

   return mod;
}

const loadedModule *moduleMgr::find(const pdstring &modName) const {
   // hash lookup followed by binary search
   loadedModule *mod = NULL;
   if (!modulesByName.find(modName, mod))
      return NULL;
   
   return mod;
}

function_t &
moduleMgr::findFn(kptr_t addr, bool startOnly) {
   // assert fails if either module or function are not found

   const chunkData &theChunkData = addr2ModAndFn(addr);
   function_t *fn = theChunkData.fn;

   if (!fn->isUnparsed() || !startOnly)
      // The check avoids a barf for an unparsed fn
      assert(fn->containsAddr(addr));

   if (startOnly)
      assert(fn->getEntryAddr() == addr);
   
   return *fn;
}

const function_t *
moduleMgr::findNextHighestFnOrNULL(kptr_t addr) const {
   chunkmap_t::const_iterator chunk_iter = addr2ChunkMapIter(addr);
   assert(chunk_iter != functionFinder.end());
   const chunkData &theChunkData = chunk_iter->second;
   const function_t *fn = theChunkData.fn;

   while (true) {
      ++chunk_iter;

      if (chunk_iter == functionFinder.end())
         // couldn't find the next function
         return NULL;
      
      const function_t *nextFn = chunk_iter->second.fn;
      if (nextFn != fn)
         // match!
         return nextFn;
   }
}
const function_t &
moduleMgr::findNextHighestFn(kptr_t addr) const {
   const function_t *fn = findNextHighestFnOrNULL(addr);
   assert(fn != NULL);
   return *fn;
}

const function_t &
moduleMgr::findFn(kptr_t addr, bool startOnly) const {
   // assert fails if either module or function are not found

   const chunkData &theChunkData = addr2ModAndFn(addr);
   const function_t *fn = theChunkData.fn;

   if (startOnly) {
      assert(fn->getEntryAddr() == addr);
   }
   else if (!fn->isUnparsed()) {
       // The check avoids a barf for an unparsed fn
       assert(fn->containsAddr(addr));
   }
   
   return *fn;
}

function_t *
moduleMgr::findFnOrNULL(kptr_t addr, bool startOnly) {
   chunkmap_t::const_iterator chunk_iter = addr2ChunkMapIter(addr);
   if (chunk_iter == functionFinder.end())
      return NULL;

   const chunkData &theChunkData = chunk_iter->second;
   function_t *fn = theChunkData.fn;

   if (startOnly && fn->getEntryAddr() != addr)
      return NULL;
   
   return fn;
}

const function_t *
moduleMgr::findFnOrNULL(kptr_t addr, bool startOnly) const {
   chunkmap_t::const_iterator chunk_iter = addr2ChunkMapIter(addr);
   if (chunk_iter == functionFinder.end())
      return NULL;

   const chunkData &theChunkData = chunk_iter->second;
   function_t *fn = theChunkData.fn;

   if (startOnly && fn->getEntryAddr() != addr)
      return NULL;
   
   return fn;
}

// --------------------

std::pair<pdstring, function_t*>
moduleMgr::findModAndFn(kptr_t addr, bool startOnly) {
   chunkmap_t::const_iterator chunk_iter = addr2ChunkMapIter(addr);
   assert(chunk_iter != functionFinder.end());
   const chunkData &theChunkData = chunk_iter->second;

   const pdstring &modName = theChunkData.modName;
   function_t *fn = theChunkData.fn;
      
   if (startOnly)
      assert(fn->getEntryAddr() == addr);
      
   return std::make_pair(modName, fn);
}

std::pair<pdstring, const function_t*>
moduleMgr::findModAndFn(kptr_t addr, bool startOnly) const {
   chunkmap_t::const_iterator chunk_iter = addr2ChunkMapIter(addr);
   assert(chunk_iter != functionFinder.end());
   const chunkData &theChunkData = chunk_iter->second;

   const pdstring &modName = theChunkData.modName;
   const function_t *fn = theChunkData.fn;
      
   if (startOnly)
      assert(fn->getEntryAddr() == addr);
      
   return std::make_pair(modName, fn);
}

// --------------------

std::pair<pdstring, function_t*>
moduleMgr::findModAndFnOrNULL(kptr_t addr, bool startOnly) {
   static std::pair<pdstring, function_t*> notfound_result("", NULL);

   chunkmap_t::const_iterator chunk_iter = addr2ChunkMapIter(addr);
   if (chunk_iter == functionFinder.end())
      // not found
      return notfound_result;

   const chunkData &theChunkData = chunk_iter->second;
   const pdstring &modName = theChunkData.modName;
   function_t *fn = theChunkData.fn;
      
   if (startOnly && fn->getEntryAddr() != addr)
      // not found
      return notfound_result;
   
   return std::make_pair(modName, fn);
}

std::pair<pdstring, const function_t*>
moduleMgr::findModAndFnOrNULL(kptr_t addr, bool startOnly) const {
   static std::pair<pdstring, const function_t*> notfound_result("", NULL);

   chunkmap_t::const_iterator chunk_iter = addr2ChunkMapIter(addr);
   if (chunk_iter == functionFinder.end())
      // not found
      return notfound_result;

   const chunkData &theChunkData = chunk_iter->second;
   const pdstring &modName = theChunkData.modName;
   const function_t *fn = theChunkData.fn;
      
   if (startOnly && fn->getEntryAddr() != addr)
      // not found
      return notfound_result;
   
   return std::make_pair(modName, fn);
}

// --------------------

bool moduleMgr::ensure_start_of_bb(kptr_t destAddr) {
   // can't call findFn() because we need to be lenient (and return false) if fn is
   // not found.
   assert(instr_t::instructionHasValidAlignment(destAddr));

   chunkmap_t::const_iterator iter = addr2ChunkMapIter(destAddr);
   if (iter == functionFinder.end())
      // not found
      return false;

   const chunkData &theChunkData = iter->second;
   function_t &fn = *theChunkData.fn;

   if (!fn.isUnparsed()) {
      assert(fn.containsAddr(destAddr));
      fn.ensure_start_of_bb(destAddr);
   }

   return true;
}

// ------------------------------------------------------------

void moduleMgr::getMemUsage(unsigned &forBasicBlocks,
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
                            ) const {
   const_iterator moditer = begin();
   const_iterator modfinish = end();
   for (; moditer != modfinish; ++moditer) {
      const loadedModule *mod = *moditer;
      
      mod->getMemUsage(forBasicBlocks,
                       forFnOrigCodes,
                       forStringPools,
                       forLiveRegAnalysisInfo,
                       forLiveRegAnalysisInfo_num1heap,
                       forLiveRegAnalysisInfo_num2heap,
                       forLiveRegAnalysisInfo_numOtherheap,
                       forFnStructAndSmallOther
                       );
   }
//Eventually, should implement on other platforms. -Igor
#ifdef sparc_sun_solaris2_7
   forLiveRegAnalysisInfo_1heap += sparc_mbdf_window_alloc::getMemUsage_1heap();
   forLiveRegAnalysisInfo_2heap += sparc_mbdf_window_alloc::getMemUsage_2heap();
#endif
   
   // call graph:
   forCallGraph += theCallGraph.getMemUsage();
}


// ------------------------------------------------------------

#ifdef _KERNINSTD_
void moduleMgr::
doInterProcRegisterAnalysis1FnOnly_CallsAlreadyKnown_NoBailOnRecursion
(kptr_t fnStartAddr, bool verbose)
{
   const chunkData &theChunkData = addr2ModAndFn(fnStartAddr);
   function_t &fn = *theChunkData.fn;
   // not const, since reg analysis sets some stuff (e.g. fn's reg anal dfs state)
   assert(fn.getEntryAddr() == fnStartAddr);
         
   const RegAnalysisHandleCall_AlreadyKnown
      theOracle(*this,
                RegAnalysisHandleCall_AlreadyKnown::LatticeIsAReference(),
                liveRegAnalysisLattice,
                verbose);
      
   doInterProcRegisterAnalysis1Fn(theChunkData.modName, fn, theOracle,
                                  false); // not verbose
}

void moduleMgr::
doInterProcRegisterAnalysis1FnOnly_CallsAlreadyKnown_BailOnRecursion
   (kptr_t fnStartAddr, bool verbose)
{
   const chunkData &theChunkData = addr2ModAndFn(fnStartAddr);
   function_t &fn = *theChunkData.fn;
   // not const, since reg analysis sets some stuff (e.g. fn's reg anal dfs state)
   assert(fn.getEntryAddr() == fnStartAddr);
         
   RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion
      theOracle(*this, fn.getEntryAddr(),
                RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion::LatticeIsAReference(),
                liveRegAnalysisLattice,
                verbose);
      
   doInterProcRegisterAnalysis1Fn(theChunkData.modName, fn,
                                  theOracle, false); // not verbose
}

// ------------------------------------------------------------

void moduleMgr::
addFunctionsWithKnownNumLevels(const pdvector<oneFnWithKnownNumLevelsByAddr> &v) {
   liveRegAnalysisLattice.addFnsWithKnownNumLevelsByAddr(v, true);
      // true --> re-sort now
}

// ------------------------------------------------------------

void moduleMgr::doGlobalRegisterAnalysis(bool verbose) {
   // strategy: depth-first search, after doing some hard-coded ones for testing

   RegAnalysisHandleCall_FirstTime theOracle(*this,
                                             RegAnalysisHandleCall_FirstTime::LatticeIsAReference(),
                                             liveRegAnalysisLattice,
                                             verbose);

   // Loop thru all functions.
   dictionary_hash<pdstring, loadedModule*>::const_iterator moditer =
      modulesByName.begin();
   dictionary_hash<pdstring, loadedModule*>::const_iterator modfinish =
      modulesByName.end();
   
   for (; moditer != modfinish; ++moditer) {
      loadedModule *mod = moditer.currval();

      loadedModule::iterator moditer_finish = mod->end();
      for (loadedModule::iterator fniter = mod->begin(); fniter != moditer_finish;
           ++fniter) {
         function_t *fn = *fniter;

         if (verbose)
            cout << "doGlobalRegisterAnalysis -- processing fn "
                 << addr2hex(fn->getEntryAddr()) << " "
                 << fn->getPrimaryName() << endl;

         // 
         // check for a skipped fn
         //

         if (fn->isUnparsed())
            continue;

         switch (fn->getRegAnalysisDFSstate()) {
            case function_t::unvisited: {
               doInterProcRegisterAnalysis1Fn(mod->getName(), *fn,
                                              theOracle,
                                              verbose);
                  // no longer throws an exception if it fails to analyze; will instead
                  // set the result to conservative and return normally.
               break;
            }
            case function_t::visitedButNotCompleted:
               cout << fn->getPrimaryName() << " about to bomb" << endl;
               assert(false); // not 100% sure how to handle this yet
               break;
            case function_t::completed:
               //cout << "moduleMgr::doGlobalRegisterAnalysis() don't need to do fn " << fn->getPrimaryName() << " since already done" << endl;
               break; // nothing needed
            default:
               assert(false);
         }
      }
   }
}

moduleMgr::dataflow_fn*
moduleMgr::getLiveRegFnBeforeInsnAddr(const function_t &fn,
                                      kptr_t insnAddr, bool verbose) const {
   // NOTE: must be called after doGlobalRegisterAnalysis() has already executed.

   // liveRegAnalysisLattice was pre-calculated:
   return fn.getLiveRegFnBeforeInsnAddr(insnAddr,
                                        RegAnalysisHandleCall_AlreadyKnown(*this,
                                                                           RegAnalysisHandleCall_AlreadyKnown::LatticeIsAReference(),
                                                                           liveRegAnalysisLattice,
                                                                           verbose),
                                        true);
      // true --> use cached answer at top of bb, if appropriate
}

moduleMgr::dataflow_fn*
moduleMgr::getLiveRegFnAfterInsnAddr(const function_t &fn,
                                     kptr_t insnAddr, bool verbose) const {
   // NOTE: must be called after doGlobalRegisterAnalysis() has already executed.

   // liveRegAnalysisLattice was pre-calculated:
   return fn.
      getLiveRegFnAfterInsnAddr(insnAddr,
                                RegAnalysisHandleCall_AlreadyKnown(*this,
                                                                   RegAnalysisHandleCall_AlreadyKnown::LatticeIsAReference(),
                                                                   liveRegAnalysisLattice,
                                                                   verbose));
}

moduleMgr::dataflow_fn*
moduleMgr::getDeadRegFnBeforeInsnAddr(const function_t &fn,
                                      kptr_t insnAddr, bool verbose) const {
   // First we do the opposite, finding *live* regs; then we call swapStartsAndStops().
   // NOTE: must be called after doGlobalRegisterAnalysis() has already executed.

   dataflow_fn* result = fn.
      getLiveRegFnBeforeInsnAddr(insnAddr,
                                 RegAnalysisHandleCall_AlreadyKnown(*this,
                                                                    RegAnalysisHandleCall_AlreadyKnown::LatticeIsAReference(),
                                                                    liveRegAnalysisLattice,
                                                                    verbose),
                                 true); // true --> use cached answer if available
   result->swapStartsAndStops();

   return result;
}

moduleMgr::dataflow_fn*
moduleMgr::getDeadRegFnAfterInsnAddr(const function_t &fn,
                                     kptr_t insnAddr, bool verbose) const {
   // First we do the opposite, finding *live* regs; then we call swapStartsAndStops().
   // NOTE: must be called after doGlobalRegisterAnalysis() has already executed.

   dataflow_fn* result =
      fn.getLiveRegFnAfterInsnAddr(insnAddr,
                                   RegAnalysisHandleCall_AlreadyKnown(*this,
                                                                      RegAnalysisHandleCall_AlreadyKnown::LatticeIsAReference(),
                                                                      liveRegAnalysisLattice,
                                                                      verbose));
   result->swapStartsAndStops();
   return result;
}

std::pair<moduleMgr::dataflow_fn*, moduleMgr::dataflow_fn*>
moduleMgr::getDeadRegFnsBeforeAndAfterInsnAddr(const function_t &fn,
                                               kptr_t insnAddr, bool verbose) const {
   // First we do the opposite, finding *live* regs; then we call swapStartsAndStops().
   // NOTE: must be called after doGlobalRegisterAnalysis() has already executed.

   // <1 usecs:
   RegAnalysisHandleCall_AlreadyKnown theOracle(*this,
                                                RegAnalysisHandleCall_AlreadyKnown::LatticeIsAReference(),
                                                liveRegAnalysisLattice,
                                                verbose);

   const mrtime_t part2StartTime = verbose ? getmrtime() : 0;

   // Basically, all the time is spent here:
   std::pair<dataflow_fn *, dataflow_fn *> result =
      fn.getLiveRegFnBeforeAndAfterInsnAddr(insnAddr, theOracle);

   if (verbose) {
      const mrtime_t part2TotalTime = getmrtime() - part2StartTime;
      cout << "moduleMgr::getDeadRegFnsBeforeAndAfterInsnAddr() fn.getLiveRegFnBeforeAndAfterInsnAddr() took " << endl
           << part2TotalTime << " nsecs, or " << part2TotalTime / 1000 << " usecs"
           << endl;
   }

   // swap starts & stops: 7-8 usecs:
   result.first->swapStartsAndStops();
   result.second->swapStartsAndStops();

   // Can't assert that result.first equals getDeadRegFnBeforeInsnAddr() because
   // getDeadRegFnBeforeInsnAddr() might retrieve a previously calculated cached
   // value (when insnAddr is the top of a basic block, to be specific), which could
   // have been calculated using a different callOracle.  In any event, the desired
   // assert is done by function_t.

   return result;
}

void moduleMgr::getDeadRegsBeforeAndAfterInsnAddr(const function_t &fn,
                                                  kptr_t insnAddr,
                                                  regset_t &before,
                                                  regset_t &after,
                                                  bool verbose) const {
   // Rest of this routine takes 6-8 usecs (actually that was w/o -O2; recheck)
   const mrtime_t phase2StartTime = verbose ? getmrtime() : 0;

   try {
      std::pair<dataflow_fn*, dataflow_fn*> fns = 
         getDeadRegFnsBeforeAndAfterInsnAddr(fn, insnAddr, verbose);

      try {
         regset_t *before_ptr = fns.first->apply(regset_t::getEmptySet());
         before = *before_ptr;
         assert(!before.exists(regset_t::getRegsThatIgnoreWrites())); // sanity check

         // For safety, one should never say that always live regs are dead
         before -= regset_t::getAlwaysLiveSet();

         //before has a copy of what is in before_ptr -> can delete
         delete before_ptr;
      }
      catch (monotone_bitwise_dataflow_fn_ll::CannotApply) {
         // conservative: no regs are dead
         before = regset_t::getEmptySet();
      }

      try {
         regset_t *after_ptr = fns.second->apply(regset_t::getEmptySet());
         after = *after_ptr;
         assert(!after.exists(regset_t::getRegsThatIgnoreWrites())); // sanity check
        
         // For safety, one should never say that always live regs are dead
         after -= regset_t::getAlwaysLiveSet();
         
         //after has a copy of what is in after_ptr -> can delete
         delete after_ptr;
      }
      catch (monotone_bitwise_dataflow_fn_ll::CannotApply) {
         // conservative: no regs are dead
         after = regset_t::getEmptySet();
      }

      if (verbose) {
         const mrtime_t phase2TotalTime = getmrtime() - phase2StartTime;
         cout << "moduleMgr::getDeadRegsBeforeAndAfterInsnAddr() phase 2 [apply() & cleanup] took"
              << endl
              << phase2TotalTime / 1000 << " usecs." << endl;
      }
   }
   catch (monotone_bitwise_dataflow_fn_ll::
	  DisagreeingNumberOfPreSaveFramesInMerge) {
      // conservative: no regs are dead
      before =  regset_t::getEmptySet();
      after =  regset_t::getEmptySet();
      return;
   }
}


regset_t*
moduleMgr::getDeadRegsAtPoint(const function_t &fn,
                              kptr_t addr, bool preReturn) const {
   // sorry, but this'll be a bit slow due to some LRAndR activity that
   // we don't really need.  But it's consistent with the choice of LRAndR,
   // and thus satisfying for correctness.
   // (Actually, here's a news flash: the LRAndR part was really quick)

   extern bool verbose_runtimeRegisterAnalysisTimings;
   const bool verbose = verbose_runtimeRegisterAnalysisTimings;

   const mrtime_t getInsnInfoStartTime = verbose ? getmrtime() : 0;

   const instr_t *insn = fn.get1OrigInsn(addr);
   instr_t::controlFlowInfo cfi;
   insn->getControlFlowInfo(&cfi);

   if (verbose) {
      const mrtime_t getInsnInfoTotalTime = getmrtime() - getInsnInfoStartTime;
      cout << "moduleMgr::getDeadRegsAtPoint() -- get1OrigInsn() & getControlFlowInfo took"
           << endl
           << getInsnInfoTotalTime << " nsecs, or "
           << getInsnInfoTotalTime / 1000 << " usecs." << endl;
   }

   const mrtime_t getDeadRegsStartTime = verbose ? getmrtime() : 0;

   regset_t* free_regs_before = regset_t::getRegSet(regset_t::empty);
   regset_t* free_regs_after = regset_t::getRegSet(regset_t::empty);
   getDeadRegsBeforeAndAfterInsnAddr(fn, addr, *free_regs_before, *free_regs_after,
                                     false // not verbose (?)
                                     );

   if (verbose) {
      const mrtime_t getDeadRegsTotalTime = getmrtime() - getDeadRegsStartTime;
      cout << "moduleMgr::getDeadRegsAtPoint() -- getDeadRegsBeforeAndAfterInsnAddr() took"
           << endl
           << getDeadRegsTotalTime << " nsecs, or "
           << getDeadRegsTotalTime/1000 << " usecs." << endl;
   }
   
   // If we've calculated that %g0 is dead either before or after the insn,
   // then we've calculated wrong!
   assert(!free_regs_before->exists(regset_t::getRegsThatIgnoreWrites()));
   assert(!free_regs_after->exists(regset_t::getRegsThatIgnoreWrites()));

   const mrtime_t createLRAndRStartTime = verbose ? getmrtime() : 0;
   
   regset_t *free_intregs_before = 
      free_regs_before->setIntersection(regset_t::getAllIntRegs());
   
   regset_t *free_intregs_after = 
      free_regs_after->setIntersection(regset_t::getAllIntRegs());
  
   //We must make a copy of insn, since it comes from get1OrigInsn,
   // i.e. points to internal function_t structures.
   instr_t *insn_copy = instr_t::getInstr(*insn);
  
   LRAndR *theLRAndR =
      LRAndR::create_LRAndR(addr, fn, insn_copy, &cfi,
			    free_intregs_before,
			       // create_LRAndR demands only an integer regset
			    free_intregs_after,
			       // create_LRAndR demands only an integer regset
			    preReturn,
			    false
			       // not verbose if, e.g. a particular
			       // tail call sequence unwinding is not yet
			       // implemented
			    );
   assert(theLRAndR);

   if (verbose) {
      const mrtime_t createLRAndRTotalTime = getmrtime() - createLRAndRStartTime;
      cout << "moduleMgr::getDeadRegsAtPoint() -- create_LRAndR() took"
           << endl
           << createLRAndRTotalTime << " nsecs, or "
           << createLRAndRTotalTime/1000 << " usecs." << endl;
   }
      
   // Now that an LRAndR has been chosen, we can ask it for the free
   // regs at this point.  Expensive to create an LRAndR temporarily just
   // for that purpose...but let's face it: only LRAndR's know all the gritty
   // details of the instrumentation point, can make sense out of
   // preInsn vs. preReturn, etc.

   // Important: set preReturn to false if that location type is undefined for this
   // kind of LRAndR:
   if (!theLRAndR->preReturnHasMeaning())
      preReturn = false;

   // Note that the following line doesn't get a reference to the LRAndR's
   // stored regset_t, because the imminent delete of the LRAndR
   // would cause the "return result" to read freed memory (thanks, Purify).
   const mrtime_t finalGetRegsStartTime = verbose ? getmrtime() : 0;
   regset_t *result = preReturn ?
      regset_t::getRegSet(*theLRAndR->getInitialIntRegsForExecutingPreReturnSnippets()) :
      regset_t::getRegSet(*theLRAndR->getInitialIntRegsForExecutingPreInsnSnippets());
      // caution: default get...PreReturnSnippets() is the dummy regset_t::empty
      // for LRAndR's which don't have meaningful preReturn semantics.  Therefore, it's
      // important that we set preReturn to false if it were true and if this
      // LRAndR doesn't have meaningful preReturn semantics.

   if (verbose) {
      const mrtime_t finalGetRegsEndTime = getmrtime() - finalGetRegsStartTime;
      cout << "moduleMgr::getDeadRegsAtPoint() -- getInitialIntRegsForExecuting*Snippets took"
           << endl
           << finalGetRegsEndTime << " nsecs, or "
           << finalGetRegsEndTime/1000 << " usecs." << endl;
   }

   const mrtime_t deleteLRAndRStartTime = verbose ? getmrtime() : 0;

   delete theLRAndR;
   theLRAndR = NULL; // this might help purify find mem leaks

   if (verbose) {
      const mrtime_t deleteLRAndRTotalTime = getmrtime() - deleteLRAndRStartTime;
      cout << "moduleMgr::getDeadRegsAtPoint() -- deleting LRAndR took"
           << endl
           << deleteLRAndRTotalTime << " nsecs, or "
           << deleteLRAndRTotalTime/1000 << " usecs." << endl;
   }

   // If we've calculated that %g0 is free, then we've calculated wrong!
   assert(!result->exists(regset_t::getRegsThatIgnoreWrites()));

   if (verbose)
      cout << "moduleMgr::getDeadRegsAtPoint(): bye" << endl << endl;

   delete free_regs_before;
   delete free_regs_after;
   
   return result;
}

#endif // _KERNINSTD_

// --------------------

const pdvector<kptr_t> &
moduleMgr::getCallSitesTo(kptr_t fnEntryAddr,
                          bool &isResultSorted) const {
   return theCallGraph.getCallsMadeToAFn(fnEntryAddr, isResultSorted);
}

void moduleMgr::change1CallSite(kptr_t callSiteAddr,
                                kptr_t oldCalleeAddr,
                                kptr_t newCalleeAddr) {
   theCallGraph.removeCallSite(callSiteAddr, oldCalleeAddr, true); // true --> assertion
   theCallGraph.addCallSite(callSiteAddr, newCalleeAddr, true); // true --> assertion
}

sampleStatistics moduleMgr::getStatsOfCalledFunctions(unsigned &numZeros) const {
   return theCallGraph.getStatsOfCalledFunctions(numZeros);
}
