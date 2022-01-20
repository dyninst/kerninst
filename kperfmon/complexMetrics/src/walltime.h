// walltime.h
// A complex metric, derived from vevents (for code sharing purposes)

#ifndef _WALLTIME_H_
#define _WALLTIME_H_

#include "vevents.h" // base class for vtime complexMetric
#include "veventsPerInvoc.h" // base class for vtime_perinvoc complexMetric

#include "countedVEvents.h" // for countedVCycles, the simple metric that we use
#include "add1AtExit.h" // for exitsFrom, the other simple metric that we use

class walltime : public vevents {
 private:
   // prevent copying:
   walltime(const walltime &);
   walltime& operator=(const walltime &);

 public:
   walltime(unsigned iid, unsigned iclusterid,
         const countedWallCycles &icountedWallCyclesSimpleMetric) :
      vevents(iid, iclusterid, "walltime",
              "This metric measures the total (not virtualized, so includes\n"
              "time switched-out) time spent in a selected piece of code\n"
	      "(function or basic block). If multiple threads execute within\n"
	      "this piece of code simultaneously, the outcome is the SUM of\n"
	      "the time spent by each thread.",
              "threads", // units when showing current values
              "thread-seconds", // units when showing total values
              complexMetric::unnormalized,
                 // so rthist shows "cpuSecs/sec" and tableVisi/total shows "cpuSecs"
                 // as the units.
              complexMetric::sum, // this is how to fold
              icountedWallCyclesSimpleMetric
              ) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned mhz) const {
      assert(getNormalizedFlag() == complexMetric::unnormalized);
      // Raw value sampled is number of cycles taking place during this bucket.
      // We need to convert from cycles to seconds

      assert(vals.size() == 1);
      double result = vals[0];
      result /= 1000000;
      result /= mhz;
      
      return result; // number of seconds during this bucket
   }
};

class walltime_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   walltime_perinvoc(const vtime_perinvoc &);
   walltime_perinvoc operator=(const vtime_perinvoc &);

 public:
   walltime_perinvoc(unsigned iid, unsigned iclusterid,
                  const countedWallCycles &iCountedWallCyclesSimpleMetric,
                  const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "walltime/invoc",
                      "This metric calculates the walltime spent *per invocation* of a desired piece of code (which can be a function or basic block).\n\n\tEssentially, this metric is simple a combination of two others: \"walltime\" divided by \"exitsFrom\".",
                      "seconds/invoc", // units when showing current values
                      "invoc", // units when showing total values (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedWallCyclesSimpleMetric,
                      iAdd1AtExitSimpleMetric) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned mhz) const {
      // sampled raw values:
      // (1) vcycles
      // (2) # exits from

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cycles = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double seconds = cycles;
         seconds /= 1000000;
         seconds /= mhz;
         
         return seconds/exits;
      }
   }
};

#endif
