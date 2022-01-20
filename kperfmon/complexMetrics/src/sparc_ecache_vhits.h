// ecache_vhits.h
// A complex metric, derived from vevents (for code sharing purposes)
// "counted" means recursion-safe (the timer counts the present recursion level)
// "v" means virtualized.

#ifndef _ECACHE_VHITS_H_
#define _ECACHE_VHITS_H_

#include "vevents.h"
#include "countedVEvents.h" // for countedECacheVHits, the simple metric that we use
#include "veventsPerVEvents.h"

class ecache_vhits : public vevents {
 private:
   const countedECacheVHits &theCountedECacheVHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   ecache_vhits(const ecache_vhits &);
   ecache_vhits& operator=(const ecache_vhits &);

 public:
   ecache_vhits(unsigned iid, unsigned iclusterid,
                const countedECacheVHits &icountedECacheVHits) :
      vevents(iid, iclusterid,
              "E-$ VHits",
              "This metric calculates the number of E-Cache (L2 cache) hits (could be code or data) in a desired piece of code (which can be a function or basic block).\n\nAs you would expect, this metric is properly virtualized, so E-Cache hits made while your code is (for example) blocked waiting for I/O are not counted.\n\n\tE-Cache hits are measured by programming the UltraSparc's %pcr register so that its %pic1 register collects E-Cache hits. This will therefore conflict with any other metric that also tries to program what %pic1 collects.",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
                 // show rthist y-axis as "events/sec", not "events"
              complexMetric::sum, // this is how to fold
              icountedECacheVHits
              ),
      theCountedECacheVHitsSimpleMetric(icountedECacheVHits) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of ecache hits (events, not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      return vals[0];
   }
};

class ecache_vhits_perinvoc : public veventsPerInvoc {
 private:
   // prevent copying:
   ecache_vhits_perinvoc(const ecache_vhits_perinvoc &);
   ecache_vhits_perinvoc &operator=(const ecache_vhits_perinvoc &);
   
 public:
   ecache_vhits_perinvoc(unsigned iid, unsigned iclusterid,
                         const countedECacheVHits &icountedECacheVHits,
                         const add1AtExit &iadd1AtExit) :
      veventsPerInvoc(iid, iclusterid,
                      "E-$ VHits/invoc",
                      "This metric calculates the number of L2 Cache (\"E-Cache\" in UltraSparc lingo) hits made in a desired piece of code (which can be a function or basic block) per invocation of that code.\n\nEssentially, this metric is a combination of two others: \"E-$ VHits\" divided by \"ExitsFrom\"; see those metrics for details.",
                      "events/invoc", // units when showing current values
                      "events/invoc secs", // units when showing total values (undefined?)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedECacheVHits,
                      iadd1AtExit) {
   }
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw values: number of ecache hits (events, not cycles) during
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


class ecache_hitandmiss_base : public veventsPerVEvents {
 private:
   const countedECacheVHits &ecachehits_smf;
   const countedECacheVRefs &ecacherefs_smf;
   
   virtual double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                              unsigned mhz) const = 0;
      
 public:
   ecache_hitandmiss_base(unsigned iid, unsigned iclusterid,
                          const pdstring &metName,
                          const pdstring &metDescription,
                          const pdstring &currentValMetUnits,
                          const pdstring &totalValMetUnits,
                          complexMetric::metric_normalized imn,
                          complexMetric::aggregate_types iaggtype,
                          const countedECacheVHits &iecachehits_smf,
                          const countedECacheVRefs &iecacherefs_smf) :
      veventsPerVEvents(iid, iclusterid, metName, metDescription,
                        currentValMetUnits, totalValMetUnits,
                        imn, iaggtype,
                        iecachehits_smf,
                        iecacherefs_smf),
      ecachehits_smf(iecachehits_smf),
      ecacherefs_smf(iecacherefs_smf) {
   }
   virtual ~ecache_hitandmiss_base() {}
};

class ecache_vmisses : public ecache_hitandmiss_base {
 private:
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      assert(vals.size() == 2);
      const uint64_t num_vhits = vals[0];
      const uint64_t num_vrefs = vals[1];

      if (num_vhits > num_vrefs)
         // strange but conceivable due to nested instrumentation
         // Treat this as 100% hit rate; no misses during this bucket
         return 0.0;
      else {
         const uint64_t num_vmisses = num_vrefs - num_vhits;
         const double result = num_vmisses;
         return result;
      }
   }
   
 public:
   ecache_vmisses(unsigned iid, unsigned iclusterid,
                  const countedECacheVHits &iecachehits_smf,
                  const countedECacheVRefs &iecacherefs_smf) :
      ecache_hitandmiss_base(iid, iclusterid,
                             "E-$ VMisses",
                             "This metric calculates the number of E-Cache (L2-Cache) misses in a desired piece of code (which can be a function or basic block).  This includes both instruction and data accesses (the UltraSparc E-Cache is unified code/data).\n\n\tAs you would expect, this metric is virtualized, so cache misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n\n\tE-Cache misses are measured by programming the UltraSparc's %pcr register so that its %pic0 register counts E-Cache references and its %pic1 register collects E-Cache hits. This will therefore conflict with any other metric that also tries to program what %pic0 or %pic1 collects.",
                             "events/sec", // units when showing current values
                             "events", // units when showing total values
                             complexMetric::unnormalized,
                             complexMetric::sum, // this is how to fold
                             iecachehits_smf,
                             iecacherefs_smf) {
   }
};

class ecache_vhitratio : public ecache_hitandmiss_base {
 private:
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      assert(vals.size() == 2);
      const uint64_t num_vhits = vals[0];
      const uint64_t num_vrefs = vals[1];

      if (num_vrefs == 0)
         return 0.0;
      else
         return (double)num_vhits / (double)num_vrefs;
   }

 public:
   ecache_vhitratio(unsigned iid, unsigned iclusterid,
                    const countedECacheVHits &iecachehits_smf,
                    const countedECacheVRefs &iecacherefs_smf) :
      ecache_hitandmiss_base(iid, iclusterid,
                             "E-$ VHitRatio",
                             "This metric calculates the hit rate of the E-Cache (i.e., the UltraSparc L2-Cache) (number of hits divided by number of accesses) for a desired piece of code, which can be a function or basic block.\n\n\tThis metric is virtualized, so cache hits and references made while that piece of code is switched out (blocked on I/O, for example) are intentionally not counted.",
                             "frac",
                             "frac seconds", // undefined?
                             complexMetric::normalized,
                             complexMetric::avg, // how to fold
                             iecachehits_smf,
                             iecacherefs_smf) {
   }
};

class ecache_vmissratio : public ecache_hitandmiss_base {
 private:
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      assert(vals.size() == 2);
      const double num_vhits = vals[0];
      const double num_vrefs = vals[1];

      if (num_vrefs == 0)
         return 0.0;
      else {
         if (num_vhits > num_vrefs)
            // conceivable due to nested instrumentation code
            // Just say 100% hit rate --> 0 --> miss rate
            return 0.0;
         else {
            const double result = 1.0 - ((double)num_vhits / (double)num_vrefs);
            assert(result >= 0.0);
            return result;
         }
      }
   }

 public:
   ecache_vmissratio(unsigned iid, unsigned iclusterid,
                     const countedECacheVHits &iecachehits_smf,
                     const countedECacheVRefs &iecacherefs_smf) :
      ecache_hitandmiss_base(iid, iclusterid,
                             "E-$ VMissRatio",
                             "This metric calculates the miss ratio of the E-Cache (i.e., UltraSparc's L2-Cache) (one minus #hits/#references) for a desired piece of code, which can be a function or basic block.\n\n\tThis metric is virtualized, so E-Cache misses and references made while that piece of code is switched out (blocked on I/O, for example) are intentially not counted.",
                             "frac",
                             "frac seconds", // undefined?
                             complexMetric::normalized,
                             complexMetric::avg, // how to fold
                             iecachehits_smf,
                             iecacherefs_smf) {
   }
};


#endif
