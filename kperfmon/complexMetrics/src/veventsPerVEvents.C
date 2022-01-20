// veventsPerVEvents.C

#include "util/h/kdrvtypes.h"
#include "countedVEvents.h"
#include "veventsPerVEvents.h"
#include "dataHeap.h"
#include "simpleMetricFocus.h"
#include "complexMetricFocus.h"

extern dataHeap *theGlobalDataHeap;

veventsPerVEvents::
veventsPerVEvents(unsigned iid, unsigned iclusterid,
                  const pdstring &metName,
                  const pdstring &metDescription,
                  const pdstring &currentValMetUnits,
                  const pdstring &totalValMetUnits,
                  complexMetric::metric_normalized imn,
                  complexMetric::aggregate_types iaggtype,
                  const countedVEvents &inumerator_smf,
                  const countedVEvents &idenominator_smf) :
      complexMetric(iid, iclusterid,
                    metName, metDescription,
                    currentValMetUnits,
                    totalValMetUnits,
                    imn, iaggtype,
                    make2(inumerator_smf, idenominator_smf)),
      numerator_smf(inumerator_smf),
      denominator_smf(idenominator_smf) {
}

void veventsPerVEvents::calculateRegionsToSampleAndReport(
    complexMetricFocus &cmf,
    kapi_mem_region **regions,
    unsigned *pNumRegions) const
{
   const pdvector<const simpleMetricFocus*> &componentSMFs = cmf.getComponentSimpleMetricFocuses();
   assert(componentSMFs.size() == 2);
   const pdvector<kptr_t> &addrs0 = componentSMFs[0]->getvtimerKernelAddrs();
   const pdvector<kptr_t> &addrs1 = componentSMFs[1]->getvtimerKernelAddrs();
   assert(addrs0.size() == 1 && addrs1.size() == 1);
   kptr_t kernelAddr0 = addrs0[0];
   kptr_t kernelAddr1 = addrs1[0];
   
   const unsigned elemsPerVector = theGlobalDataHeap->getElemsPerVectorVT();
   const unsigned bytesPerStride = theGlobalDataHeap->getBytesPerStrideVT();
   const unsigned numRegions = 2 * elemsPerVector; // 2 vectors

   kapi_mem_region *result = new kapi_mem_region[numRegions];
   generateVectorToSample(kernelAddr0, elemsPerVector, bytesPerStride, 
			  &result[0]);
   generateVectorToSample(kernelAddr1, elemsPerVector, bytesPerStride, 
			  &result[elemsPerVector]);
   *regions = result;
   *pNumRegions = numRegions;
}
