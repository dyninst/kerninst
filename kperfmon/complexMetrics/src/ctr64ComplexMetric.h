// ctr64ComplexMetric.h
// A complex metric that can serve as a base class for entriesTo, exitsFrom, etc.
// This just exists to save some code: initialization, sampling code, number of
// buckets, and interpreting perfect sampled buckets are all the same across
// across entriesTo, exitsFrom, etc. type metrics

#ifndef _CTR64_COMPLEX_METRIC_H_
#define _CTR64_COMPLEX_METRIC_H_

#include "complexMetric.h"
#include "complexMetricFocus.h"

class ctr64ComplexMetric : public complexMetric {
 private:
   // prevent copying
   ctr64ComplexMetric(const ctr64ComplexMetric &);
   ctr64ComplexMetric& operator=(const ctr64ComplexMetric &);
   
   // required by the base class:
   void calculateRegionsToSampleAndReport(complexMetricFocus &,
					  kapi_mem_region **regions,
					  unsigned *pNumRegions) const;

 protected:
   double interpretValForVisiShipment(const pdvector<sample_t> &,
				      unsigned mhz) const;
      
 public:
   ctr64ComplexMetric(unsigned id, unsigned clusterid,
                      const pdstring &metName,
                      const pdstring &metDescription,
                      const pdstring &metCurrentValUnitsName,
                      const pdstring &metTotalValUnitsName,
                      complexMetric::metric_normalized,
                      complexMetric::aggregate_types,
                      const simpleMetric &theSimpleMetric);
   virtual ~ctr64ComplexMetric() {}
};


#endif
