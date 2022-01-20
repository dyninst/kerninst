// vtime.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _VTIME_H_
#define _VTIME_H_

#include "vevents.h" // base class for vtime complexMetric
#include "veventsPerInvoc.h" // base class for vtime_perinvoc complexMetric

#include "countedVEvents.h" // for countedVCycles, the simple metric that we use
#include "add1AtExit.h" // for exitsFrom, the other simple metric that we use

class vtime : public vevents {
 private:
   // prevent copying:
   vtime(const vtime &);
   vtime& operator=(const vtime &);

 public:
   vtime(unsigned iid, unsigned iclusterid,
         const countedVCycles &icountedVCyclesSimpleMetric) :
      vevents(iid, iclusterid, "vtime",
              "This metric measures the cpu (not wall) time spent in a chosen piece of code (function or basic block).\n\n\tThis metric is virtualized, meaning it excludes the time spent switched out (due to blocking I/O, synchronization, preemption, ...)\n\n\tIt works by instrumenting the entry of the function/basic block to read the machine's cycle counter register and store it, instrumenting the exit point(s) to re-read the cycle counter register, subtract from the earlier start time, and add the delta to an accumulated total.  (Thus far we have a wall time metric, with no virtualization.)  To achieve virtualization, the kernel's context switch code is instrumented to (1) stop the timer if switched out while the timer was active (remembering the kernel thread), and (2) re-start the timer when that kernel thread resumes execution.\n\n\tContrast this metric with \"concurrency\", which does not virtualize.\n\n\tThis metric will help you measure the time the cpu is spending attempting to execute your code.  (We say \"attempting\" instead of \"usefully\" because cpu pipeline stalls are included in this metric.)  If you are more interested in measuring latency (e.g, if the routine being measured spends most of its time doing blocking I/O or synchronization), then go with \"concurrency\" instead.",
              "cpus", // units when showing current values
              "cpu seconds", // units when showing total values
              complexMetric::unnormalized,
                 // so rthist shows "cpuSecs/sec" and tableVisi/total shows "cpuSecs"
                 // as the units.
              complexMetric::sum, // this is how to fold
              icountedVCyclesSimpleMetric
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

class vtime_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   vtime_perinvoc(const vtime_perinvoc &);
   vtime_perinvoc operator=(const vtime_perinvoc &);

 public:
   vtime_perinvoc(unsigned iid, unsigned iclusterid,
                  const countedVCycles &iCountedVCyclesSimpleMetric,
                  const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "vtime/invoc",
                      "This metric calculates the virtualized time (just like the vtime metric) spent *per invocation* of a desired piece of code (which can be a function or basic block).\n\n\tEssentially, this metric is simple a combination of two others: \"vtime\" divided by \"exitsFrom\".",
                      "seconds/invoc", // units when showing current values
                      "invoc", // units when showing total values (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedVCyclesSimpleMetric,
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
