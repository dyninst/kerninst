// icache_vstallcycles.h
// A complex metric, derived from vevents (for code sharing purposes)
// "v" means virtualized.

#ifndef _ICACHE_VSTALLCYCLES_H_
#define _ICACHE_VSTALLCYCLES_H_

#include "countedVEvents.h" // a bunch of simpleMetrics
#include "vevents.h"
#include "veventsPerInvoc.h"
#include "veventsPerVTime.h"

class icache_vstallcycles : public vevents {
 private:
   const countedICacheVStallCycles &theCountedICacheVStallCycles;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   icache_vstallcycles(const icache_vstallcycles &);
   icache_vstallcycles &operator=(const icache_vstallcycles &);
   
 public:
   icache_vstallcycles(unsigned iid, unsigned iclusterid,
                       const countedICacheVStallCycles &iCountedICacheVStallCycles) :
      vevents(iid, iclusterid,
              "I-$ VStallTime",
              "This metric calculates the (virtualized) time that the processor spends stalled when the UltraSparc instruction buffer is empty due to an I-$ miss.\n\n\tThis includes L2 cache miss processing time, should an L2 cache miss also occur.\n\n\tI-Cache stall time is measured by programming the UltraSparc's %pcr register so that its %pic0 register collects I-Cache stall cycles. This will therefore conflict with any other metric that also tries to program what %pic0 collects.",
              "cpus", // units when showing current values
              "cpu seconds", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCountedICacheVStallCycles),
      theCountedICacheVStallCycles(iCountedICacheVStallCycles) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned mhz) const {
      // sampled raw value is number of icache stall cycles during this bucket.
      // We must divide by mhz and then by a million to get stall seconds during
      // this bucket.
      
      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);

      double result = vals[0];
      result /= mhz;
      result /= 1000000;

      return result;
   }
};

class icache_vstallcycles_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   icache_vstallcycles_perinvoc(const icache_vstallcycles_perinvoc &);
   icache_vstallcycles_perinvoc &operator=(const icache_vstallcycles_perinvoc &);
      
 public:
   icache_vstallcycles_perinvoc(unsigned iid, unsigned iclusterid,
                                const countedICacheVStallCycles &iCountedICacheVStallCycles,
                                const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "I-$ VStallTime/invoc",
                      "This metric calculates the (virtualized) time that the processor spends stalled due to an I-$ miss in a desired piece of code (which can be a function or basic block), per invocation of that code.\n\n\tEssentially, this metric is simply a combination of two others: \"I-$ VStallTime\" divided by \"exitsFrom\". See those metrics for further details.",
                      "cpus/invoc", // units when showing current values
                      "cpus/invoc seconds", // units when showing totals (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg, // this is how to fold
                      iCountedICacheVStallCycles,
                      iAdd1AtExitSimpleMetric) {
   }
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned mhz) const {
      // sampled raw values are: (1) vcycles spent doing icache miss processing, and
      // (2) # exits from.

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cycles = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double seconds = cycles;
         seconds /= 1000000;
         seconds /= mhz;
         
         return seconds / exits;
      }
   }
};

class icache_vstallcycles_per_vtime : public veventsPerVTime {
 private:

 public:
   icache_vstallcycles_per_vtime(unsigned iid, unsigned iclusterid,
                                 const countedICacheVStallCycles &iCountedICacheVStallCycles,
                                 const countedVCycles &iCountedVCycles) :
      veventsPerVTime(iid, iclusterid,
                      "I-$ VStallFraction",
                      "This metric calculates the fraction of time that a desired piece of code (which can be a function or basic block) is idling due to I-Cache miss handling.\n\n\tBoth the stall cycles (numerator) and the total time (denominator) are virtualized, so they exclude time spent (for example) blocked on I/O requests.",
                      "frac", // no units really; this is a true scalar value
                      "seconds", // units for total time (undefined really)
                      complexMetric::normalized,
                      complexMetric::avg, // how to fold
                      iCountedICacheVStallCycles,
                      iCountedVCycles) {
   }
   virtual ~icache_vstallcycles_per_vtime() {}

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // There will be two values: stall cycles, and total cycles.
      assert(vals.size() == 2);
      
      const uint64_t stall_cycles = vals[0];
      const uint64_t total_cycles = vals[1];
      if (total_cycles == 0)
         return 0.0;
      else {
         double result = (double)stall_cycles / (double)total_cycles;
         
         // Make sure the result is never greater than 1.0 (can happen due to
         // vtime instrumentation being inside of icache stall cycles instrumentation,
         // for example).
         if (result > 1.0)
            result = 1.0;

         return result;
      }
   }
};

#endif
