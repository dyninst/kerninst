// ctr64ComplexMetric.C

#include "util/h/kdrvtypes.h"
#include "ctr64ComplexMetric.h"
#include "dataHeap.h"

ctr64ComplexMetric::ctr64ComplexMetric(unsigned iid,
                                       unsigned iclusterid,
                                       const pdstring &metName,
                                       const pdstring &metDescription,
                                       const pdstring &metCurrentValUnitsName,
                                       const pdstring &metTotalValUnitsName,
                                       complexMetric::metric_normalized mn,
                                       complexMetric::aggregate_types at,
                                       const simpleMetric &theSimpleMetric) :
   complexMetric(iid, iclusterid, metName, metDescription,
                 metCurrentValUnitsName,
                 metTotalValUnitsName,
                 mn, at,
                 pdvector<const simpleMetric*>(1, &theSimpleMetric)) {
}

void ctr64ComplexMetric::calculateRegionsToSampleAndReport(
    complexMetricFocus &cmf,
    kapi_mem_region **regions,
    unsigned *pNumRegions) const
{
    const pdvector<const simpleMetricFocus*> &smfs = 
	cmf.getComponentSimpleMetricFocuses();
    assert(smfs.size() == 1);
    
    const simpleMetricFocus *add1smf = smfs[0];
   
    const pdvector<kptr_t> &uint8KernelAddrs = add1smf->getuint8KernelAddrs();
    assert(uint8KernelAddrs.size() == 1);
    kptr_t kernelAddr = uint8KernelAddrs[0];

    extern dataHeap *theGlobalDataHeap;
    const unsigned elemsPerVector = theGlobalDataHeap->getElemsPerVector8();
    const unsigned bytesPerStride = theGlobalDataHeap->getBytesPerStride8();

    kapi_mem_region *result = new kapi_mem_region[elemsPerVector];
    generateVectorToSample(kernelAddr, elemsPerVector, bytesPerStride, result);

    *regions = result;
    *pNumRegions = elemsPerVector;
}


double ctr64ComplexMetric::
interpretValForVisiShipment(const pdvector<sample_t> &vals,
                            unsigned /* sampling_cpu_mhz */) const {
   // This routine doesn't need to do much of interest...just return "val",
   // after converting it to a double (a necessary evil just before shipping a bucket
   // to a visi)
   
   assert(vals.size() == 1);
   return (double)vals[0];
}
