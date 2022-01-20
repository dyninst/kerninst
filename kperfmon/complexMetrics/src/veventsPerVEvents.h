// veventsPerVEvents.h
// Base class for the virtualized metrics that give some ratio of vevents to
// another vevents.  For example, the icache hit ratio.

#ifndef _VEVENTS_PER_VEVENTS_H_
#define _VEVENTS_PER_VEVENTS_H_

#include "complexMetric.h"
class countedVEvents;
class complexMetricFocus;

class veventsPerVEvents : public complexMetric {
 private:
   const countedVEvents &numerator_smf;
   const countedVEvents &denominator_smf;

   // prevent copying:
   veventsPerVEvents(const veventsPerVEvents &);
   veventsPerVEvents &operator=(const veventsPerVEvents &);

   // Required by the base class
   void calculateRegionsToSampleAndReport(complexMetricFocus &,
					  kapi_mem_region **regions,
					  unsigned *pNumRegions) const;

 public:
   veventsPerVEvents(unsigned iid, unsigned iclusterid,
                     const pdstring &metName,
                     const pdstring &metDescription,
                     const pdstring &currentValMetUnits,
                     const pdstring &totalValMetUnits,
                     complexMetric::metric_normalized imn,
                     complexMetric::aggregate_types iaggtype,
                     const countedVEvents &inumerator_smf,
                     const countedVEvents &idenominator_smf);
   virtual ~veventsPerVEvents() {}

   virtual double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                              unsigned mhz) const = 0;
};

#endif
