// outlinedGroup.h

#ifndef _OUTLINED_GROUP_H_
#define _OUTLINED_GROUP_H_

#include "funkshun.h"
#include "archTraits.h"
#include "downloadToKernel.h" // kerninstd/igen
#include "kerninstdClient.xdr.CLNT.h"
#include "fnBlockAndEdgeCounts.h"
#include "outliningEmit.h" // pickBlockOrderingFnType (pointer to function)
#include "hotBlockCalcOracle.h"
#include "usparc_icache.h"

class groupMgr;
   // anything more than a fwd decl would result in recursive #include's
class hotBlockCalcOracle;
class fnCodeObjects; // possible since we only use ptrs & refs in this .h file
class outlining1FnCounts; // possible since we only use ptrs & refs in this .h file
class outliningLocations; // possible since we only use ptrs & refs in this .h file

class outlinedGroup {
 public:
   typedef function_t::bbid_t bbid_t;

   enum blockPlacementPrefs {origSeq, chains};

 private:
   // ------------------ prefs --------------------

   bool doTheReplaceFunction;
   bool replaceFunctionPatchUpCallSitesToo;

   pickBlockOrderingFnType blockOrderingFunc;
      // a direct function of "theBlockPlacementPrefs"
      // pickBlockOrderingOrigSeq or pickBlockOrderingChains, at the moment

   hotBlockCalcOracle::ThresholdChoices theHotBlockThresholdChoice;
   
   // ------------------ group id --------------------

   const unsigned groupUniqueId;
   static unsigned nextUnusedGroupId;

   // ------------------ root fn of the group --------------------

   pdstring rootFnModName;
   const function_t &rootFn;
      // unlike allFuncsInGroup[] and rootFnNdx, this vrble is initialized in the ctor.
      // That's why we need to keep this vrble
   unsigned rootFnGroupId; // UINT_MAX if wasn't already outlined
   fnBlockAndEdgeCounts *rootFnBlockAndEdgeCounts;

   pdvector<kptr_t> forceIncludeDescendants;

   groupMgr &theGroupMgr;

   // fnBlockCounters[] dictionary is very important: it caches block counts
   // for (possibility lots of) functions.  This is the first thing that we
   // collect; that is: first block counts for ALL functions that we're interested
   // in outlining are collected, and ONLY THEN do we give any thought to
   // collecting code objects, generating outlined code, and installing it
   // into the kernel.
   dictionary_hash<kptr_t, pdvector<unsigned> > fnBlockCounters;
      // key is the *original* address of the function, not its outlined place of
      // residence.

   // ------------------------------------------------------------

   // Code objects stuff:
   dictionary_hash<kptr_t, bool> codeObjectParseRequestsNotYetSent;
      // .value is a dummy
      // This dictionary exist due to an igen flaw: sending over a flood of
      // async requests can lead to deadlock, as our outgoing socket buffer fills
      // up (thus we block on a _write()), while kerninstd can't soak up anything
      // from the buffer because it's trying to send us the result from a much
      // earlier parse request.  Note that this deadlock happens even when all of the
      // igen calls are $async.  The solution is for kperfmon to calm down a bit and
      // never send a flood of requests...instead, send one at a time, keeping track
      // of which requests still have to be sent.  codeObjectParseRequestsNotYetSent[]
      // is that set.
   dictionary_hash<unsigned, kptr_t> pendingCodeObjectParseRequests;
      // requests that have been sent to kerninstd but for which we haven't
      // yet received a response.
      // key: reqid  value: fn entry address

   dictionary_hash<kptr_t, fnCodeObjects*> parsedFnCodeObjects;
      // key: fn's entry address
   unsigned numNonNullEntriesInParsedFnCodeObjects;
      // needed because parsedFnCodeObjects.size() will include the NULL sentinel
      // entries that we have occasion to place.

   kerninstdClientUser *connection_to_kerninstd;

   // ------------------------------------------------------------

   // Not initialized until the start of doTheActualOutlining().  Excludes fns
   // which might otherwise slip through the cracks, and be, undeservedly, included
   // in the outlined group.  So allFuncsInGroup.size() can be less than
   // parsedFnCodeObjects.size() and fnBlockCounters.size(), for example.
   pdvector< pair<pdstring, const function_t*> > allFuncsInGroup;
   unsigned rootFnNdx; // ndx into the above vector

   unsigned downloadedToKernelReqId; // -1U when undefined
   kptr_t outlinedCodeEntryAddr; // quite different from fn.getStartAddr() of course.
      // We need to store this for one reason: replaceFn
      // Also note that this will be the start of the *code*; data (like
      // a jump table residing just before the fn) will come just *before* this.
      // I don't think that we need to store where the data starts.

   pdvector<unsigned> replaceFnReqIds;

   // ------------------------------------------------------------

   static void updateICacheStatsFor1Fn(usparc_icache &icstats,
                                       const function_t &fn,
                                       const pdvector<bbid_t> &hotBlockIds);

   // ------------------------------------------------------------

   void downloadCodeToKernelAndParseAndReplaceFn(const pdvector<downloadToKernelFn> &,
                                                 const emitOrdering &,
                                                 bool tryForNucleus);
      // This is the big one.

   void doReplaceFunctions(
       const pdvector< pair<pdvector<kptr_t>,unsigned> > &downloadedAddrs,
       bool callSitesToo);
      // called by downloadCodeToKernelAndParseAndREplaceFn()

   outlinedGroup(const outlinedGroup &);
   outlinedGroup &operator=(const outlinedGroup &);

   void doTheActualOutlining();
   
 public:
   outlinedGroup(bool doTheReplaceFunction,
                 bool replaceFunctionPatchUpCallSitesToo,
                 blockPlacementPrefs,
                 hotBlockCalcOracle::ThresholdChoices,
                 const pdstring &rootFnModName, const function_t &rootFn,
		 const pdvector<kptr_t> iForceIncludeDescendants,
		 const dictionary_hash< kptr_t, pdvector<unsigned> > &iCounters,
		 groupMgr& iGroupMgr,
                 kerninstdClientUser *igen_connection);
      // sorry, we can't yet initialize "allFuncsInGroup"; we must do that later.
  ~outlinedGroup();

   void changeDoTheReplaceFunctionFlag(bool);
   void changeOutliningReplaceFunctionPatchUpCallSitesTooFlag(bool);
   void changeOutliningBlockPlacementTo(blockPlacementPrefs);

   hotBlockCalcOracle::ThresholdChoices getHotBlockThresholdChoice() const {
      return theHotBlockThresholdChoice;
   }
   
   void kperfmonIsGoingDownNow();
      // clean everything up (dtor will assert as such)

   const function_t &getRootFn() const {
      return rootFn;
   }
   kptr_t getRootFnEntryAddr() const {
      return rootFn.getEntryAddr();
   }

   const pdvector<unsigned> &get1FnBlockCounts(kptr_t fnEntryAddr) const {
      return fnBlockCounters.get(fnEntryAddr);
   }
   const pdvector<unsigned> &getRootFnBlockCounts() const {
      return get1FnBlockCounts(rootFn.getEntryAddr());
   }
   const fnCodeObjects &get1FnCodeObjects(kptr_t fnEntryAddr) const {
      // NOTE: Could be unParsed()
      const fnCodeObjects *fco = parsedFnCodeObjects.get(fnEntryAddr);
      return *fco;
   }
   
   const pdvector<kptr_t> &getForceIncludeDescendants() const {
      return forceIncludeDescendants;
   }

   // --------------------

   void asyncSend1CodeObjectParseRequest();
   void handle1FnCodeObjects(unsigned reqid, const fnCodeObjects &);

   // --------------------

   void asyncOutline();

   void unOutline();
};

#endif
