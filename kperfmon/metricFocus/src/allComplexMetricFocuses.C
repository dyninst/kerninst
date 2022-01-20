// allComplexMetricFocuses.C

#include "allComplexMetricFocuses.h"

allComplexMetricFocuses::allComplexMetricFocuses() :
   allComplexMetricFocusesHash(metric_focus_id_hash)
{
}

allComplexMetricFocuses::~allComplexMetricFocuses() {
   dictionary_hash<metric_focus_id, complexMetricFocus*>::const_iterator iter =
      allComplexMetricFocusesHash.begin();
   dictionary_hash<metric_focus_id, complexMetricFocus*>::const_iterator finish =
      allComplexMetricFocusesHash.end();

   while (iter != finish) {
      complexMetricFocus *mf = iter.currval();
      delete mf;
      ++iter;
   }

   allComplexMetricFocusesHash.clear();
}

bool allComplexMetricFocuses::anythingSubscribed() const {
   dictionary_hash<metric_focus_id, complexMetricFocus*>::const_iterator iter =
      allComplexMetricFocusesHash.begin();
   dictionary_hash<metric_focus_id, complexMetricFocus*>::const_iterator finish =
      allComplexMetricFocusesHash.end();

   while (iter != finish) {
      complexMetricFocus *mf = iter.currval();
      if (mf->anythingSubscribed())
         return true;
      ++iter;
   }
   return false;
}

metric_focus_id
allComplexMetricFocuses::
metricIdAndFocusId2MetricFocusId(unsigned metricid, unsigned focusid) const {
   metric_focus_id id(metricid, focusid);

   return id;
}

pair<unsigned, unsigned>
allComplexMetricFocuses::metricFocusId2MetricIdAndFocusId(const metric_focus_id &id) const {
   const pair<unsigned, unsigned> result = make_pair(id.metricid, id.focusid);
   
   assert(allComplexMetricFocusesHash.get(id)->getMetricAndFocusIds() == result);

   return result;
}

complexMetricFocus &
allComplexMetricFocuses::
createOrReturnExistingMetricFocus(const complexMetric &theMetric,
                                  const focus &theFocus,
                                  creation_states &theState
                                  ) {
   // Doesn't instrument the kernel
   // does this metric/focus already exist?

   const metric_focus_id id(theMetric.getId(), theFocus.getId());

   complexMetricFocus *mf;
   if (allComplexMetricFocusesHash.find(id, mf)) {
      theState = existsAndValid;
   }
   else {
      // Create new metric/focus pair
      mf = new complexMetricFocus(false, theMetric,
                                  theFocus);

      allComplexMetricFocusesHash.set(id, mf);
   
      theState = didNotExist;
   }
   
   return *mf;
}

pdvector<complexMetricFocus*>
allComplexMetricFocuses::closeDown1Subscriber(cmfSubscriber &s) {
   // returns a vector of complexMetricFocus' for which you should begin the process
   // of uninstrumenting, un-sampling, etc.
   dictionary_hash<metric_focus_id, complexMetricFocus*>::const_iterator iter =
      allComplexMetricFocusesHash.begin();
   dictionary_hash<metric_focus_id, complexMetricFocus*>::const_iterator finish =
      allComplexMetricFocusesHash.end();

   pdvector<complexMetricFocus*> result;
   
   while (iter != finish) {
      complexMetricFocus *mf = iter.currval();
      if (mf->unsubscribe_if(s))
         result += mf;
      ++iter;
   }

   return result;
}

void allComplexMetricFocuses::fry1(complexMetricFocus &mf) {
   const metric_focus_id id = mf.getMetricFocusId();

   delete allComplexMetricFocusesHash.get_and_remove(id);
}

