// dcache_vreads.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _DCACHE_VREADS_H_
#define _DCACHE_VREADS_H_

#include "vevents.h"
#include "countedVEvents.h" // for countedDCacheVReads, the simple metric that we use

class dcache_vreads : public vevents {
 private:
   // prevent copying:
   dcache_vreads(const dcache_vreads &);
   dcache_vreads& operator=(const dcache_vreads &);

 public:
   dcache_vreads(unsigned iid, unsigned iclusterid,
                 const countedDCacheVReads &icountedDCacheVReads) :
      vevents(iid, iclusterid,
              "D-$ VReads",
              "This metric calculates the number of L1 D-Cache reads (but not writes) made in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so D-Cache reads made while your code is (for example) blocked waiting for I/O are not counted.\n\n\tD-Cache reads are measured by programming the UltraSparc's %pcr register so that its %pic0 register collects L1 D-Cache number of reads. This will therefore conflict with any other metric that also tries to program what %pic0 collects",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
                 // show rthist y-axis as "events/sec", not "events"
              complexMetric::sum, // this is how to fold
              icountedDCacheVReads
              ) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of dcache read events (not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      return vals[0];
   }
};

class dcache_vreads_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   dcache_vreads_perinvoc(const dcache_vreads_perinvoc &);
   dcache_vreads_perinvoc &operator=(const dcache_vreads_perinvoc &);
   
 public:
   dcache_vreads_perinvoc(unsigned iid, unsigned iclusterid,
                          const countedDCacheVReads &iCountedDCacheVReads,
                          const add1AtExit &iAdd1AtExit) :
      veventsPerInvoc(iid, iclusterid,
                      "D-$ VReads/invoc",
                      "This metric calculates the number of L1 D-Cache reads made in a desired piece of code (which can be a function or basic block) per invocation of that code.\n\n\tEssentially, this metric is a combination of two others: \"D-$ VReads\" divided by \"ExitsFrom\"; see those metrics for details.",
                      "events/invoc", // units when showing current values
                      "events/invoc secs", // units when showing total values (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedDCacheVReads,
                      iAdd1AtExit) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw values: number of dcache reads (events, not cycles) during
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
