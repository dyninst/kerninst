// icache_vrefs.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _ICACHE_VREFS_H_
#define _ICACHE_VREFS_H_

#include "vevents.h"
#include "countedVEvents.h" // for countedICacheVRefs, the simple metric that we use

class icache_vrefs : public vevents {
 private:
   const countedICacheVRefs &theCountedICacheVRefsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   icache_vrefs(const icache_vrefs &);
   icache_vrefs& operator=(const icache_vrefs &);

 public:
   icache_vrefs(unsigned iid, unsigned iclusterid,
                const countedICacheVRefs &icountedICacheVRefs) :
      vevents(iid, iclusterid,
              "I-$ VRefs",
              "This metric calculates the number of L1 I-Cache references in a desired piece of code (which can be a function or basic block).\n\n\tAn I-Cache reference on the UltraSparc is considered to be a single fetch of <=4 instructions from among an aligned block of 8 instructions.  I-Cache references are usually prefetches, so these instruction(s) will not necessarily be executed.\n\n\tAs you would expect, this metric is properly virtualized, so I-Cache references made while your code is (for example) blocked waiting for I/O are not counted.\n\n\tI-Cache references are measured by programming the UltraSparc's %pcr register so that its %pic0 register collects I-Cache hits. This will therefore conflict with any other metric that also tries to program what %pic0 collects",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
                 // show rthist y-axis as "events/sec", not "events"
              complexMetric::sum, // this is how to fold
              icountedICacheVRefs
              ),
      theCountedICacheVRefsSimpleMetric(icountedICacheVRefs) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of icache refs (events, not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      return vals[0];
   }
};

class icache_vrefs_perinvoc : public veventsPerInvoc {
 private:
   icache_vrefs_perinvoc(const icache_vrefs_perinvoc &);
   icache_vrefs_perinvoc &operator=(const icache_vrefs_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // There are two values.  The first is the numerator (#icache refs);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t icache_refs = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = icache_refs;
         result /= exits;

         return result;
      }
   }
 public:
   icache_vrefs_perinvoc(unsigned iid, unsigned iclusterid,
                         const countedICacheVRefs &icountedICacheVRefs,
                         const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "I-$ VRefs/invoc",
                      "This metric calculates the number of L1 I-Cache references, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"I-$ VRefs\" divided by \"exitsFrom\"; see those metrids for further details.",
                      "refs/invoc", // units when showing current values
                      "refs/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedICacheVRefs,
                      iAdd1AtExitSimpleMetric) {
   }
};

#endif
