#include <tcl.h>
#include "metricFocusManager.h"
#include "simpleMetricFocus.h"
#include "complexMetricFocus.h"
#include "allSimpleMetricFocuses.h"
#include "sampleHandler.h"
#include "dataHeap.h"
#include "common/h/Vector.h"
#include "util/h/hashFns.h"
#include "kapi.h"

extern kapi_manager kmgr;

class data_to_free {
private:
    pdvector<kptr_t> uint8KernelAddrs, timer16KernelAddrs, vtimerKernelAddrs;

public:
    data_to_free(const simpleMetricFocus &smf) :
	uint8KernelAddrs(smf.getuint8KernelAddrs()),
	timer16KernelAddrs(smf.gettimer16KernelAddrs()),
	vtimerKernelAddrs(smf.getvtimerKernelAddrs()) {
    }
    ~data_to_free() {}

    const pdvector<kptr_t> &getuint8KernelAddrs() const {
	return uint8KernelAddrs;
    }
    const pdvector<kptr_t> &gettimer16KernelAddrs() const {
	return timer16KernelAddrs;
    }
    const pdvector<kptr_t> &getvtimerKernelAddrs() const {
	return vtimerKernelAddrs;
    }
};

static void free_some_data(ClientData cd) 
{
    data_to_free *d = (data_to_free *)cd;

    const pdvector<kptr_t> &uint8KernelAddrs = d->getuint8KernelAddrs();
    const pdvector<kptr_t> &timer16KernelAddrs = d->gettimer16KernelAddrs();
    const pdvector<kptr_t> &vtimerKernelAddrs = d->getvtimerKernelAddrs();

    extern dataHeap *theGlobalDataHeap;

    for (pdvector<kptr_t>::const_iterator iter = uint8KernelAddrs.begin();
	 iter != uint8KernelAddrs.end(); ++iter)
	theGlobalDataHeap->unAlloc8(*iter);
    for (pdvector<kptr_t>::const_iterator iter = timer16KernelAddrs.begin();
	 iter != timer16KernelAddrs.end(); ++iter)
	theGlobalDataHeap->unAlloc16(*iter);
    for (pdvector<kptr_t>::const_iterator iter = vtimerKernelAddrs.begin();
	 iter != vtimerKernelAddrs.end(); ++iter)
	theGlobalDataHeap->unAllocVT(*iter);

    delete d;
    d = NULL; // help purify find mem leaks
}

metricFocusManager::metricFocusManager() :
    spliceRequestMap(splice_id_hash),
    sampleReqId2Handler(splice_id_hash), // same hash fn will do nicely
    handler2SampleReqId(sampleHandlerPtrHash),
    simpleMetricFocus2ItsSpliceReqIds(simpleMetricFocusPtrHash)
{
}

unsigned metricFocusManager::splice_id_hash(const unsigned &iid)
{
    // a static method
    unsigned id = iid;

    // should work well enough given that ids are simply increased by
    // 1 all of the time
    return id;
}

unsigned metricFocusManager::simpleMetricFocusPtrHash(
    const simpleMetricFocus * const &ptr) 
{
    // Since simpleMetricFocuses can't be copied (private copy ctor and 
    // operator=()), we can be assured that returning "ptr" is OK.
    return addrHash4((dptr_t)ptr); // util lib/hashFns.C
}

unsigned metricFocusManager::sampleHandlerPtrHash(
    const sampleHandler * const &ptr) 
{
    // sampleHandlers can't be copied (private copy ctor and operator=()),
    // so we can be assured that returning "ptr" is OK.
    return addrHash4((dptr_t)ptr); // util lib/hashFns.C
}

void metricFocusManager::spliceSimpleMetricFocus(const simpleMetricFocus &smf)
{
    pdvector<unsigned> spliceReqIdsForThisSMF;
   
    const unsigned numSnippets = smf.getNumSnippets();
    for (unsigned lcv=0; lcv < numSnippets; ++lcv) {
	std::pair<kptr_t, const snippet *> p = smf.getSnippetInfo(lcv);
	kptr_t launchAddr = p.first;
	const snippet *theSnippet = p.second;

	int reqid;
	kapi_snippet ksnippet = theSnippet->getKapiSnippet();

	kapi_point point;
	switch (theSnippet->getPlacement()) {
	case snippet::preinsn:
	    point = kapi_point(launchAddr, kapi_pre_instruction);
	    break;
	case snippet::prereturn:
	    point = kapi_point(launchAddr, kapi_pre_return);
	    break;
	default:
	    assert(false && "Invalid snippet placement");
	}
	if ((reqid = kmgr.insertSnippet(ksnippet, point)) < 0) {
	    abort();
	}
	spliceReqIdsForThisSMF += reqid;
	spliceRequestMap.set(reqid, std::make_pair(reqid, theSnippet->dup()));
    }

    simpleMetricFocus2ItsSpliceReqIds.set(&smf, spliceReqIdsForThisSMF);
}

void metricFocusManager::
spliceButDoNotSampleInstantiatedComplexMetricFocus(sampleHandler &theHandler) 
{
    // To splice/sample a cmf:
    // 1) splice all of the cmf's component smf's (where needed)
    // 2) download & periodically execute the sampling code

    const pdvector<const simpleMetricFocus*> &componentSMFs =
	theHandler.getComponentSimpleMetricFocuses();

    pdvector<const simpleMetricFocus*>::const_iterator smfiter = 
	componentSMFs.begin();
    pdvector<const simpleMetricFocus*>::const_iterator smffinish =
	componentSMFs.end();

    for (; smfiter != smffinish; ++smfiter) {
	const simpleMetricFocus *smf = *smfiter;
      
	if (smf->getSplicedRefCount() == 0)
	    // This simpleMetricFocus needs to be spliced into the kernel now.
	    spliceSimpleMetricFocus(*smf);

	smf->referenceSpliced();
	assert(smf->getSplicedRefCount() > 0);
    }
}

void metricFocusManager::
unSpliceAndStopSamplingComplexMetricFocus(const sampleHandler &theHandler,
                                          bool kperfmonIsGoingDownNow) 
{
    // Deallocation of ctrs & timers: for safety, that needs to wait until all of
    // the code has been uninstrumented.

    // Called by visualizationUser::StopMetricResource(), among others, to start
    // the process of un-instrumenting, un-sampling, and freeing of data used
    // by this metricFocus.

    const sampleHandler *shPtr = &theHandler;

    // Step 1: stop sampling.  It's a good idea to do this before unsplicing, to avoid
    // any final, stray samples that, because of the unsplice, may contain loony data.
    // On the other hand, since we're also removing from
    // sampleReqId2Handler, it's not really possible for this
    // "stray sample" situation to occur.  So the real reason that we stop sampling
    // before we unsplice is that I feel like it.
    // NEW: consider instrumenting multiple metric/resource combos at once.  Since we
    // now instrument all of them before emitting sampling code for any of them, it's
    // possible to enter this routine to disable a cmf whose sampling code has not
    // yet been emitted.  So we must now be tolerant of this situation.

    unsigned downloadedCodeId;
    if (handler2SampleReqId.find_and_remove(shPtr, downloadedCodeId)) {
	if (kmgr.stop_sampling(downloadedCodeId) < 0) {
	    assert(false && "stop_sampling error");
	}
//     assert(sampleReqId2Handler.get(downloadedCodeId) == cmfPtr);
	sampleReqId2Handler.undef(downloadedCodeId);
    }
    else
	assert(!sampleReqId2Handler.defines(downloadedCodeId));

   // Step 2: unsplice.

    const pdvector<const simpleMetricFocus*> &componentSMFs = 
	theHandler.getComponentSimpleMetricFocuses();
    // Since simpleMetricFocuses are independent of each other, we can unsplice
    // them independently.

    typedef pdvector<const simpleMetricFocus*> vsmf;
    vsmf componentSMFsToFry; // empty for now...
    for (vsmf::const_iterator smfiter = componentSMFs.begin();
	 smfiter != componentSMFs.end(); ++smfiter) {
	const simpleMetricFocus *smf = *smfiter;

	assert(smf->getSubscribedRefCount() >= smf->getSplicedRefCount());
	if (smf->dereferenceSpliced()) {
	    assert(smf->getSplicedRefCount() == 0);

	    // unsplice the snippets of this smf now (and free up data, after 3 sec delay)
	    unSpliceSimpleMetricFocus(*smf, kperfmonIsGoingDownNow);

	    // Let the simpleMetric do additional cleanup at this time, if desired.
	    // For example: vSimpleMetrics will wish to decrement a ref count and perhaps
	    // remove context switching instrumentation.
	    assert(smf->getSplicedRefCount() == 0);

	    const simpleMetric &theSimpleMetric = smf->getSimpleMetric();
	    theSimpleMetric.cleanUpAfterUnsplice(*smf);

	    assert(smf->getSplicedRefCount() == 0);
	}
	assert(smf->getSubscribedRefCount() >= smf->getSplicedRefCount());

	extern allSimpleMetricFocuses *globalAllSimpleMetricFocuses;
	if (globalAllSimpleMetricFocuses->unsubscribeTo(*smf))
	    componentSMFsToFry += smf; // we'll actually fry it later in this method
    }

    // Step 3: Inform the metric of the unsplice; perhaps it would like to do some
    // cleaning up now.  This is important.  For example, consider what a vmetric
    // would like to do here:
    // 1) remove the vtimer from the in-kernel vector of all vtimers, so the cswitch
    //    instrumentation, which has not yet been removed and might not be removed
    //    (if any other vtimers still active), doesn't try to stop & restart this
    //    timer any more.
    // 2) decrement a refcount for the downloaded-to-kernel "how-to-stop" and
    //    "how-to-restart" routines for this metric, and if zero, remove those
    //    routines from the kernel.  It's safe to do this now, because
    //    the vtimer has been removed from the in-kernel vector of all vtimers.  And
    //    by the way, there's no need for the customary 3-sec safety wait, because
    //    there's no way for us to interrupt the cswitchout/in routines, which is the
    //    only time the downloaded how-to-stop and how-to-restart routines are
    //    executed.

   // Fry the component smf's (actually, only if the smf's reference count
   // is now zero).
    for (vsmf::const_iterator smfiter = componentSMFsToFry.begin();
	 smfiter != componentSMFsToFry.end(); ++smfiter) {
	const simpleMetricFocus *smf = *smfiter;
	assert(smf->getSubscribedRefCount() >= smf->getSplicedRefCount());
	assert(smf->getSubscribedRefCount() == 0);
	assert(smf->getSplicedRefCount() == 0);

	extern allSimpleMetricFocuses *globalAllSimpleMetricFocuses;
	globalAllSimpleMetricFocuses->fryZeroReferencedSimpleMetricFocus(*smf);
    }
}

void
metricFocusManager::unSpliceSimpleMetricFocus(const simpleMetricFocus &smf,
					      bool kperfmonIsGoingDownNow) 
{
    // We un-splice in reverse order of splicing.  This turns out to be
    // necessary to avoid some nasty race conditions.  For example, consider
    // unsplicing vtimer start & stop code.  If we remove the stop code
    // first, then it's possible for two starts to be executed w/o any stops,
    // which is an assert failure.  Conversely, two stops is harmless (such a
    // check on stop is nothing new, for other reasons).  We note that the
    // real solution is "cooperative" code patches, where an atomic unsplice
    // can happen by simply changing a flag.

    const pdvector<unsigned> &spliceRequestIds = 
	simpleMetricFocus2ItsSpliceReqIds.get(&smf);
   
    // As mentioned above, we must iterate through spliceRequestIds in 
    // REVERSE order.
    if (spliceRequestIds.size() > 0) {
	pdvector<unsigned>::const_iterator first = spliceRequestIds.begin();
	pdvector<unsigned>::const_iterator iter = spliceRequestIds.end() - 1;

	unsigned num_processed = 0; // just for assert checking
      
	do {
	    const unsigned spliceRequestId = *iter;
	    kmgr.removeSnippet(spliceRequestId);

	    // remove entry from spliceRequestMap:
	    std::pair<kptr_t, snippet*> spliceReqInfo =
		spliceRequestMap.get_and_remove(spliceRequestId);
	    delete spliceReqInfo.second;
	    spliceReqInfo.second = NULL; // help purify find mem leaks
         
	    ++num_processed;
	} while (iter-- != first);
      
	assert(num_processed == spliceRequestIds.size());
    }

    assert(simpleMetricFocus2ItsSpliceReqIds.defines(&smf));
    simpleMetricFocus2ItsSpliceReqIds.undef(&smf);

    // After unsplicing the code, remove the data associated with the smf.
    // We wait 3 seconds, since the unspliced code could still be in execution.
    data_to_free *d = new data_to_free(smf);
    // the callback will delete what we have just new'd

    if (kperfmonIsGoingDownNow)
	free_some_data((ClientData)d);
    else
	(void)Tcl_CreateTimerHandler(3000, // millisecs (3 secs)
				     free_some_data,
				     (ClientData)d);
}

// Data-sampling callback. Defined in instrument.C
extern int kperfmonSamplingCallback(unsigned reqid, uint64_t time, 
				    const sample_t *values, 
                                    unsigned numvals);

void metricFocusManager::
reqGenesisEstablishingCMFSample(complexMetricFocus *cmf)
{
    // First it sends a request for an immediate "dummy" sample, which
    // will be used to establish a "genesis" time and value base for the
    // metricFocusBucketer.

    const complexMetric &theComplexMetric = cmf->getMetric();
    kapi_mem_region *regionsToSample; // the array will be allocated with new
    unsigned numRegions;

    theComplexMetric.calculateRegionsToSampleAndReport(*cmf, &regionsToSample,
						       &numRegions);
    // To start sampling we need to execute the sampling code once.
    // Only when the response arrives, will we start doing the code
    // periodically. I guess, that's done to not flood kperfmon with data
    // samples while we are still instrumenting other metrics
    int sampleReqIdUsed;
    if ((sampleReqIdUsed = kmgr.sample_periodically(regionsToSample, 
						    numRegions,
						    kperfmonSamplingCallback,
						    0 /* do once */)) < 0) {
	assert(false && "sample_periodically error");
    }
    delete[] regionsToSample;
    
    sampleHandler &theHandler = cmf->getSampleHandler();
    sampleReqId2Handler.set(sampleReqIdUsed, &theHandler);
    handler2SampleReqId.set(&theHandler, sampleReqIdUsed);
}

void metricFocusManager::reqPeriodicCMFSample(sampleHandler &sh,
					      unsigned initialRateMillisecs)
{
   assert(initialRateMillisecs != 0);
   const unsigned sampleReqId = handler2SampleReqId.get(&sh);
   
   if (kmgr.adjust_sampling_interval(sampleReqId, initialRateMillisecs) < 0) {
       assert(false && "adjust_sampling_interval error");
   }
}

void metricFocusManager::
changePeriodicCMFSampleInterval(sampleHandler &sh,
                                unsigned newSampleRateMillisecs)
{
   const unsigned sampleReqId = handler2SampleReqId.get(&sh);

   if (kmgr.adjust_sampling_interval(sampleReqId, newSampleRateMillisecs) < 0){
       assert(false && "adjust_sampling_interval error");
   }
}

void metricFocusManager::
removePeriodicCMFSampleInterval(sampleHandler &sh)
{
   // unusual for this to get called; unSpliceAndStopSamplingComplexMetricFocus
   // is the typical routine that stops cmf sampling.

   const unsigned sampleReqId = handler2SampleReqId.get(&sh);
      // this routine doesn't remove from cmf2SampleReqId, nor from
      // sapmleReqId2Handler; the idea being that sampling could be
      // resumed at any moment.  Uninstrumentation, after all, has *not*
      // taken place when this gets called.
   
   if (kmgr.adjust_sampling_interval(sampleReqId, 0 /* stop sampling */) < 0) {
       assert(false && "adjust_sampling_interval error");
   }
}

// Receive a sample for one cmf from a callback and pass it on to the
// appropriate sampleHandler
void metricFocusManager::handleOneSample(unsigned uniqSampleReqId, 
					 uint64_t time, 
                                         const sample_t *values,
					 unsigned numvalues)
{
    sampleHandler *shPtr = NULL;

    if (!sampleReqId2Handler.find(uniqSampleReqId, shPtr)) {
	// dout << "Received a sample for a non-existent reqId; ignoring\n";
    }
    else if (shPtr->isDisabled()) {
	// dout << "Received a sample for a disabled handler; ignoring\n";
    }
    else {
	shPtr->handleRawSample(time, values, numvalues);
	// bucket up value(s); if perfect bucket(s) are now available, 
	// will ship to subscriber(s) of this cmf (such as a visi or 
	// outlining mgr).
    }
}

