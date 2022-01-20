// dcache_vwritehits.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _DCACHE_VWRITEHITS_H_
#define _DCACHE_VWRITEHITS_H_

#include "vevents.h"
#include "countedVEvents.h" // for the simpleMetric countedDCacheVWriteHits

class dcache_vwritehits : public vevents {
 private:
   const countedDCacheVWriteHits &theCountedDCacheVWriteHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   dcache_vwritehits(const dcache_vwritehits &);
   dcache_vwritehits& operator=(const dcache_vwritehits &);

 public:
   dcache_vwritehits(unsigned iid, unsigned iclusterid,
                     const countedDCacheVWriteHits &icountedDCacheVWriteHits) :
      vevents(iid, iclusterid,
              "D-$ VWriteHits",
              "This metric calculates the number of L1 D-Cache write hits made in a desired piece of code (which can be a function or basic block).\n\nAs you would expect, this metric is properly virtualized, so D-Cache hits made while your code is (for example) blocked waiting for I/O are not counted.\n\nD-Cache write hits are measured by programming the UltraSparc's %pcr register so that its %pic1 register collects L1 D-Cache write hits. This will therefore conflict with any other metric that also tries to program what %pic1 collects.",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
                 // show rthist y-axis as "events/sec", not "events"
              complexMetric::sum, // this is how to fold
              icountedDCacheVWriteHits
              ),
      theCountedDCacheVWriteHitsSimpleMetric(icountedDCacheVWriteHits) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of dcache write hits (events, not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      return vals[0];
   }
};

class dcache_vwritehits_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   dcache_vwritehits_perinvoc(const dcache_vwritehits_perinvoc &);
   dcache_vwritehits_perinvoc &operator=(const dcache_vwritehits_perinvoc &);

 public:
   dcache_vwritehits_perinvoc(unsigned iid, unsigned iclusterid,
                             const countedDCacheVWriteHits &iCountedDCacheVWriteHits,
                             const add1AtExit &iAdd1AtExit) :
      veventsPerInvoc(iid, iclusterid,
                      "D-$ VWriteHits/invoc",
                      "This metric calculates the number of L1 D-Cache write hits made in a desired piece of code (which can be a function or basic block) per invocation of that code.\n\n\tEssentially, this metric is a combination of two others: \"D-$ VWriteHits\" divided by \"ExitsFrom\"; see those metrics for details.",
                      "events/invoc", // units when showing current values
                      "events/invoc secs", // units when showing total values (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedDCacheVWriteHits,
                      iAdd1AtExit) {
   }
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw values: number of dcache write hits (events, not cycles) during
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
