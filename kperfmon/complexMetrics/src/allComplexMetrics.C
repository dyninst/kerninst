// allComplexMetrics.C

#include "allComplexMetrics.h"

allComplexMetrics::allComplexMetrics() {
   // empty vector for now
}

allComplexMetrics::~allComplexMetrics() {
   for (pdvector<const complexMetric*>::const_iterator iter = begin(); iter != end();
        ++iter) {
      const complexMetric *theMetric = *iter;
      delete theMetric;
   }
}

void allComplexMetrics::add(const complexMetric *theMetric) { // allocated w/ newx
   assert(all.size() == theMetric->getId());
   all += theMetric;
}

const complexMetric &allComplexMetrics::getById(unsigned metricid) const {
   return *all[metricid];
}

const complexMetric &allComplexMetrics::find(const pdstring &metName) const {
   pdvector<const complexMetric *>::const_iterator iter = all.begin();
   pdvector<const complexMetric *>::const_iterator finish = all.end();
   for (; iter != finish; ++iter) {
      const complexMetric *theMetric = *iter;
      if (theMetric->getName() == metName)
         return *theMetric;
   }
   
   assert(0);
}

