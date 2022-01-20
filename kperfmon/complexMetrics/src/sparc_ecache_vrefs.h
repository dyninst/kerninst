// ecache_vrefs.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _ECACHE_VREFS_H_
#define _ECACHE_VREFS_H_

#include "vevents.h"
#include "countedVEvents.h" // for countedICacheVRefs, the simple metric that we use

class ecache_vrefs : public vevents {
 private:
   const countedECacheVRefs &thecountedECacheVRefsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   ecache_vrefs(const ecache_vrefs &);
   ecache_vrefs& operator=(const ecache_vrefs &);

 public:
   ecache_vrefs(unsigned iid, unsigned iclusterid,
                const countedECacheVRefs &icountedECacheVRefs) :
      vevents(iid, iclusterid,
              "E-$ VRefs",
              "This metric calculates the number of E-Cache (L2 cache) references (could be code or data) in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so E-Cache references made while your code is (for example) blocked waiting for I/O are not counted.\n\n\tE-Cache references are measured by programming the UltraSparc's %pcr register so that its %pic0 register collects E-Cache references. This will therefore conflict with any other metric that also tries to program what %pic0 collects",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
                 // show rthist y-axis as "events/sec", not "events"
              complexMetric::sum, // this is how to fold
              icountedECacheVRefs
              ),
      thecountedECacheVRefsSimpleMetric(icountedECacheVRefs) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of ecache refs (events, not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      return vals[0];
   }
};

class ecache_vrefs_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   ecache_vrefs_perinvoc(const ecache_vrefs_perinvoc &);
   ecache_vrefs_perinvoc &operator=(const ecache_vrefs_perinvoc &);
   
 public:
   ecache_vrefs_perinvoc(unsigned iid, unsigned iclusterid,
                         const countedECacheVRefs &icountedECacheVRefs,
                         const add1AtExit &iadd1AtExit) :
      veventsPerInvoc(iid, iclusterid,
                      "E-$ VRefs/invoc",
                      "This metric calculates the number of L2 Cache (\"E-Cache\" in UltraSparc lingo) references made in a desired piece of code (which can be a function or basic block) per invocation of that code.\n\nEssentially, this metric is a combination of two others: \"E-$ VRefs\" divided by \"ExitsFrom\"; see those metrics for details.",
                      "events/invoc", // units when showing current values
                      "events/invoc secs", // units when showing total values (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedECacheVRefs,
                      iadd1AtExit) {
   }
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw values: number of ecache refs (events, not cycles) during
      // this bucket and number of exits during this bucket.
      
      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t events = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else
         return (double)events / (double)exits;
   }
};


#endif
