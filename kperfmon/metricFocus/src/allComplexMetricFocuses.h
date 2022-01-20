// allComplexMetricFocuses.h

#ifndef _ALL_COMPLEX_METRIC_FOCUSES_
#define _ALL_COMPLEX_METRIC_FOCUSES_

#include "complexMetricFocus.h"
#include "complexMetric.h"
#include "util/h/hashFns.h"

class cmfSubscriber;

class allComplexMetricFocuses {
 private:
   static unsigned metric_focus_id_hash(const metric_focus_id &id) {
      return addrHash(id.metricid) + addrHash(id.focusid);
   }
   dictionary_hash<metric_focus_id, complexMetricFocus*> allComplexMetricFocusesHash;
      // value: metricFocus, allocated with "new".  A pointer is best, so when the
      // dictionary resizes itself internally, references to a metricFocus don't
      // become dangling.
   
   allComplexMetricFocuses& operator=(const allComplexMetricFocuses &);
   allComplexMetricFocuses(const allComplexMetricFocuses &);
   
 public:
   allComplexMetricFocuses();
  ~allComplexMetricFocuses();

   const complexMetricFocus &get(const metric_focus_id &id) const {
      const complexMetricFocus *result = allComplexMetricFocusesHash.get(id);
      return *result;
   }
   complexMetricFocus &get(const metric_focus_id &id) {
      complexMetricFocus *result = allComplexMetricFocusesHash.get(id);
      return *result;
   }
   
   const complexMetricFocus &getByMetricIdAndFocusId(unsigned metricid,
                                                     unsigned focusid) const {
      const metric_focus_id id(metricid, focusid);
      return get(id);
   }
   complexMetricFocus &getByMetricIdAndFocusId(unsigned metricid, unsigned focusid) {
      const metric_focus_id id(metricid, focusid);
      return get(id);
   }
   
   bool anythingSubscribed() const;
      // returns true iff any of the m/f pairs is subscribed to by any cmfSubscriber

   metric_focus_id metricIdAndFocusId2MetricFocusId(unsigned metricid,
                                                    unsigned focusid) const;
   std::pair<unsigned, unsigned>
   metricFocusId2MetricIdAndFocusId(const metric_focus_id &id) const;

   enum creation_states {didNotExist, existsAndValid};
   complexMetricFocus &createOrReturnExistingMetricFocus(const complexMetric &,
                                                         const focus &,
                                                         creation_states &theState);

   bool subscribe(const metric_focus_id &id,
                  cmfSubscriber &s,
                  bool barfIfAlreadySubscribed) {
      // doesn't instrument the kernel.
      return allComplexMetricFocusesHash.get(id)->subscribe(s, barfIfAlreadySubscribed);
   }

   pdvector<complexMetricFocus*> closeDown1Subscriber(cmfSubscriber &);
      // returns a vector of complexMetricFocus' for which you should begin the process
      // of uninstrumenting, un-sampling, etc.

   void fry1(complexMetricFocus &);
      // outside world should call this only after the complexMetricFocus has been
      // successfully uninstrumented, un-sampled, etc., so that we can literally
      // call "delete" the complexMetricFocus structure.

   typedef dictionary_hash<metric_focus_id, complexMetricFocus*>::const_iterator const_iterator;
   const_iterator begin() {
      return allComplexMetricFocusesHash.begin();
   }
   const_iterator end() {
      return allComplexMetricFocusesHash.end();
   }
};

#endif
