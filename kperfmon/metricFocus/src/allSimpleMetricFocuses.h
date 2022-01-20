// allSimpleMetricFocuses.h
// This class maintains the set of all simpleMetricFocus' (indexed by
// the pair: simple metric id and focus id)

#ifndef _ALL_SIMPLE_METRIC_FOCUSES_H_
#define _ALL_SIMPLE_METRIC_FOCUSES_H_

#include "util/h/Dictionary.h"
#include "util/h/hashFns.h"
#include "simpleMetricFocus.h"

class allSimpleMetricFocuses {
 private:
   dictionary_hash< std::pair<unsigned,unsigned>, simpleMetricFocus*> allSMFs;
      // key: .first: simpleMetric id   .second: focus id
      // value: simple metric focus, allocated with new

   // prevent copying:
   allSimpleMetricFocuses(const allSimpleMetricFocuses &);
   allSimpleMetricFocuses& operator=(const allSimpleMetricFocuses);
   
 public:
   allSimpleMetricFocuses();
  ~allSimpleMetricFocuses();
      // asserts allSMFs is empty -- very strong assert

   simpleMetricFocus *subscribeTo(const simpleMetric &, const focus &);
   bool unsubscribeTo(const simpleMetricFocus &);
      // returns true iff reference count is now zero.  Doesn't fry even if refCount
      // is now zero; this gives you a chance to do some cleanup using the
      // contents of the smf.

   void fryZeroReferencedSimpleMetricFocus(const simpleMetricFocus &);
};

#endif
