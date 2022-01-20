// complexMetricFocus.h
// This class maintains state information for an instantiated complex metric/focus
// A complex (as opposed to simple) metricFocus contains:
// 1) one or more simple metricFocus's (for the data & code)
// 2) A bucketer.
// 3) A list of cmfSubscriber's (such as visiSubscriber) that are subscribed to this
//    complexMetricFocus.

#ifndef _COMPLEX_METRIC_FOCUS_H_
#define _COMPLEX_METRIC_FOCUS_H_

#include "util/h/Dictionary.h"
#include "util/h/bucketer.h"
#include <algorithm> // stl's find()
#include "snippet.h"
#include "complexMetric.h"
#include "focus.h"
#include "cmfSubscriber.h"
#include "simpleMetricFocus.h"

// --------------------------------------------------------------------------------

struct metric_focus_id {
   unsigned metricid, focusid;
   metric_focus_id(unsigned imetricid, unsigned ifocusid) {
      metricid = imetricid;
      focusid = ifocusid;
   }
      
   bool operator==(const metric_focus_id &src) const {
      return (metricid == src.metricid && focusid == src.focusid);
   }
};

// --------------------------------------------------------------------------------

class complexMetric; // forward reference needed to avoid recursive #include
class sampleHandler;

class complexMetricFocus {
 private:
   const complexMetric &theMetric; // used in sampling
      // OK to store a ref; metrics aren't volatile since allMetrics stores a
      // vec of *ptrs*
   focus theFocus;

   sampleHandler &theHandler; // We receive samples from the handler

   pdvector< pdvector<sample_t> > trashableDeltaSampleValues;
      // size of outside vector equals the # of subscribers.
      // Then, each element is itself a vector, of the same size
      // as fragileDeltaSampleValuesToPassToBucketer.
   
   bool disabled;
   
   // cmfSubscriber's we're connected to:
   pdvector<cmfSubscriber*> subscribersToThisMFPair;
      // We forward sample data to these subscriber(s)
      // OK to store ptrs since globalSubscribers is a vec of ptrs

   bool unsubscribe(cmfSubscriber &, bool barfIfNotSubscribed);
      // used to uninstrument the kernel, but no longer

   pdvector<cmfSubscriber*>::const_iterator
   findSubscriber(cmfSubscriber &theSubscriber) const {
      // returns end() if not found
      return std::find(subscribersToThisMFPair.begin(),
                       subscribersToThisMFPair.end(),
                       &theSubscriber);
   }
   pdvector<cmfSubscriber*>::iterator
   findSubscriber(cmfSubscriber &theSubscriber) {
      // returns end() if not found
      return std::find(subscribersToThisMFPair.begin(),
                       subscribersToThisMFPair.end(),
                       &theSubscriber);
   }
      
   // disallow copying:
   complexMetricFocus(const complexMetricFocus &);
   complexMetricFocus& operator=(const complexMetricFocus &);

 public:
   complexMetricFocus(bool,
                      const complexMetric &,
                      const focus &,
		      sampleHandler &);
  ~complexMetricFocus();

   const complexMetric &getMetric() const { return theMetric; }
   const focus &getFocus() const { return theFocus; }
   sampleHandler& getSampleHandler() { return theHandler; }

   metric_focus_id getMetricFocusId() const {
      return metric_focus_id(theMetric.getId(), theFocus.getId());
   }
   std::pair<unsigned, unsigned> getMetricAndFocusIds() const {
      // pretty much the same as the above routine.
      return std::make_pair(theMetric.getId(), theFocus.getId());
   }

   bool anythingSubscribed() const;
   const pdvector<cmfSubscriber*> &getSubscribers() const {
      return subscribersToThisMFPair;
   }

   // Query each subscriber for its sampling rate, pick min of those
   unsigned getPresentSampleIntervalFromSubscribers() const;

   void disableMe();
   bool isDisabled() const {
      return disabled;
   }
   void unDisableMe();
      // re-genesis is still required

   const pdvector<const simpleMetricFocus *> &
   getComponentSimpleMetricFocuses() const;

   // subscribers expressing interest/disinterest in this metric/focus pair:
   bool subscribe(cmfSubscriber &, bool barfIfAlreadySubscribed);
      // used to instrument the kernel, but no longer

   bool unsubscribe(cmfSubscriber &s) {
      // Unsubscribe; if that was the last subscriber, then invalidate m/f
      // (fry it) and return true; else return false.
      return unsubscribe(s, true);
         // true --> barf if not subscribed by "s"
   }
   bool unsubscribe_if(cmfSubscriber &s) {
      // If "s" wasn't subscribed, do nothing.  Else unsubscribe; if that was the
      // last subscriber, then invalidate m/f (fry it) & return true; else
      // return false.
      return unsubscribe(s, false);
         // false --> no barf if not subscribed to by "s"
   }
   bool isSubscribedToBy(cmfSubscriber &) const;

   // Folding: metricFocuses don't fold, visis do.

   // Used for asserts only, right?
   uint64_t getGenesisTime() const;
   
   // Given a long vector of values for all CPUs and multiple simpleMetrics, 
   // produce a "double" value for our CPU
   double interpretAndAggregate(const pdvector<sample_t> &sampleVector);

   // Receive a vector of deltas from our sampleHandler and pass these
   // deltas (which are NOT bucketed) to our subscribers
   void handleDeltaSample(uint64_t startTime, uint64_t endTimePlus1,
			  const pdvector<sample_t> &deltaSamples);
};

#endif
