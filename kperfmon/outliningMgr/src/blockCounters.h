// blockCounters.h

#ifndef _BLOCK_COUNTERS_H_
#define _BLOCK_COUNTERS_H_

#include "tcl.h" // ClientData
#include "kapi.h"
#include "common/h/String.h"
#include "common/h/Vector.h"
#include "outlining1FnCounts.h"

class outliningMgr;

class blockCounters {
    // ------------------ prefs --------------------

    unsigned outliningBlockCountDelay; // in millisecs
    bool doChildrenToo;
    bool forceOutliningOfAllChildren;

   // ------------------ root fn of the group --------------------

    pdstring rootFnModName;
    kapi_function krootFn;

    unsigned rootFnGroupId; // UINT_MAX if wasn't already outlined

    // ------------------ fixed descendants --------------------

    bool usingFixedDescendants;
    kapi_vector<kptr_t> fixedDescendants;
    // Each entry: entryAddr of a function

    kapi_vector<kptr_t> forceIncludeDescendants;
  
    // Some functions can not be outlined (e.g., mutex_exit)
    kapi_vector<kptr_t> forceExcludeDescendants;

    // Allows us to notify the outlining mgr when we are done collecting counts
    outliningMgr &theOutliningMgr;

    // We want to keep the number of functions profiled simultaneously
    // under a certain threshold. Otherwise, kerninstd gets overwhelmed
    // with thousands of sampling requests. 
    unsigned numOutstandingCollectRequests;

    // Functions waiting to be profiled when the number of outstanding
    // requests drops below the threshold.
    pdvector< std::pair<pdstring, kapi_function> > pendingCollectRequests;
    // ------------------------------------------------------------

    // fnBlockCounters[] dictionary is very important: it caches block counts
    // for (possibility lots of) functions.  This is the first thing that we
    // collect; that is: first block counts for ALL functions that we're
    // interested in outlining are collected, and ONLY THEN do we give any
    // thought to collecting code objects, generating outlined code, and
    // installing it into the kernel.
    dictionary_hash<kptr_t, outlining1FnCounts*> fnBlockCounters;
    // key is the *original* address of the function, not its outlined
    // place of residence.

    // ------------------------------------------------------------

    // will assert fail if the fn is "unParsed()" or block counts havealready
    // been collected for this fn (or are in the process of being collected).
    void asyncCollectBlockCountsForFn(const pdstring &modName,
				      const kapi_function &kfn);

    static void blockCountsFor1FnHaveBeenReceived_part2(ClientData);
    void blockCountsFor1FnHaveBeenReceived_part3(const kapi_function &kfn);

    bool haveBlockCountsAlreadyFullyArrivedAndThenBeenUninstrumentedFor1Fn(
	const kapi_function &kfn) const;

    // you pass the fn (could be an already-outlined function);
    // we look up the *original* function (if the one you passed in was
    // outlined) and then return true iff in fixedDescendants[].
    bool isFnInFixedDescendants(const kapi_function &ikfn) const;
    
    // Same as above but for the force-included descendants
    bool isFnInForceIncludeDescendants(const kapi_function &ikfn) const;

    // Same as above but for the force-included descendants
    bool isFnInForceExcludeDescendants(const kapi_function &ikfn) const;

    void doneCollectingBlockCountsFor1Fn(const kapi_function &kfn);

   // ------------------------------------------------------------

    blockCounters(const blockCounters &);
    blockCounters &operator=(const blockCounters &);

 public:
    blockCounters(
	unsigned iOutliningBlockCountDelay, // in millisecs
	bool iDoChildrenToo,
	bool iForceOutliningOfAllChildren,
	const pdstring &iRootFnModName, const kapi_function &iRootFn,
	bool usingFixedDescendants,
	const pdvector< std::pair<pdstring,kapi_function> > &fixedDescendants,
	const pdvector< std::pair<pdstring,kapi_function> > &iforceIncludeDescendants,
	const pdvector< std::pair<pdstring,kapi_function> > &iforceExcludeDescendants,
	outliningMgr &iOutliningMgr);

    ~blockCounters();

    void changeOutliningBlockCountDelay(unsigned msecs);
    void changeOutlineChildrenTooFlag(bool);
    void changeForceOutliningOfAllChildrenFlag(bool);
   
    void kperfmonIsGoingDownNow();
    // clean everything up (dtor will assert as such)

    void asyncCollectBlockCounts();
    void blockCountsFor1FnHaveBeenReceived(const kapi_function &kfn);
    void checkForDoneWithBlockCountCollectingStage(
	const kapi_function &lastBlkCntArrivalFn);
};

#endif
