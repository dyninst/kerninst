// vevents.C

#include "util/h/kdrvtypes.h"
#include "vevents.h"
#include "allSimpleMetricFocuses.h"
#include "dataHeap.h"
#include "complexMetricFocus.h"

void vevents::calculateRegionsToSampleAndReport(
    complexMetricFocus &cmf,
    kapi_mem_region **regions,
    unsigned *pNumRegions) const
{
   // Needs to be public since it's invoked via a callback routine.
   // The info that this routine returns will be passed as the 3d argument
   // to emitSamplingCode().
   const pdvector<const simpleMetricFocus*> &smfs = cmf.getComponentSimpleMetricFocuses();
   assert(smfs.size() == 1);
   
   const simpleMetricFocus *smf = smfs[0];
   
   const pdvector<kptr_t> &vtimerKernelAddrs = smf->getvtimerKernelAddrs();
   assert(vtimerKernelAddrs.size() == 1);
   kptr_t kernelAddr = vtimerKernelAddrs[0];

   extern dataHeap *theGlobalDataHeap;
   const unsigned elemsPerVector = theGlobalDataHeap->getElemsPerVectorVT();
   const unsigned bytesPerStride = theGlobalDataHeap->getBytesPerStrideVT();

   kapi_mem_region *result = new kapi_mem_region[elemsPerVector];
   generateVectorToSample(kernelAddr, elemsPerVector, bytesPerStride, result);

   *regions = result;
   *pNumRegions = elemsPerVector;
}
