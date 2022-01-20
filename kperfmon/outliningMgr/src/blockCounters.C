#include <algorithm> // sort
#include <stdio.h> // FILE ops
#include "kapi.h"

#include "util/h/hashFns.h"
#include "util/h/mrtime.h"
#include "util/h/rope-utils.h"

#include "blockCounters.h"
#include "outliningMgr.h"
#include "allComplexMetrics.h"
#include "metricFocusManager.h"
#include "instrument.h"

extern kapi_manager kmgr;

blockCounters::blockCounters(
    unsigned ioutliningBlockCountDelay, // in millisecs
    bool idoChildrenToo,
    bool iforceOutliningOfAllChildren,
    const pdstring &irootFnModName,
    const kapi_function &irootFn,
    bool iusingFixedDescendants,
    const pdvector< pair<pdstring, kapi_function> > &ifixedDescendants,
    const pdvector< pair<pdstring, kapi_function> > &iforceIncludeDescendants,
    const pdvector< pair<pdstring, kapi_function> > &iforceExcludeDescendants,
    outliningMgr &iOutliningMgr) :
    outliningBlockCountDelay(ioutliningBlockCountDelay),
    doChildrenToo(idoChildrenToo),
    forceOutliningOfAllChildren(iforceOutliningOfAllChildren),
    rootFnModName(irootFnModName),
    krootFn(irootFn),
#ifdef sparc_sun_solaris2_7
    rootFnGroupId(kmgr.parseOutlinedLevel(krootFn.getEntryAddr())),
#else
    rootFnGroupId(0),
#endif
    usingFixedDescendants(iusingFixedDescendants),
    theOutliningMgr(iOutliningMgr),
    numOutstandingCollectRequests(0),
    fnBlockCounters(addrHash4)
{
    // initialized fixedDescendants, then sort it:
    pdvector< pair<pdstring, kapi_function> >::const_iterator iter =
	ifixedDescendants.begin();
    pdvector< pair<pdstring, kapi_function> >::const_iterator finish =
	ifixedDescendants.end();
    for (; iter != finish; ++iter) {
	fixedDescendants.push_back(iter->second.getEntryAddr());
    }
    std::sort(fixedDescendants.begin(), fixedDescendants.end());

    // initialize forceIncludeDescendants, then sort it:
    iter = iforceIncludeDescendants.begin();
    finish = iforceIncludeDescendants.end();
    for (; iter != finish; ++iter) {
	forceIncludeDescendants.push_back(iter->second.getEntryAddr());
    }
    std::sort(forceIncludeDescendants.begin(), forceIncludeDescendants.end());

    // initialize forceExcludeDescendants, then sort it:
    iter = iforceExcludeDescendants.begin();
    finish = iforceExcludeDescendants.end();
    for (; iter != finish; ++iter) {
	forceExcludeDescendants.push_back(iter->second.getEntryAddr());
    }
    std::sort(forceExcludeDescendants.begin(), forceExcludeDescendants.end());
}

blockCounters::~blockCounters()
{
    dictionary_hash<kptr_t, outlining1FnCounts*>::const_iterator iter =
	fnBlockCounters.begin();
    dictionary_hash<kptr_t, outlining1FnCounts*>::const_iterator finish =
	fnBlockCounters.end();
    for (; iter != finish; ++iter) {
	outlining1FnCounts *c = *iter;
	delete c;
    }
}

void blockCounters::kperfmonIsGoingDownNow()
{
    dictionary_hash<kptr_t, outlining1FnCounts*>::const_iterator iter =
	fnBlockCounters.begin();
    dictionary_hash<kptr_t, outlining1FnCounts*>::const_iterator finish =
	fnBlockCounters.end();
    for (; iter != finish; ++iter) {
	outlining1FnCounts *c = *iter;
	c->kperfmonIsGoingDownNow();
    }
}

// --------------------

void blockCounters::changeOutliningBlockCountDelay(unsigned msecs)
{
   outliningBlockCountDelay = msecs;
}

void blockCounters::changeOutlineChildrenTooFlag(bool flag)
{
   doChildrenToo = flag;
}

void blockCounters::changeForceOutliningOfAllChildrenFlag(bool flag)
{
   forceOutliningOfAllChildren = flag;
}

static void asyncInstrumentBasicBlocks(const pdstring &modName,
                                       const kapi_function &kfn,
                                       cmfSubscriber &theCmfSubscriber)
{
    // don't call this routine if the fn is unparsed:
    assert(!kfn.isUnparsed());
   
    const pdvector<long> pids_empty_vec;
    const kptr_t fnEntryAddr = kfn.getEntryAddr();
    extern allComplexMetrics global_allMetrics;
    const complexMetric &theMetric = global_allMetrics.find("entries to");

    const unsigned numBBs = kfn.getNumBasicBlocks();
    assert(numBBs > 0);
    // if 0, the "unParsed()" check about would have caught it, right?

    extern metricFocusManager *theGlobalMetricFocusManager;
   
   //sampleCodeBufferer_genesisCMFSamplesWhenDone_verbose
    sampleCodeBufferer_genesisCMFSamplesWhenDone
	mySampleCodeBufferer(numBBs, *theGlobalMetricFocusManager);

    mrtime_t lastReportedTime = getmrtime();
    bool reportedAny = false;
   
    for (unsigned bbnum=0; bbnum < numBBs; ++bbnum) {
	const focus theFocus = focus::makeFocus(modName,
						fnEntryAddr,
						bbnum,
						0, // dummy location
						0, // dummy insn num
						0, 0, // dummy "from" mod/fn
						pids_empty_vec, // pids
						CPU_TOTAL);
	asyncInstrument1Selection(theCmfSubscriber, theMetric, theFocus,
				  mySampleCodeBufferer);

	const mrtime_t currTime = getmrtime();
	if (currTime - lastReportedTime > 1000000000LL) {
	    const unsigned fracDone = (bbnum+1) * 100 / numBBs;
	    cout << "[" << fracDone << "%] ";
	    cout.flush();
         
	    lastReportedTime = currTime;
	    reportedAny = true;
	}
    }

    if (reportedAny) {
	cout << endl;
    }
}

void blockCounters::
asyncCollectBlockCountsForFn(const pdstring &modName, const kapi_function &kfn)
{
   // do not call this routine if the fn is unParsed():
   assert(!kfn.isUnparsed());
   
   const kptr_t fnEntryAddr = kfn.getEntryAddr();
   assert(!fnBlockCounters.defines(fnEntryAddr));

   const unsigned maxlen = 255;
   char buffer[maxlen];
   if (kfn.getName(buffer, maxlen) < 0) {
       assert(false && "Function name is too long");
   }
   pdstring fnName(buffer);

   // First, collect information about indirect callees of this function
   kapi_vector<kptr_t> regCallsFromAddr, regCallsToAddr;
   kapi_vector<kptr_t> interProcFromAddr, interProcToAddr;
   kapi_vector<kptr_t> unAnalyzableCallsFromAddr;

   if (kfn.getCallees(NULL,
		      &regCallsFromAddr, &regCallsToAddr,
		      &interProcFromAddr, &interProcToAddr,
		      &unAnalyzableCallsFromAddr) < 0) {
       assert(false);
   }

#ifdef sparc_sun_solaris2_7
   kapi_vector<kptr_t>::const_iterator iter =
       unAnalyzableCallsFromAddr.begin();
   for (; iter != unAnalyzableCallsFromAddr.end(); iter++) {
       const kptr_t siteAddr = *iter;
       cout << "Instrumenting fn " 
	    << addr2hex(fnEntryAddr)
	    << " (" << modName << "/" << fnName << ") at callsite " 
	    << addr2hex(siteAddr) << " to collect indirect callees\n";
       if (kmgr.watchCalleesAt(siteAddr) < 0) {
	   assert(false);
       }
   }
#endif

   // Second, asynchronously collect block counts for this function
   cout << "Instrumenting block counts for fn at " 
	<< addr2hex(fnEntryAddr)
        << " (" << modName << "/" << buffer << ")..." << endl;

   outlining1FnCounts *theCounter =new outlining1FnCounts(*this, modName, kfn);
   assert(theCounter);
      
   fnBlockCounters.set(fnEntryAddr, theCounter);
   numOutstandingCollectRequests++;
   
   extern uint64_t millisecs2cycles(unsigned, unsigned mhzTimes1000);
      // complexMetricFocus.C

   const unsigned mhz1000 = kmgr.getSystemClockMHZ() * 1000;

   const uint64_t delayIntervalCycles =
       millisecs2cycles(outliningBlockCountDelay, mhz1000);

   const uint64_t kerninstdMachTime = kmgr.getKerninstdMachineTime();

   theCounter->createGetBBCountsSubscriber(delayIntervalCycles,
					   kerninstdMachTime);


   asyncInstrumentBasicBlocks(modName, kfn, theCounter->getCmfSubscriber());
}

void blockCounters::asyncCollectBlockCounts()
{
   assert(!krootFn.isUnparsed());

   asyncCollectBlockCountsForFn(rootFnModName, krootFn);
}

void blockCounters::blockCountsFor1FnHaveBeenReceived(const kapi_function &kfn)
{
    const unsigned maxlen = 255;
    char buffer[maxlen];
    if (kfn.getName(buffer, maxlen) < 0) {
	assert(false && "Function name is too long");
    }
    cout << "block counts for fn " << buffer
	 << " have been received..." << endl;
    // we'll process as such on the next tcl idle event

    // NASTY BUG ALERT: It's tempting, now that all block counts have
    // been received, to uninstrument and unsubscribe right now.  But that
    // will lead to a core dump, since this routine was itself invoked from a
    // "perfect-bucket-received" callback routine.  So clearly, we mustn't
    // fry such a structure, lest the crucial "this" variable suddenly become
    // deleted memory!  So, we say "when next idle [tcl's good at knowing
    // that]", unsubscribe.

    pair<blockCounters *, kapi_function> *callbackData = 
	new pair<blockCounters *, kapi_function>(this, kfn);

    Tcl_DoWhenIdle(blockCountsFor1FnHaveBeenReceived_part2,
		   (ClientData)callbackData);
}

void blockCounters::blockCountsFor1FnHaveBeenReceived_part2(ClientData cd)
{
    pair<blockCounters *, kapi_function> *callbackData =
	(pair<blockCounters *, kapi_function> *)cd;
    
    blockCounters *pthis = callbackData->first;
    kapi_function kfn = callbackData->second;
   
    delete callbackData;
   
    pthis->blockCountsFor1FnHaveBeenReceived_part3(kfn);
}

void blockCounters::blockCountsFor1FnHaveBeenReceived_part3(
    const kapi_function &kfn)
{
    // uninstrument.  In order to do this, we must retrieve
    // the complexMetricFocus from allComplexMetricFocuses class. No problem,
    // so long as we have metric id and focus id.
    outlining1FnCounts *theCounter = fnBlockCounters.get(kfn.getEntryAddr());

    // Fry the cmfSubscriber; that will uninstrument everything
    theCounter->fryGetBBCountsSubscriberAndUninstrument();
      // does it all: uninstruments, unsamples, removes from global
      // dictionary of all cmf subscribers, and calls delete

    cout << "Uninstrumented bb counting for fn at "
	 << addr2hex(kfn.getEntryAddr()) << endl;

    doneCollectingBlockCountsFor1Fn(kfn);
      // if all done collecting block counts for all fns, this will move us
      // on to the next stage: collecting code objects
}

bool blockCounters::
haveBlockCountsAlreadyFullyArrivedAndThenBeenUninstrumentedFor1Fn(
    const kapi_function &kfn) const
{
    const kptr_t fnEntryAddr = kfn.getEntryAddr();

    outlining1FnCounts *c = NULL;
    if (!fnBlockCounters.find(fnEntryAddr, c)) {
	return false;
    }

    return c->allDoneCollectingAndTellingAndUninstrumenting();
}

// you pass the fn (could be an already-outlined function);
// we look up the *original* function (if the one you passed in was
// outlined) and then return true iff in fixedDescendants[].
bool blockCounters::isFnInFixedDescendants(const kapi_function &ifn) const
{
#ifdef sparc_sun_solaris2_7
   const kptr_t origFnEntryAddr =
       kmgr.getOutlinedFnOriginalFn(ifn.getEntryAddr());
      // of course, if ifn was not outlined to begin with,
      // then this call will return no changes.

   return std::binary_search(fixedDescendants.begin(), fixedDescendants.end(),
                             origFnEntryAddr);
#else
   return false;
#endif
}

// Same as above but for the force-included descendants
bool
blockCounters::isFnInForceIncludeDescendants(const kapi_function &ifn) const
{
#ifdef sparc_sun_solaris2_7
    const kptr_t origFnEntryAddr = 
	kmgr.getOutlinedFnOriginalFn(ifn.getEntryAddr());
    
    return std::binary_search(forceIncludeDescendants.begin(),
                              forceIncludeDescendants.end(),
                              origFnEntryAddr);
#else
   return false;
#endif
}

// Same as above but for the force-excluded descendants
bool
blockCounters::isFnInForceExcludeDescendants(const kapi_function &ifn) const
{
#ifdef sparc_sun_solaris2_7
    const kptr_t origFnEntryAddr = 
	kmgr.getOutlinedFnOriginalFn(ifn.getEntryAddr());
    
    return std::binary_search(forceExcludeDescendants.begin(),
                              forceExcludeDescendants.end(),
                              origFnEntryAddr);
#else
   return false;
#endif
}

void blockCounters::doneCollectingBlockCountsFor1Fn(const kapi_function &kfn)
{
    // OK, block counts have arrived and have been stored for this
    // function. Now we need to do the same for some of its callees: those
    // which were called at least once.  We used to have a stricter check:
    // those which were called enought so that their entry block was
    // considered "hot". But that had 2 problems: (1) the fn could have been
    // cold, but when summed over the several call sites at which it's
    // invoked, the total could be hot enough, and (2) even if the entry
    // block is cold, a loop within the function may cause other of its
    // blocks to be hot. In both cases, we wouldn't want to miss the chance
    // to recognize part of the callee as hot. At this stage, we can be
    // overly conservative and recognize functions as tentatively-hot, even
    // if in the end, they don't make the cut.

    assert(haveBlockCountsAlreadyFullyArrivedAndThenBeenUninstrumentedFor1Fn(
	kfn));
   
    outlining1FnCounts *theCounter = fnBlockCounters.get(kfn.getEntryAddr());
    assert(theCounter->allDoneCollectingAndTellingAndUninstrumenting());

    numOutstandingCollectRequests--;
    const pdvector<unsigned> &fnBlockCounts = theCounter->getAllBlockCounts();

    // "fnBlockCounts[]" are used to calculate the subset of blocks for
    // which we should search for calls & interprocedural branches...such
    // callees MIGHT in the end make the final cut and be included in the
    // outlined group.  Note the use of the prefs vrble
    // "forceOutliningOfAllChildren" (usually false).

    kapi_vector<bbid_t> nonZeroBlockIds, zeroBlockIds;
    const unsigned numBBs = kfn.getNumBasicBlocks();
    assert(numBBs == fnBlockCounts.size());
   
    for (bbid_t bbid = 0; bbid < numBBs; ++bbid) {
	// Note that part of the usual test for a hot block, whether the block
	// is the function's entry block, is intentionally left out here...
	// for while it's true that fn's entry block will always be considered
	// "hot", that fudgification doesn't have to be extended to calls made 
	// in such a block.
	if (forceOutliningOfAllChildren || fnBlockCounts[bbid] > 0) {
	    nonZeroBlockIds.push_back(bbid);
	}
	else {
	    zeroBlockIds.push_back(bbid);
	}
    }
   
    kapi_vector<kptr_t> regCallsFromAddr, regCallsToAddr;
    kapi_vector<kptr_t> interProcFromAddr, interProcToAddr;
    kapi_vector<kptr_t> unAnalyzableCallsFromAddr;

    if (kfn.getCallees(&nonZeroBlockIds,
		       &regCallsFromAddr, &regCallsToAddr,
		       &interProcFromAddr, &interProcToAddr,
		       &unAnalyzableCallsFromAddr) < 0) {
	assert(false);
    }

    pdvector<kptr_t> calleesToMeasure;

    // Make a note of (normal) callees...
    kapi_vector<kptr_t>::const_iterator iaddr;
    for (iaddr = regCallsToAddr.begin();
	 iaddr != regCallsToAddr.end(); iaddr++) {
	calleesToMeasure.push_back(*iaddr);
    }

    // ...now do the same for interprocedural branches
    for (iaddr = interProcToAddr.begin();
	 iaddr != interProcToAddr.end(); iaddr++) {
	calleesToMeasure.push_back(*iaddr);
    }

    // ...now do the same for collected indirect calls
#ifdef sparc_sun_solaris2_7
    for (iaddr = unAnalyzableCallsFromAddr.begin();
	 iaddr != unAnalyzableCallsFromAddr.end(); iaddr++) {
	const kptr_t siteAddr = *iaddr;
	if (kmgr.unWatchCalleesAt(siteAddr) < 0) {
	    abort();
	}
	kapi_vector<kptr_t> callees;
	kapi_vector<unsigned> counts;
	if (kmgr.getCalleesForSite(siteAddr, &callees, &counts) < 0) {
	    abort();
	}

	cout << "Callees at " << addr2hex(siteAddr) << " are:";
	kapi_vector<kptr_t>::const_iterator icallee = callees.begin();
	kapi_vector<unsigned>::const_iterator icount = counts.begin();
	for (; icallee != callees.end() && icount != counts.end();
	     icallee++, icount++) {
	    kptr_t calleeAddr = *icallee;
	    unsigned calleeCount = *icount;
	    cout << " (" << addr2hex(calleeAddr) << ", " << calleeCount << ")";
	    // We do not want all indirect callees (or we'll outline the entire
	    // kernel. Let's select only "frequently-executed" ones:
	    // whose execution count is >= 1/4 of their parent's count
	    if (calleeCount * 4 >= fnBlockCounts[0]) {
		calleesToMeasure.push_back(calleeAddr);
	    }
	}
	assert(icallee == callees.end() && icount == counts.end());
	cout << endl;
    }
#endif

    // Some of the call sites are still watched (the cold ones). Let's
    // stop watching them for cleanness
    kapi_vector<kptr_t> unAnalyzableColds;

    if (zeroBlockIds.size() != 0 &&
	kfn.getCallees(&zeroBlockIds, &regCallsFromAddr, &regCallsToAddr,
		       &interProcFromAddr, &interProcToAddr,
		       &unAnalyzableColds) < 0) {
	assert(false);
    }
    for (iaddr = unAnalyzableColds.begin();
	 iaddr != unAnalyzableColds.end(); iaddr++) {
	const kptr_t siteAddr = *iaddr;
	if (kmgr.unWatchCalleesAt(siteAddr) < 0) {
	    abort();
	}
    }

#ifdef sparc_sun_solaris2_7
    if (kfn.getEntryAddr() == krootFn.getEntryAddr()) {
	// Now is the time to add the "forceIncludeDescendants[]". The only
	// real trick is to properly convert the module name, etc. in case we
	// are re-outlining.
      
	pdvector<kptr_t>::const_iterator force_iter =
	    forceIncludeDescendants.begin();
	pdvector<kptr_t>::const_iterator force_finish =
	    forceIncludeDescendants.end();
	for (; force_iter != force_finish; ++force_iter) {
	    const kptr_t forceOrigAddr = *force_iter;
	    const kptr_t forceThisFn = kmgr.getSameOutlinedLevelFunctionAs(
		krootFn.getEntryAddr(), forceOrigAddr);

	    calleesToMeasure.push_back(forceThisFn);
	}
    }
#endif
   
    std::sort(calleesToMeasure.begin(), calleesToMeasure.end());
    pdvector<kptr_t>::const_iterator new_end = 
       std::unique(calleesToMeasure.begin(),
                   calleesToMeasure.end());
    const unsigned new_size = new_end - calleesToMeasure.begin();
    if (new_size < calleesToMeasure.size()) {
	calleesToMeasure.shrink(new_size);
    }

    // Think of this as a breadth-first search of the call graph, rooted at
    // "fn", measuring 1 level.

    if (doChildrenToo) {
	pdvector<kptr_t>::const_iterator callee_iter = calleesToMeasure.begin();
	pdvector<kptr_t>::const_iterator callee_finish = calleesToMeasure.end();
	for (; callee_iter != callee_finish; ++callee_iter) {
	    const kptr_t destAddr = *callee_iter;
         
	    kapi_module kmodCallee;
	    kapi_function kfuncCallee;
	    if (kmgr.findModuleAndFunctionByAddress(destAddr, &kmodCallee,
						    &kfuncCallee) < 0) {
		cout << "Sorry, could not collect block counts for fn "
		     << addr2hex(destAddr) << "; that fn is unknown to us."
		     << endl;
		continue;
	    }

	    const unsigned maxlen = 255;
	    char buffer[maxlen];
	    if (kmodCallee.getName(buffer, maxlen) < 0) {
		assert(false && "Module name is too long");
	    }
	    pdstring calleeModName(buffer);
	    if (kfuncCallee.getName(buffer, maxlen) < 0) {
		assert(false && "Function name is too long");
	    }
	    pdstring calleeFuncName(buffer);

	    if (kfuncCallee.isUnparsed()) {
		// TODO: this warning will probably be printed many times for
		// the same function; fix this.
		cout << "Sorry, could not collect block counts for fn at "
		     << addr2hex(kfuncCallee.getEntryAddr()) << endl
		     << "since it was not successfully parsed into basic"
		     << " blocks." << endl;
	    }
	    else {
		kapi_module kmod;
		kapi_function ktmp;
		if (kmgr.findModuleAndFunctionByAddress(kfn.getEntryAddr(), 
							&kmod, &ktmp) < 0) {
		    assert(false);
		}
		if (kmod.getName(buffer, maxlen) < 0) {
		    assert(false && "Module name is too long");
		}
		pdstring modName(buffer);
		if (kfn.getName(buffer, maxlen) < 0) {
		    assert(false && "Function name is too long");
		}
		pdstring fnName(buffer);
		
		cout << modName << "/" << fnName << " --> "
		     << calleeModName << "/" << calleeFuncName;

		if (!fnBlockCounters.defines(kfuncCallee.getEntryAddr())) {
		    const bool isBeingExcluded = 
			isFnInForceExcludeDescendants(kfuncCallee);

		    const bool isBeingForced = 
			isFnInForceIncludeDescendants(kfuncCallee);
               
		    const bool leaveOutBecauseNotInFixedDescendants =
			usingFixedDescendants &&
			!isFnInFixedDescendants(kfuncCallee);

		    if (isBeingExcluded) {
			cout << " [excluding at user request]" << endl;
		    }
		    else if (leaveOutBecauseNotInFixedDescendants &&
			     !isBeingForced) {
			cout << " [skipping; not one of the fixed descendants]"
			     << endl;
		    }
#ifdef sparc_sun_solaris2_7
		    else if (kmgr.parseOutlinedLevel(kfuncCallee.getEntryAddr()) != rootFnGroupId) {
			cout << " [mis-matched group level; prob. not hot]"
			     << endl;
		    }
#endif
		    else {
			// Collect this guy's block counts
			cout << endl;
			pendingCollectRequests.push_back(
			    std::pair<pdstring, kapi_function>(calleeModName,
                                                               kfuncCallee));
		    }
		}
		else {
		    // already collected, or marked for collection at least...
		    cout << " [already collected]" << endl;
		}
	    }
	}
    }
    unsigned numToErase = 0;
    pdvector< std::pair<pdstring, kapi_function> >::const_iterator iter =
	pendingCollectRequests.begin();
    for (; numOutstandingCollectRequests < 10 &&
           iter != pendingCollectRequests.end(); iter++) {
	const pdstring& calleeModName = iter->first;
	const kapi_function& calleeFunc = iter->second;
	
	// pendingCollectRequests may have duplicates. Consult fnBlockCounters
	// to see if we processed the function yet
	if (!fnBlockCounters.defines(calleeFunc.getEntryAddr())) {
	    asyncCollectBlockCountsForFn(calleeModName, calleeFunc);
	}
	numToErase++;
    }
    if (numToErase > 0) {
	pendingCollectRequests.erase(0, numToErase-1);
    }
	
    if (pendingCollectRequests.size() == 0) {
	// We're all done collecting information rooted at "fn". Perhaps that
	// means that in the grand scheme of things, we're all done...perhaps
	// not. Check.
	checkForDoneWithBlockCountCollectingStage(kfn);
    }
}

void blockCounters::checkForDoneWithBlockCountCollectingStage(
    const kapi_function &/*lastBlkCntArrivalFn*/)
{
//     cout << "blockCounters: got block counts for fn \""
//          << lastBlkCntArrivalFn.getPrimaryName().c_str()
//          << "\"; checking to see if all done with block count collecting\n";
   
    for (dictionary_hash<kptr_t, outlining1FnCounts*>::const_iterator iter =
	     fnBlockCounters.begin(); iter != fnBlockCounters.end(); ++iter) {
	const outlining1FnCounts *c = *iter;
	if (!c->allDoneCollectingAndTellingAndUninstrumenting()) {
	    cout << "still waiting for some block counts to arrive..." << endl;
	    return;
	}
    }

    cout << "All done with the block count collection stage..." << endl;

    theOutliningMgr.doneCollectingBlockCounts(krootFn.getEntryAddr(),
					      fnBlockCounters,
					      forceIncludeDescendants);
}
