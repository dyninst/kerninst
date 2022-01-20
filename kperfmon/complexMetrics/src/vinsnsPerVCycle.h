// vInsnsPerVCycle.h
// (virtualized) instructions per (virtualized) cycle complexMetric
// Think "instrucions/cycle, or IPC".  Quite useful.

#ifndef _VINSNS_PER_VCYCLE_H_
#define _VINSNS_PER_VCYCLE_H_

// fwd decls avoid #includes, which lead to huge .o files
class complexMetric;
class countedVCycles;
class countedVInsns;

class vInsnsPerVCycleBase : public complexMetric {
 private:
   const countedVInsns &theCountedVInsnsSimpleMetric;
   const countedVCycles &theCountedVCyclesSimpleMetric;

   // prevent copying:
   vInsnsPerVCycleBase(const vInsnsPerVCycleBase &src);
   vInsnsPerVCycleBase &operator=(const vInsnsPerVCycleBase &src);

   // Required by the base class
   void calculateRegionsToSampleAndReport(complexMetricFocus &,
					  kapi_mem_region **regions,
					  unsigned *pNumRegions) const;

 public:
   vInsnsPerVCycleBase(unsigned iid, unsigned iclusterid,
                       const countedVInsns &icountedVInsnsSimpleMetric,
                       const countedVCycles &icountedVCyclesSimpleMetric,
                       const pdstring &metname,
                       const pdstring &metdescription,
                       const pdstring &currvalMetUnits,
                       const pdstring &totalMetUnits,
                       metric_normalized imn,
                       aggregate_types iaggtypes);
   virtual ~vInsnsPerVCycleBase() {}

   virtual double
   interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                               unsigned mhz) const = 0;
};

// --------------------------------------------------------------------------------

class vInsnsPerVCycle : public vInsnsPerVCycleBase {
 private:
   // prevent copying:
   vInsnsPerVCycle(const vInsnsPerVCycle &src);
   vInsnsPerVCycle &operator=(const vInsnsPerVCycle &src);
   
 public:
   vInsnsPerVCycle(unsigned iid, unsigned iclusterid,
                   const countedVInsns &icountedVInsnsSimpleMetric,
                   const countedVCycles &icountedVCyclesSimpleMetric);
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const;
};

class vCyclesPerVInsn : public vInsnsPerVCycleBase {
 private:
   // prevent copying:
   vCyclesPerVInsn(const vCyclesPerVInsn &src);
   vCyclesPerVInsn &operator=(const vCyclesPerVInsn &src);
   
 public:
   vCyclesPerVInsn(unsigned iid, unsigned iclusterid,
                   const countedVInsns &icountedVInsnsSimpleMetric,
                   const countedVCycles &icountedVCyclesSimpleMetric);

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const;
};

// --------------------------------------------------------------------------------
#endif
