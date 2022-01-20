#ifndef _SAMPLE_HANDLER_H_
#define _SAMPLE_HANDLER_H_

#include "focus.h"
#include "complexMetric.h"
#include "complexMetricFocus.h"
#include "metricFocusManager.h"

struct sample_handler_id {
   unsigned metricid, focusid;
   sample_handler_id(unsigned imetricid, unsigned ifocusid) {
      metricid = imetricid;
      focusid = ifocusid;
   }
      
   bool operator==(const sample_handler_id &src) const {
      return (metricid == src.metricid && focusid == src.focusid);
   }
};

class sampleHandler {
 private:
    const complexMetric &theMetric;
    focus fuzzyFocus;

    // Sampled buckets for this m/f instantiation:
    uint64_t genesisTime; // 0 --> genesis hasn't yet occurred

    // 0 --> undefined (for assertion checking).  Defined during "genesis"
    // to be genesisTime-1.
    uint64_t lastTimeOfSample;
    
    unsigned presentInterval;

    // running totals.  Size of the vector is initialized right away, but
    // the actual contents aren't initialized until "genesis" time.
    pdvector<sample_t> lastSampleValues;

    // Fixed-size that is filled with (runningTotals - lastSampleValues)
    pdvector<sample_t> deltaSamples;

    // Metric-focuses that are subscribed to this handler
    pdvector<complexMetricFocus*> theMetricFocuses;

    // Individual components of this complexMetric, we get these from the 
    // global dictionary of SMF's
    pdvector<const simpleMetricFocus*> componentSMFs;

    // When two handlers conflict on the use of %pic, we disable one of
    // them (the old one)
    bool disabled;

    // disallow copying:
    sampleHandler(const sampleHandler &);
    sampleHandler& operator=(const sampleHandler &);

 public:
    sampleHandler(const complexMetric &aMetric,
		  const focus &aFocus) : 
	theMetric(aMetric), fuzzyFocus(aFocus),
	genesisTime(0), lastTimeOfSample(0), presentInterval(0),
	lastSampleValues(aMetric.getNumberValuesToBucket(), 0),
	deltaSamples(aMetric.getNumberValuesToBucket(), 0),
	disabled(false) {

	assert(fuzzyFocus.getCPUId() == FUZZY);
    }

    ~sampleHandler() {
	assert(theMetricFocuses.size() == 0);
    }

    const complexMetric &getMetric() const { 
	return theMetric; 
    }

    const focus &getFocus() const { 
	return fuzzyFocus; 
    }

    sample_handler_id getSampleHandlerId() const {
	return sample_handler_id(theMetric.getId(), fuzzyFocus.getId());
    }

    bool isReferenced() {
	return (theMetricFocuses.size() > 0);
    }

    bool isDisabled() {
	return disabled;
    }

    void addComponentSimpleMetricFocus(const simpleMetricFocus *smf) {
	componentSMFs += smf;
    }

    const pdvector<const simpleMetricFocus *> &
    getComponentSimpleMetricFocuses() const {
	return componentSMFs;
    }
    
    unsigned getNumSimpleComponents() const {
	return componentSMFs.size();
    }

    // Has the first sample arrived?
    bool hasGenesisOccurredYet() const {
	return genesisTime != 0;
    }

    uint64_t getGenesisTime() const {
	assert(hasGenesisOccurredYet());
	return genesisTime;
    }

    // Given our theMetric and provided theFocus, find the corresponding
    // CMF or create one, if nothing found
    complexMetricFocus& findOrCreateMetricFocus(const focus& theFocus);

    // Check if CMF specified by (metricid, focusid) is subscribed to us
    complexMetricFocus* findMetricFocus(unsigned metricid, unsigned focusid);

    // Unsubscribe cmf from this handler. Do not uninstrument the kernel
    // even if it was the last metric
    void removeMetric(complexMetricFocus *cmf);

    // Unsplice and stop sampling
    void uninstrument(bool goingDown);
    
    // Unsubscribe all cmf's that feed only theSubscriber
    void frySubscribedMetrics(cmfSubscriber *theSubscriber);

    // When two handlers conflict on the use of %pic, we disable one of
    // them (the old one)
    void disableMe();

    // Is is ever called?
    void unDisableMe();

    // Receive the first sample, initialize our data structures and
    // start periodic sampling
    void doGenesisNow(uint64_t cmfGenesisTime,
		      const sample_t *initialRunningTotals,
		      unsigned numvalues);

    // All samples arrive here. We convert them to deltas and pass to
    // the complexMetrics subscribed to us
    void handleRawSample(uint64_t timeOfSample, 
			 const sample_t *runningTotals, unsigned numvalues);

    // Ask all subscribers about their sampling interval, pick the 
    // minimum value and adjust the sampling rate in kerninstd
    void recalcPresentSampleIntervalFromSubscribers();
};
#endif
