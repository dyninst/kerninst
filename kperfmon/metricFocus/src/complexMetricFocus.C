// complexMetricFocus.C

#include "complexMetricFocus.h"
#include "sampleHandler.h"
#include "util/h/hashFns.h"
#include "kapi.h"

#include <iostream.h>
#include <iomanip.h>
//using namespace std;

extern kapi_manager kmgr;

unsigned cycles2millisecs(uint64_t cycles, unsigned mhzTimes1000) {
   // (not static since also used by visiSubscriber.C)

   // "cycles" cycles * (1 sec / mhz*million cycles) * (1000 millisec / sec)
   // yields ("cycles" / (1000 * mhz)) millisecs
   uint64_t result = cycles / mhzTimes1000;
   return result;
}

uint64_t millisecs2cycles(unsigned msecs, unsigned mhzTimes1000) {
   return (uint64_t)msecs * mhzTimes1000;
}

// ----------------------------------------------------------------------

complexMetricFocus::complexMetricFocus(bool,
                                       const complexMetric &iTheMetric,
                                          // OK to pass a ref (won't dangle)
                                          // thanks to allMetrics storing ptrs
                                       const focus &iTheFocus,
				       sampleHandler &iTheHandler
                                       ) :
               theMetric(iTheMetric), theFocus(iTheFocus), 
	       theHandler(iTheHandler)
    // trashableDeltaSampleValues will be an empty vector, since
    // there are as yet no subscribers.
{
   disabled = false;
}

complexMetricFocus::~complexMetricFocus() {
   // Since m/f pairs are kept around as long as possible, this dtor isn't called until
   // KernInst shuts down, right?

   assert(subscribersToThisMFPair.size() == 0);
   assert(trashableDeltaSampleValues.size() == 0);
}

// Query each subscriber for its sampling rate, pick min of those
unsigned complexMetricFocus::getPresentSampleIntervalFromSubscribers() const {
    uint64_t newSampleInterval = 0;
      
    pdvector<cmfSubscriber*>::const_iterator siter = 
	subscribersToThisMFPair.begin();
    pdvector<cmfSubscriber*>::const_iterator sfinish = 
	subscribersToThisMFPair.end();
    for (; siter != sfinish; ++siter) {
	const cmfSubscriber *s = *siter;

	// currently, all subscribers (yes, even outlining subscribers)
	// have a method called "getPresentSampleIntervalInCycles()" 
	// [though the outlining subscriber is reportedly grumpy about 
	// having to define such a method, since it dislikes the idea of 
	// a periodic sampling interval, preferring to call it a one-time 
	// sample].

	const uint64_t thisSubscriberInterval = 
	    s->getPresentSampleIntervalInCycles();
	if (thisSubscriberInterval != 0 &&
	    (newSampleInterval == 0 || 
	     thisSubscriberInterval < newSampleInterval)) {
	    newSampleInterval = thisSubscriberInterval;
	}
    }

    const unsigned mhzTimes1000 = kmgr.getSystemClockMHZ() * 1000;

    const unsigned newSampleIntervalMillisecs = 
	cycles2millisecs(newSampleInterval, mhzTimes1000);

    return newSampleIntervalMillisecs;
}

bool complexMetricFocus::anythingSubscribed() const {
   assert(trashableDeltaSampleValues.size() == subscribersToThisMFPair.size());
   return subscribersToThisMFPair.size() > 0;
}

void complexMetricFocus::disableMe() {
    assert(!disabled);
    disabled = true;

    pdvector<cmfSubscriber*>::const_iterator isub = 
	subscribersToThisMFPair.begin();
    pdvector<cmfSubscriber*>::const_iterator endsub = 
	subscribersToThisMFPair.end();

    for (; isub != endsub; isub++) {
	cmfSubscriber *sub = *isub;
	sub->disableMetricFocusPair(*this);
    }
}

void complexMetricFocus::unDisableMe() {
    // re-instrumentation has not yet occurred.  re-genesis still needs
    // to occur, by the way.  Can it happen in this routine?  I don't think
    // so, since, because re-instrumentation hasn't yet taken place, we can't
    // possibly have the necessary initial sample.

    // Things should still be as they were for disableMe(); assert it:
    assert(disabled);

    disabled = false;

    const unsigned mhz = kmgr.getSystemClockMHZ();

    pdvector<cmfSubscriber*>::const_iterator isub = 
	subscribersToThisMFPair.begin();
    pdvector<cmfSubscriber*>::const_iterator endsub = 
	subscribersToThisMFPair.end();

    for (; isub != endsub; isub++) {
	cmfSubscriber *sub = *isub;
	sub->reEnableDisabledMetricFocusPair(*this, mhz);
    }
    // See comment in disableMe() on why I don't think that we should try to
    // call undisable on the component simpleMetricFocuses.
}

bool complexMetricFocus::subscribe(cmfSubscriber &s, bool barfIfAlreadySubscribed) {
   // Doesn't instrument the kernel
   // returns false iff nothing done
   // first, assert that it isn't already subscribed

   bool subscribed = isSubscribedToBy(s);
   if (subscribed)
      if (barfIfAlreadySubscribed)
         assert(false && "already subscribed to this metric/focus pair");
      else
         return false;
   
   subscribersToThisMFPair += &s;
   pdvector<sample_t> *ptr = trashableDeltaSampleValues.append_with_inplace_construction();
   new((void*)ptr)pdvector<sample_t>(theMetric.getNumberValuesToBucket(), 0);

   pdvector< pdvector<sample_t> >::const_iterator iter = trashableDeltaSampleValues.begin();
   pdvector< pdvector<sample_t> >::const_iterator finish = trashableDeltaSampleValues.end();
   for (; iter != finish; ++iter) {
      const pdvector<sample_t> &v = *iter;
      assert(v.size() == theMetric.getNumberValuesToBucket());
   }

   theHandler.recalcPresentSampleIntervalFromSubscribers();
      // the genesis sample may not have arrived yet, in which case we'll do nothing

   return true;
}

bool complexMetricFocus::unsubscribe(cmfSubscriber &s, bool barfIfNotSubscribed) {
   // Returns true iff this m/f now has no subscribers to it, so you should
   // begin the process of uninstrumenting, un-sampling, etc. this complexMetricFocus.

   // Is "s" subscribed to this m/f pair?
   pdvector<cmfSubscriber*>::iterator s_iter = findSubscriber(s);
   if (s_iter == subscribersToThisMFPair.end())
      if (barfIfNotSubscribed)
         assert(false && "complexMetricFocus::unsubscribe() -- not subscribed to this metric-focus?");
      else
         return false;

   if (!isDisabled())
      s.subscriberHasRemovedCurve(*this); // will proabably just fry bucketer

   // remove from vector
   assert(subscribersToThisMFPair.size() > 0);
      // else back() and pop_back() will do weird things
   *s_iter = subscribersToThisMFPair.back();
   subscribersToThisMFPair.pop_back();

   trashableDeltaSampleValues.pop_back();
   assert(trashableDeltaSampleValues.size() == subscribersToThisMFPair.size());

   // NOTE: We do *not* call delete on the cmfSubscriber.

   theHandler.recalcPresentSampleIntervalFromSubscribers();
      // the genesis sample may not have arrived yet, in which case we'll do nothing
   
   return (subscribersToThisMFPair.size() == 0);
}

bool complexMetricFocus::isSubscribedToBy(cmfSubscriber &s) const {
   pdvector<cmfSubscriber*>::const_iterator s_iter = findSubscriber(s);
   return (s_iter != subscribersToThisMFPair.end());
}

// ----------------------------------------------------------------------

// Receive a vector of deltas from our sampleHandler and pass these
// deltas (which are NOT bucketed) to our subscribers (such as 
// visi(s)/outliningMgr)
void complexMetricFocus::
handleDeltaSample(uint64_t startTime, uint64_t endTimePlus1,
		  const pdvector<sample_t> &deltaSamples) 
{
    // CAUTION: as an enabling technology, the various bucketer
    // add()-type routines are allowed to (and indeed do) trash the contents
    // of the passed-in data values.  Thus we must be very careful, lest only
    // the first subscriber receive correct values!

    assert(subscribersToThisMFPair.size() > 0);
    assert(trashableDeltaSampleValues.size()==subscribersToThisMFPair.size());
   
    pdvector<cmfSubscriber*>::const_iterator siter = 
	subscribersToThisMFPair.begin();
    pdvector<cmfSubscriber*>::const_iterator sfinish = 
	subscribersToThisMFPair.end();
    pdvector<sample_t> *tditer = trashableDeltaSampleValues.begin();
    for (; siter != sfinish; ++siter, ++tditer) {
	cmfSubscriber *s = *siter;
	assert(s);

	pdvector<sample_t> &trashableDeltas = *tditer;
	assert(trashableDeltas.size() == deltaSamples.size());

	// Here we make a copy of the data.  Underlying allocation won't
	// be needed (and it better not be, or our whole effort of 
	// preallocating the variable trashableDeltas[] has gone to waste).
	trashableDeltas = deltaSamples;
      
	s->add(*this, startTime, endTimePlus1, trashableDeltas);
    }
}

// Given a vector of values of all simple metrics on all CPUs, produce
// the final value of the metric for a given CPU or CPU-aggregate
// - The vector stores values of the first simple metric on all CPUs, 
//   followed by the second SM on all CPUs, ...
double complexMetricFocus::
interpretAndAggregate(const pdvector<sample_t> &sampleVector)
{
    const unsigned numSimple = theHandler.getNumSimpleComponents();
    const unsigned numIds = sampleVector.size() / numSimple;
    const unsigned numCPUs = kmgr.getNumCPUs();
    
    assert(numIds == kmgr.getMaxCPUId() + 1);
    assert(numIds * numSimple == sampleVector.size());
    assert(numCPUs != 0);

    const int cpuid = theFocus.getCPUId();

    pdvector<sample_t> tuple(numSimple, 0);
    unsigned i, j, k;
    double rv = 0.0;

    if (cpuid >= FIRST_REAL_CPU) { // We are focused on a particular CPU
	const int physId = kmgr.getIthCPUId(cpuid);
	assert((unsigned)physId < numIds);
	k = physId;
	for (j=0; j<numSimple; j++) {
	    tuple[j] = sampleVector[k];
	    k += numIds;
	}
	const unsigned mhz = kmgr.getCPUClockMHZ(cpuid);
    
	rv = theMetric.interpretValForVisiShipment(tuple, mhz);
    }
    else if (cpuid == CPU_TOTAL) { // Aggregate the values across CPUs
	k = 0;
	for (j=0; j<numSimple; j++) {
	    for (i=0; i<numIds; i++) {
		tuple[j] += sampleVector[k++];
	    }
	}
	// We should probably compute AverageMHZ
	const unsigned mhz = kmgr.getCPUClockMHZ(0);
    
	rv = theMetric.interpretValForVisiShipment(tuple, mhz);
    }
    else {
	assert(false && "Unknown cpu id");
    }

    return rv;
}

const pdvector<const simpleMetricFocus *>& complexMetricFocus::
getComponentSimpleMetricFocuses() const 
{
    return theHandler.getComponentSimpleMetricFocuses();
}

uint64_t complexMetricFocus::getGenesisTime() const
{
    return theHandler.getGenesisTime();
}
