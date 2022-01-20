// branchMispredict.h

#ifndef _BRANCH_MISPREDICT_H_
#define _BRANCH_MISPREDICT_H_

#include "countedVEvents.h" // a bunch of simpleMetrics
#include "vevents.h"
#include "veventsPerInvoc.h"
#include "veventsPerVTime.h"

class branchMispred_vstallcycles : public vevents {
 private:
   const countedBranchMispredVStallCycles &thecountedBranchMispredVStallCycles;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   branchMispred_vstallcycles(const branchMispred_vstallcycles &);
   branchMispred_vstallcycles &operator=(const branchMispred_vstallcycles &);
   
 public:
   branchMispred_vstallcycles(unsigned iid, unsigned iclusterid,
                                 const countedBranchMispredVStallCycles &icountedBranchMispredVStallCycles) :
      vevents(iid, iclusterid,
              "BranchMispred VStallTime",
              "This metric calculates the (virtualized) time that the processor spends stalled when the UltraSparc instruction buffer is empty due to a branch misprediction.\n\n\t\n\n\tThe branch misprediction stall time is measured by programming the UltraSparc's %pcr register so that its %pic1 register collects Branch mispredict stall cycles. This will therefore conflict with any other metric that also tries to program what %pic0 collects.  As suggested in the UltraSparc-IIi User's Guide, we have multiplied the %pic0 value by 2, because branch misprediction also kills instructions after the dispatch point.",
              "cpus", // units when showing current values
              "cpu seconds", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedBranchMispredVStallCycles),
      thecountedBranchMispredVStallCycles(icountedBranchMispredVStallCycles) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned mhz) const {
      // sampled raw value is number of branch mispredict stall cycles during bucket.
      // We first multiply this by 2, as suggested by UltraSparc-IIi User's Guide.
      // ("Branch misprediction kills instructions after the dispatch point, so the
      // total number of pipeline bubbles is approximately twice as big as measured
      // from this count.")
      // We must divide by mhz and then by a million to get stall seconds during
      // this bucket.
      
      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);

      double result = vals[0];
      result *= 2; // As suggested by UltraSparc-IIi User's guide; see above.
      result /= mhz;
      result /= 1000000;

      return result;
   }
};

class branchMispred_vstallcycles_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   branchMispred_vstallcycles_perinvoc(const branchMispred_vstallcycles_perinvoc &);
   branchMispred_vstallcycles_perinvoc &operator=(const branchMispred_vstallcycles_perinvoc &);
      
 public:
   branchMispred_vstallcycles_perinvoc(unsigned iid, unsigned iclusterid,
                                          const countedBranchMispredVStallCycles &icountedBranchMispredVStallCycles,
                                          const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "BranchMispred VStallTime/invoc",
                      "This metric calculates the (virtualized) time that the processor spends stalled due to a branch misprediction in a desired piece of code (which can be a function or basic block), per invocation of that code.\n\n\tEssentially, this metric is simply a combination of two others: \"BranchMispred VStallTime\" divided by \"exitsFrom\". See those metrics for further details.",
                      "cpus/invoc", // units when showing current values
                      "cpus/invoc seconds", // units when showing totals (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg, // this is how to fold
                      icountedBranchMispredVStallCycles,
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
         seconds *= 2; // As recommended by UltraSparc-IIi's User's guide; see above.
         seconds /= 1000000;
         seconds /= mhz;
         
         return seconds / exits;
      }
   }
};

class branchMispred_vstallcycles_per_vtime : public veventsPerVTime {
 private:

 public:
   branchMispred_vstallcycles_per_vtime(unsigned iid, unsigned iclusterid,
                                        const countedBranchMispredVStallCycles &icountedBranchMispredVStallCycles,
                                        const countedVCycles &iCountedVCycles) :
      veventsPerVTime(iid, iclusterid,
                      "BranchMispred VStallTime/VTime",
                      "This metric calculates the fraction of time that a desired piece of code (which can be a function or basic block) is idling due to branch misprediction.\n\n\tBoth the stall cycles (numerator) and the total time (denominator) are virtualized, so they exclude time spent (for example) blocked on I/O requests.",
                      "frac", // no units really; this is a true scalar value
                      "seconds", // units for total time (undefined really)
                      complexMetric::normalized,
                      complexMetric::avg, // how to fold
                      icountedBranchMispredVStallCycles,
                      iCountedVCycles) {
   }
   virtual ~branchMispred_vstallcycles_per_vtime() {}

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // There will be two values: stall cycles, and total cycles.
      assert(vals.size() == 2);
      
      const uint64_t stall_cycles = vals[0];
      const uint64_t total_cycles = vals[1];
      if (total_cycles == 0)
         return 0.0;
      else {
         double result = (double)stall_cycles * 2.0 / (double)total_cycles;
         // The 2.0 kludge is as recommended by the UltraSparc-IIi's User's Guide,
         // in order to get closer to the actual value.
         
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
