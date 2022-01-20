// veventsPerVTime.C

#include <sys/sysmacros.h> // roundup()
#include "util/h/kdrvtypes.h"
#include "veventsPerVTime.h"
#include "allSimpleMetricFocuses.h"
#include "complexMetricFocus.h"
#include "dataHeap.h"

extern dataHeap *theGlobalDataHeap;

void veventsPerVTime::calculateRegionsToSampleAndReport(
    complexMetricFocus &cmf,
    kapi_mem_region **regions,
    unsigned *pNumRegions) const
{
   const pdvector<const simpleMetricFocus*> &componentSMFs = cmf.getComponentSimpleMetricFocuses();
   assert(componentSMFs.size() == 2);
   
   const simpleMetricFocus *smf = componentSMFs[0];
   const pdvector<kptr_t> &kernelAddrs0 = smf->getvtimerKernelAddrs();
   assert(kernelAddrs0.size() == 1);
   kptr_t kernelAddr = kernelAddrs0[0];

   const unsigned elemsPerVector = theGlobalDataHeap->getElemsPerVectorVT();
   const unsigned bytesPerStride = theGlobalDataHeap->getBytesPerStrideVT();
   const unsigned numRegions = 2 * elemsPerVector; // 2 vectors
   
   kapi_mem_region *result = new kapi_mem_region[numRegions];
   generateVectorToSample(kernelAddr, elemsPerVector, bytesPerStride, result);

   smf = componentSMFs[1];
   const pdvector<kptr_t> &kernelAddrs1 = smf->getvtimerKernelAddrs();
   assert(kernelAddrs1.size() == 1);
   kernelAddr = kernelAddrs1[0];
   generateVectorToSample(kernelAddr, elemsPerVector, bytesPerStride, 
			  &result[elemsPerVector]);
   *regions = result;
   *pNumRegions = numRegions;
}
