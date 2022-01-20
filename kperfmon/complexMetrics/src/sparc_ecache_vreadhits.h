// ecache_vreadhits.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _ECACHE_VREADHITS_H_
#define _ECACHE_VREADHITS_H_

#include "vevents.h"
#include "countedVEvents.h" // for countedECacheVReadHits, the simple metric that we use

class ecache_vreadhits : public vevents {
 private:
   const countedECacheVReadHits &theCountedECacheVReadHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   ecache_vreadhits(const ecache_vreadhits &);
   ecache_vreadhits& operator=(const ecache_vreadhits &);

 public:
   ecache_vreadhits(unsigned iid, unsigned iclusterid,
                    const countedECacheVReadHits &icountedECacheVReadHits) :
      vevents(iid, iclusterid,
              "E-$ VReadHits",
              "This metric calculates the number of E-Cache (L2 cache) read hits (from L1 D-Cache misses) in a desired piece of code (which can be a function or basic block).\n\nAs you would expect, this metric is properly virtualized, so E-Cache read hits made while your code is (for example) blocked waiting for I/O are not counted.\n\n\tE-Cache read hits are measured by programming the UltraSparc's %pcr register so that its %pic0 register collects E-Cache read hits. This will therefore conflict with any other metric that also tries to program what %pic0 collects.",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
                 // show rthist y-axis as "events/sec", not "events"
              complexMetric::sum, // this is how to fold
              icountedECacheVReadHits
              ),
      theCountedECacheVReadHitsSimpleMetric(icountedECacheVReadHits) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of ecache read hits (events, not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      return vals[0];
   }
};

#endif
