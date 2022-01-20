// veventsPerInvoc.C

#include <sys/sysmacros.h> // roundup()
#include "util/h/kdrvtypes.h"
#include "veventsPerInvoc.h"
#include "allSimpleMetricFocuses.h"
#include "dataHeap.h"
#include "complexMetricFocus.h"

extern dataHeap *theGlobalDataHeap;

void veventsPerInvoc::calculateRegionsToSampleAndReport(
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

   unsigned elemsPerVector = theGlobalDataHeap->getElemsPerVectorVT();
   unsigned bytesPerStride = theGlobalDataHeap->getBytesPerStrideVT();
   unsigned numRegions = 2 * elemsPerVector; // 2 vectors

   kapi_mem_region *result = new kapi_mem_region[numRegions];
   generateVectorToSample(kernelAddr, elemsPerVector, bytesPerStride, result);
   
   smf = componentSMFs[1];
   const pdvector<kptr_t> &kernelAddrs1 = smf->getuint8KernelAddrs();
   assert(kernelAddrs1.size() == 1);
   kernelAddr = kernelAddrs1[0];

   assert(elemsPerVector == theGlobalDataHeap->getElemsPerVector8());
   bytesPerStride = theGlobalDataHeap->getBytesPerStride8();
   generateVectorToSample(kernelAddr, elemsPerVector, bytesPerStride, 
			  &result[elemsPerVector]); // second vector goes here
   *regions = result;
   *pNumRegions = numRegions;
}
