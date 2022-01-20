// dcache_vwrites.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _DCACHE_VWRITES_H_
#define _DCACHE_VWRITES_H_

#include "vevents.h"
#include "countedVEvents.h" // for countedDCacheVWrites, the simple metric that we use

class dcache_vwrites : public vevents {
 private:
   const countedDCacheVWrites &theCountedDCacheVWritesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   dcache_vwrites(const dcache_vwrites &);
   dcache_vwrites& operator=(const dcache_vwrites &);

 public:
   dcache_vwrites(unsigned iid, unsigned iclusterid,
                  const countedDCacheVWrites &icountedDCacheVWrites) :
      vevents(iid, iclusterid,
              "D-$ VWrites",
              "This metric calculates the number of L1 D-Cache writes (but not reads) made in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so D-Cache writes made while your code is (for example) blocked waiting for I/O are not counted.\n\n\tWe measure D-Cache writes by programming the UltraSparc's %pcr register so that its %pic0 register collects L1 D-Cache number of writes. This will therefore conflict with any other metric that also tries to program what %pic0 collects",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
                 // show rthist y-axis as "events/sec", not "events"
              complexMetric::sum, // this is how to fold
              icountedDCacheVWrites
              ),
      theCountedDCacheVWritesSimpleMetric(icountedDCacheVWrites) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of dcache writes (events, not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      return vals[0];
   }
};

class dcache_vwrites_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   dcache_vwrites_perinvoc(const dcache_vwrites_perinvoc &);
   dcache_vwrites_perinvoc &operator=(const dcache_vwrites_perinvoc &);
   
 public:
   dcache_vwrites_perinvoc(unsigned iid, unsigned iclusterid,
                          const countedDCacheVWrites &iCountedDCacheVWrites,
                          const add1AtExit &iAdd1AtExit) :
      veventsPerInvoc(iid, iclusterid,
                      "D-$ VWrites/invoc",
                      "This metric calculates the number of L1 D-Cache writes made in a desired piece of code (which can be a function or basic block) per invocation of that code.\n\n\tEssentially, this metric is a combination of two others: \"D-$ VWrites\" divided by \"ExitsFrom\"; see those metrics for details.",
                      "events/invoc", // units when showing current values
                      "events/invoc secs", // units when showing total values (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedDCacheVWrites,
                      iAdd1AtExit) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw values: number of dcache writes (events, not cycles) during
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
