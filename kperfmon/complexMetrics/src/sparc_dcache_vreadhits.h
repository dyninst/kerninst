// dcache_vreadhits.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _DCACHE_VREADHITS_H_
#define _DCACHE_VREADHITS_H_

#include "vevents.h"
#include "veventsPerInvoc.h"
#include "countedVEvents.h" // for countedDCacheVReadHits, the simple metric that we use

class dcache_vreadhits : public vevents {
 private:
   const countedDCacheVReadHits &theCountedDCacheVReadHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   dcache_vreadhits(const dcache_vreadhits &);
   dcache_vreadhits& operator=(const dcache_vreadhits &);

 public:
   dcache_vreadhits(unsigned iid, unsigned iclusterid,
                    const countedDCacheVReadHits &icountedDCacheVReadHits) :
      vevents(iid, iclusterid,
              "D-$ VReadHits",
              "This metric calculates the number of L1 D-Cache read hits made in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so D-Cache hits made while your code is (for example) blocked waiting for I/O are not counted.\n\n\tAn UltraSparc D-Cache assess is considered a read hit if: (1) it accesses the D-Cache tags and does not enter the load buffer (because the load buffer is already empty), or (2) it exits the load buffer (due to a D-Cache miss or a non-empty load buffer). By waiting until the reference exits the load buffer, the UltraSparc chip can properly exclude loads that turned into misses due to multiprocessor snoop occurring during its stay in the load buffer (e.g. from an E-Cache miss).\n\n\tD-Cache reads are measured by programming the UltraSparc's %pcr register so that its %pic1 register collects L1 D-Cache read hits. This will therefore conflict with any other metric that also tries to program what %pic1 collects.",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedDCacheVReadHits
              ),
      theCountedDCacheVReadHitsSimpleMetric(icountedDCacheVReadHits) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of dcache read hits (events, not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      return vals[0];
   }
};

class dcache_vreadhits_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   dcache_vreadhits_perinvoc(const dcache_vreadhits_perinvoc &);
   dcache_vreadhits_perinvoc &operator=(const dcache_vreadhits_perinvoc &);

 public:
   dcache_vreadhits_perinvoc(unsigned iid, unsigned iclusterid,
                             const countedDCacheVReadHits &iCountedDCacheVReadHits,
                             const add1AtExit &iAdd1AtExit) :
      veventsPerInvoc(iid, iclusterid,
                      "D-$ VReadHits/invoc",
                      "This metric calculates the number of L1 D-Cache read hits made in a desired piece of code (which can be a function or basic block) per invocation of that code.\n\n\tEssentially, this metric is a combination of two others: \"D-$ VReadHits\" divided by \"ExitsFrom\"; see those metrics for details.",
                      "events/invoc", // units when showing current values
                      "events/invoc secs", // units when showing total values (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedDCacheVReadHits,
                      iAdd1AtExit) {
   }
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw values: number of dcache read hits (events, not cycles) during
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
