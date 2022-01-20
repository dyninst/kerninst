// veventsPerInvoc.h
// Base class for counted, virtualized complex metrics PER INVOCATION (that's
// the difference between this class and class vevents)

#ifndef _VEVENTS_PER_INVOC_H_
#define _VEVENTS_PER_INVOC_H_

#include "complexMetric.h"
#include "countedVEvents.h"
#include "add1AtExit.h"

class veventsPerInvoc : public complexMetric {
 private:
   const countedVEvents &theCountedVEventsSimpleMetric;
   const add1AtExit &theAdd1AtExitSimpleMetric;

   // prevent copying:
   veventsPerInvoc(const veventsPerInvoc &);
   veventsPerInvoc &operator=(const veventsPerInvoc &);

   // Required by the base class
   void calculateRegionsToSampleAndReport(complexMetricFocus &,
					  kapi_mem_region **regions,
					  unsigned *pNumRegions) const;
 public:
   veventsPerInvoc(unsigned iid, unsigned iclusterid,
                   const pdstring &metName,
                   const pdstring &metDescription,
                   const pdstring &currentValMetUnits,
                   const pdstring &totalValMetUnits,
                   complexMetric::metric_normalized imn,
                   complexMetric::aggregate_types iaggtype,
                   const countedVEvents &iCountedVEventsSimpleMetric,
                   const add1AtExit &iAdd1AtExitSimpleMetric) :
      complexMetric(iid, iclusterid,
                    metName, metDescription,
                    currentValMetUnits,
                    totalValMetUnits,
                    imn, iaggtype,
                    make2(iCountedVEventsSimpleMetric,
                          iAdd1AtExitSimpleMetric)),
      theCountedVEventsSimpleMetric(iCountedVEventsSimpleMetric),
      theAdd1AtExitSimpleMetric(iAdd1AtExitSimpleMetric) {}

   virtual ~veventsPerInvoc() {}

   // complexMetric provides satisfactory default versions for
   // modifiesPcr(), queryPostInstrumentationPcr(), and changeInPcrWouldInvalidateMF().
   
   virtual double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                              unsigned mhz) const = 0;
      // we can't provide a generic version that will satisfy everyone, because the
      // value that is actually sampled may be cycles (e.g. vtime) (in which case we
      // should divide by 1000000*mhz) or it may be an count (e.g. icache misses)
      // in which case we should do no such thing!
};

#endif
