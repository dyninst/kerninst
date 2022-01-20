// veventsPerVTime.h
// Base class for the virtualized metrics that give some fraction of vtime.
// For example, the fraction of vtime spent in branch mispredictions.

#ifndef _VEVENTS_PER_VTIME_H_
#define _VEVENTS_PER_VTIME_H_

#include "complexMetric.h"
#include "countedVEvents.h"

class veventsPerVTime : public complexMetric {
 private:
   const countedVEvents &theCountedVEventsSimpleMetric;
   const countedVCycles &theCountedVCyclesSimpleMetric;

   // prevent copying:
   veventsPerVTime(const veventsPerVTime &);
   veventsPerVTime &operator=(const veventsPerVTime &);

   // Required by the base class
   void calculateRegionsToSampleAndReport(complexMetricFocus &,
					  kapi_mem_region **regions,
					  unsigned *pNumRegions) const;

 public:
   veventsPerVTime(unsigned iid, unsigned iclusterid,
                   const pdstring &metName,
                   const pdstring &metDescription,
                   const pdstring &currentValMetUnits,
                   const pdstring &totalValMetUnits,
                   complexMetric::metric_normalized imn,
                   complexMetric::aggregate_types iaggtype,
                   const countedVEvents &iCountedVEventsSimpleMetric,
                   const countedVCycles &iCountedVCyclesSimpleMetric) :
      complexMetric(iid, iclusterid,
                    metName, metDescription,
                    currentValMetUnits,
                    totalValMetUnits,
                    imn, iaggtype,
                    make2(iCountedVEventsSimpleMetric,
                          iCountedVCyclesSimpleMetric)),
      theCountedVEventsSimpleMetric(iCountedVEventsSimpleMetric),
      theCountedVCyclesSimpleMetric(iCountedVCyclesSimpleMetric) {
   }
   virtual ~veventsPerVTime() {}

   // complexMetric provides satisfactory default versions of
   // modifiesPcr(), queryPostInstrumentationPcr(), and changeInPcrWouldInvalidateMF().

   virtual double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                              unsigned mhz) const = 0;
      // we can't provide a generic version that will satisfy everyone, because the
      // value that is actually sampled may be cycles (e.g. vtime) (in which case we
      // should divide by 1000000*mhz) or it may be an count (e.g. icache misses)
      // in which case we should do no such thing!
};

#endif
