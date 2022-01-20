// allComplexMetrics.h
// A simple database of all metrics

#ifndef _ALL_COMPLEX_METRICS_H_
#define _ALL_COMPLEX_METRICS_H_

#include "common/h/Vector.h"
#include "complexMetric.h"

class allComplexMetrics {
 private:
   pdvector<const complexMetric*> all; // the index is the metric id

   allComplexMetrics(const allComplexMetrics &);
   allComplexMetrics& operator=(const allComplexMetrics &);
   
 public:
   allComplexMetrics();
  ~allComplexMetrics();

   void add(const complexMetric *theMetric); // allocated w/ new

   unsigned size() const { return all.size(); }
   
   const complexMetric &getById(unsigned metricid) const;
   const complexMetric &operator[](unsigned metricid) const {
      return getById(metricid);
   }

   const complexMetric &find(const pdstring &metName) const;

   typedef pdvector<const complexMetric*>::const_iterator const_iterator;
   typedef pdvector<const complexMetric*>::iterator iterator;

   const_iterator begin() const { return all.begin(); }
//   iterator       begin() { return all.begin(); }
   const_iterator end() const { return all.end(); }
//   iterator       end() { return all.end(); }
};

#endif
