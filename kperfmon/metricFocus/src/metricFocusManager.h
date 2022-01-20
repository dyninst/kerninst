#ifndef _METRIC_FOCUS_MANAGER_H_
#define _METRIC_FOCUS_MANAGER_H_

#include <utility>
#include "util/h/kdrvtypes.h"
#include "util/h/Dictionary.h"
#include "kapi.h"

class simpleMetricFocus;
class complexMetricFocus;
class sampleHandler;
class snippet;

class metricFocusManager {
    // would prefer snippet& to snippet* but c++ can't have vectors of
    // refs. Don't worry about the snippet* pointing to dangling memory when
    // the vector of all metricFocus pairs gets resized, because that vector
    // is a vector of ptrs.  Perhaps we only need to store a ptr to the
    // owning metricFocus; the pair<> perhaps isn't needed?
    dictionary_hash<unsigned, std::pair<kptr_t, snippet *> > spliceRequestMap;
    
    dictionary_hash<unsigned, sampleHandler*> sampleReqId2Handler;
    dictionary_hash<const sampleHandler*, unsigned> handler2SampleReqId;

    // In theory, this info could be kept in the simpleMetricFocus itself,
    // but that would miss the point of isolating that class from the details
    // of igen.
    dictionary_hash<const simpleMetricFocus *, pdvector<unsigned> >
	simpleMetricFocus2ItsSpliceReqIds;

    void spliceSimpleMetricFocus(const simpleMetricFocus &);
    void unSpliceSimpleMetricFocus(const simpleMetricFocus&,
				   bool kperfmonIsGoingDownNow);
    // prevent copying:
    metricFocusManager(const metricFocusManager &);
    metricFocusManager& operator=(const metricFocusManager &);

    static unsigned splice_id_hash(const unsigned &id);
    static unsigned simpleMetricFocusPtrHash(const simpleMetricFocus * const&);
    static unsigned sampleHandlerPtrHash(const sampleHandler * const &ptr);
    
 public:
    metricFocusManager();

    void spliceButDoNotSampleInstantiatedComplexMetricFocus(sampleHandler &);

    // note that the param is a const; we don't fry or even modify the
    // metricFocus; we just get some info from it...presumably, the caller to
    // this routine will indeed fry the metricFocus structure quite soon, but
    // that's another story.  This routine unsplices, stops sampling, and
    // arranges for data to be deleted after a 3 second safety delay.  In
    // addition, it informs the parent metric that an unsplice of one of
    // "its" metricFocus' is being uninstrumented; perhaps the metric wishes
    // to do some cleaning up (so far just the vtimer metrics have any
    // cleaning up to do).
    void unSpliceAndStopSamplingComplexMetricFocus(const sampleHandler &,
						   bool kperfmonIsGoingDown);
   
    // sends request and stores an entry into sampleReqId2ComplexMetricFocusMap
    // NEW 07/2000: first it sends a request for an immediate "dummy" sample,
    // which will be used to establish a "genesis" time and value base for
    // the metricFocusBucketer.
    void reqGenesisEstablishingCMFSample(complexMetricFocus *cmf);
    void reqPeriodicCMFSample(sampleHandler &sh, unsigned initialRateMs);
    void changePeriodicCMFSampleInterval(sampleHandler &,
					 unsigned newSampleRateMillisecs);

    // unusual for this to get called
    // unSpliceAndStopSamplingComplexMetricFocus is the typical routine
    // that stops cmf sampling.
    void removePeriodicCMFSampleInterval(sampleHandler &);

   // Receive a sample for one cmf from a callback and pass it on to the
   // appropriate sampleHandler
   void handleOneSample(unsigned reqid, uint64_t time, 
			const sample_t *values, unsigned numvalues);
};

#endif
