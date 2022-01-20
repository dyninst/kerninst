// vinsnsPerVCycle.C

#include "util/h/kdrvtypes.h"
#include "complexMetric.h"
#include "countedVEvents.h" // needed since in .h file, they were just fwd decl'd
#include "vinsnsPerVCycle.h"
#include "complexMetricFocus.h"
#include "allSimpleMetricFocuses.h"
#include "dataHeap.h"
#ifndef ppc64_unknown_linux2_4
vInsnsPerVCycleBase::
vInsnsPerVCycleBase(unsigned iid, unsigned iclusterid,
                    const countedVInsns &icountedVInsnsSimpleMetric,
                    const countedVCycles &icountedVCyclesSimpleMetric,
                    const pdstring &metname,
                    const pdstring &metdescription,
                    const pdstring &currvalMetUnits,
                    const pdstring &totalMetUnits,
                    metric_normalized imn,
                    aggregate_types iaggtypes) :
   complexMetric(iid, iclusterid, metname, metdescription,
                 currvalMetUnits,
                 totalMetUnits,
                 imn, iaggtypes,
                 make2(icountedVInsnsSimpleMetric,
                       icountedVCyclesSimpleMetric)),
   theCountedVInsnsSimpleMetric(icountedVInsnsSimpleMetric),
   theCountedVCyclesSimpleMetric(icountedVCyclesSimpleMetric) {
}

void vInsnsPerVCycleBase::calculateRegionsToSampleAndReport(
    complexMetricFocus &cmf,
    kapi_mem_region **regions,
    unsigned *pNumRegions) const
{
   const pdvector<const simpleMetricFocus *> &smfs = cmf.getComponentSimpleMetricFocuses();
   assert(smfs.size() == 2);

   const simpleMetricFocus *smf = smfs[0]; // the vinsns smf
   const pdvector<kptr_t> &kernelAddrs0 = smf->getvtimerKernelAddrs();
   assert(kernelAddrs0.size() == 1);
   kptr_t kernelAddr = kernelAddrs0[0];

   extern dataHeap *theGlobalDataHeap;
   const unsigned elemsPerVector = theGlobalDataHeap->getElemsPerVectorVT();
   const unsigned bytesPerStride = theGlobalDataHeap->getBytesPerStrideVT();
   const unsigned numRegions = 2 * elemsPerVector; // 2 vectors

   kapi_mem_region *result = new kapi_mem_region[numRegions];
   generateVectorToSample(kernelAddr, elemsPerVector, bytesPerStride, result);

   smf = smfs[1]; // the vcycles smf
   const pdvector<kptr_t> &kernelAddrs1 = smf->getvtimerKernelAddrs();
   assert(kernelAddrs1.size() == 1);
   kernelAddr = kernelAddrs1[0];

   generateVectorToSample(kernelAddr, elemsPerVector, bytesPerStride, 
			  &result[elemsPerVector]);
   *regions = result;
   *pNumRegions = numRegions;
}

// ----------------------------------------------------------------------

vInsnsPerVCycle::
vInsnsPerVCycle(unsigned iid, unsigned iclusterid,
                const countedVInsns &icountedVInsnsSimpleMetric,
                const countedVCycles &icountedVCyclesSimpleMetric) :
   vInsnsPerVCycleBase(iid, iclusterid,
                       icountedVInsnsSimpleMetric, icountedVCyclesSimpleMetric,
                       "vIPC",
                       "This metric measures the number of instructions per cycle spent in a chosen piece of code (function or basic block).\n\n\tThis metric is virtualized (so it really measures the number of *virtualized* instructions per *virtualized* cycle), so it excludes the instructions and the cycles that your chosen piece of code may spend switched out (due to blocking I/O, synchronization, preemption, ...)\n",
                       "ipc", // units when showing current values
                       "ipc secs", // units when showing totals
                       complexMetric::normalized,
                       complexMetric::avg) // this is how to fold
{
}

double vInsnsPerVCycle::
interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                            unsigned /*mhz*/) const {
   // first value will be num(v)insns
   // second value will be num(v)cycles
   assert(vals.size() == 2);
   
   const uint64_t bucketNumCompletedVInsns = vals[0];
   const uint64_t bucketNumVCycles = vals[1];

   // avoid divide-by-zero:  (Yes, it *can* happen; since we're talking *virtualized*
   // cycles in the denominator...and virtualized num cycles can easily be zero over
   // a 0.2 second interval...)
   if (bucketNumVCycles == 0)
      return 0;
   else {
      const double result = (double)bucketNumCompletedVInsns /
         (double)bucketNumVCycles;
      if (false) {
         const double inverse_result =
            (double)bucketNumVCycles / (double)bucketNumCompletedVInsns;
         cout << "ipc: " << result << " (and cpi: " << inverse_result << ")" << endl;
      }

      return result;
   }
}

vCyclesPerVInsn::
vCyclesPerVInsn(unsigned iid, unsigned iclusterid,
                const countedVInsns &icountedVInsnsSimpleMetric,
                const countedVCycles &icountedVCyclesSimpleMetric) :
      vInsnsPerVCycleBase(iid, iclusterid,
                          icountedVInsnsSimpleMetric, icountedVCyclesSimpleMetric,
                          "vCPI",
                          "This metric measures the number of cycles per instruction (CPI) for a chosen piece of code (function or basic block).\n\n\tThis metric is virtualized (so it really measures the number of *virtualized* cycles per *virtualized* instruction), so it excludes the cycles and the instructions that your chosen piece of code may spend switched out (due to blocking I/O, synchronization, preemption, ...)\n",
                          "cpi", // units when showing current values
                          "cpi secs", // units when showing total values
                          complexMetric::normalized,
                          complexMetric::avg) // this is how to fold
{
}

double
vCyclesPerVInsn::
interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                            unsigned /*mhz*/) const {
   // first value will be num(v)insns
   // second value will be num(v)cycles
   assert(vals.size() == 2);
   
   const uint64_t bucketNumCompletedVInsns = vals[0];
   const uint64_t bucketNumVCycles = vals[1];

   // avoid divide-by-zero:  (Yes, it *can* happen; since we're talking
   // *virtualized* insns in the denominator...and virtualized num insns can
   // easily be zero over a 0.2 second interval...)
   if (bucketNumCompletedVInsns == 0)
      return 0;
   else {
      const double result = (double)bucketNumVCycles /
         (double)bucketNumCompletedVInsns;
      if (false) {
         const double inverse_result =
            (double)bucketNumCompletedVInsns / (double)bucketNumVCycles;
         
         cout << "cpi: " << result << " (and ipc: " << inverse_result << ")" << endl;
      }
      
      return result;
   }
}
#endif
