// vevents.h
// A base class for counted, virtualized complex metrics.
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _VEVENTS_H_
#define _VEVENTS_H_

#include "complexMetric.h"
#include "countedVEvents.h" // the simple metric that we'll use is "class threadTicks"

class vevents : public complexMetric {
 private:
   const countedVEvents &theCountedVEventsSimpleMetric;
      // a base class...the actual simple metric will be something
      // that is derived from this.
   
   // prevent copying:
   vevents(const vevents &);
   vevents& operator=(const vevents &);

   // Required by the base class
   void calculateRegionsToSampleAndReport(complexMetricFocus &,
					  kapi_mem_region **regions,
					  unsigned *pNumRegions) const;
   
 protected:
   const countedVEvents &getCountedVEventsSimpleMetric() const {
      return theCountedVEventsSimpleMetric;
   }
      
 public:
   vevents(unsigned iid, unsigned iclusterid,
           const pdstring &metName,
           const pdstring &metDescription,
           const pdstring &currentValMetUnits,
           const pdstring &totalValMetUnits,
           complexMetric::metric_normalized imn,
           complexMetric::aggregate_types iaggtype,
           const countedVEvents &iCountedVEventsSimpleMetric) :
      complexMetric(iid, iclusterid,
                    metName, metDescription,
                    currentValMetUnits,
                    totalValMetUnits,
                    imn, iaggtype,
                    pdvector<const simpleMetric*>(1, &iCountedVEventsSimpleMetric)),
      theCountedVEventsSimpleMetric(iCountedVEventsSimpleMetric) {
   }
   virtual ~vevents() { }

   // class complexMetric provides satisfactory default versions of
   // modifiesPcr(), queryPostInstrumentationPcr(), and changeInPcrWouldInvalidateMF().
   
   virtual double interpretValForVisiShipment(const pdvector<sample_t> &vals,
                                              unsigned mhz) const = 0;
      // we can't provide a generic version that will satisfy everyone, because the
      // value that is actually sampled may be cycles (e.g. vtime) (in which case we
      // should divide by 1000000*mhz) or it may be an count (e.g. icache misses)
      // in which case we should do no such thing!
};

#endif
