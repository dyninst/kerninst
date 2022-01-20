// icache_vhits.h
// A complex metric, derived from vevents (for code sharing purposes)
// "v" means virtualized.

#ifndef _ICACHE_VHITS_H_
#define _ICACHE_VHITS_H_

#include "vevents.h"
#include "countedVEvents.h" // for countedICacheVHits, the simple metric that we use
#include "veventsPerVEvents.h"

class icache_vhits : public vevents {
 private:
   const countedICacheVHits &theCountedICacheVHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   icache_vhits(const icache_vhits &);
   icache_vhits& operator=(const icache_vhits &);

 public:
   icache_vhits(unsigned iid, unsigned iclusterid,
                const countedICacheVHits &icountedICacheVHits) :
      vevents(iid, iclusterid,
              "I-$ VHits",
              "This metric calculates the number of L1 I-Cache hits in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so I-Cache hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n\n\tI-Cache hits are measured by programming the UltraSparc's %pcr register so that its %pic1 register collects I-Cache hits. This will therefore conflict with any other metric that also tries to program what %pic1 collects.",
              "events/sec", // units when showing current values
              "events", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedICacheVHits
              ),
      theCountedICacheVHitsSimpleMetric(icountedICacheVHits) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of icache hits (events, not cycles) during
      // this bucket.
      // So we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class icache_vhits_perinvoc : public veventsPerInvoc {
 private:
   icache_vhits_perinvoc(const icache_vhits_perinvoc &);
   icache_vhits_perinvoc &operator=(const icache_vhits_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t icache_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = icache_hits;
         result /= exits;

         return result;
      }
   }
 public:
   icache_vhits_perinvoc(unsigned iid, unsigned iclusterid,
                         const countedICacheVHits &icountedICacheVHits,
                         const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "I-$ VHits/invoc",
                      "This metric calculates the number of L1 I-Cache hits, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"I-$ VHits\" divided by \"exitsFrom\"; see those metrids for further details.",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedICacheVHits,
                      iAdd1AtExitSimpleMetric) {
   }
};

// --------------------------------------------------------------------------------

// XXX -- metrics having to do with icache misses have (temporarily?) been put on
// ice, because I don't think they're correct.  In particular, they depend on the
// number of icache misses being #refs minus #hits.  The problem here is #refs.
// i-cache refs are at the granularity of up-to-4-insn blocks, while #hits has the
// more "normal" granularity of insns.

//  class icache_hitandmiss_base : public veventsPerVEvents {
//   private:
//     virtual double interpretPerfectBucketValForVisiShipment(const pdvector<uint64_t> &vals,
//                                                             unsigned mhz) const = 0;
      
//   public:
//     icache_hitandmiss_base(unsigned iid, unsigned iclusterid,
//                            const pdstring &metName,
//                            const pdstring &metDescription,
//                            const pdstring &currentValMetUnits,
//                            const pdstring &totalValMetUnits,
//                            complexMetric::metric_normalized imn,
//                            complexMetric::aggregate_types iaggtype,
//                            const countedICacheVHits &iicachehits_smf,
//                            const countedICacheVRefs &iicacherefs_smf) :
//        veventsPerVEvents(iid, iclusterid, metName, metDescription,
//                          currentValMetUnits, totalValMetUnits,
//                          imn, iaggtype,
//                          iicachehits_smf,
//                          iicacherefs_smf) {
//     }
//     virtual ~icache_hitandmiss_base() {}
//  };

//  class icache_vmisses : public icache_hitandmiss_base {
//   private:
//     double interpretPerfectBucketValForVisiShipment(const pdvector<uint64_t> &vals,
//                                                     unsigned /*mhz*/) const {
//        assert(vals.size() == 2);
//        const uint64_t num_vhits = vals[0];
//        const uint64_t num_vrefs = vals[1];

//        if (num_vhits > num_vrefs)
//           // strange but conceivable due to nested instrumentation
//           // Treat this as 100% hit rate; no misses during this bucket
//           return 0.0;
//        else {
//           const uint64_t num_vmisses = num_vrefs - num_vhits;
//           const double result = num_vmisses;
//           return result;
//        }
//     }
   
//   public:
//     icache_vmisses(unsigned iid, unsigned iclusterid,
//                    const countedICacheVHits &iicachehits_smf,
//                    const countedICacheVRefs &iicacherefs_smf) :
//        icache_hitandmiss_base(iid, iclusterid,
//                               "I-$ VMisses",
//                               "This metric calculates the number of L1 I-Cache misses in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is virtualized, so I-Cache misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n\n\tI-Cache misses are measured by programming the UltraSparc's %pcr register so that its %pic0 register counts I-Cache references and its %pic1 register collects I-Cache hits. This will therefore conflict with any other metric that also tries to program what %pic0 or %pic1 collects.",
//                               "events/sec", // units when showing current values
//                               "events", // units when showing total values
//                               complexMetric::unnormalized,
//                               complexMetric::sum, // this is how to fold
//                               iicachehits_smf,
//                               iicacherefs_smf) {
//     }
//  };

//  class icache_vmisses_perinvoc : public veventsPerInvoc {
//   private:
//     icache_vmisses_perinvoc(const icache_vmisses_perinvoc &);
//     icache_vmisses_perinvoc &operator=(const icache_vmisses_perinvoc &);
   
//     double interpretPerfectBucketValForVisiShipment(const pdvector<uint64_t> &vals,
//                                                     unsigned /* mhz */) const {
//        // There are two values.  The first is the numerator (#icache misses);
//        // the second is the denominator (#invocs)

//        assert(getNormalizedFlag() == complexMetric::normalized);
      
//        const uint64_t icache_misses = vals[0];
//        const uint64_t exits = vals[1];
//        if (exits == 0)
//           return 0.0;
//        else {
//           double result = icache_misses;
//           result /= exits;

//           return result;
//        }
//     }
//   public:
//     icache_vmisses_perinvoc(unsigned iid, unsigned iclusterid,
//                             const countedICacheVMisses &icountedICacheVMisses,
//                             const add1AtExit &iAdd1AtExitSimpleMetric) :
//        veventsPerInvoc(iid, iclusterid,
//                        "I-$ VMisses/invoc",
//                        "This metric calculates the number of L1 I-Cache misses, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"I-$ VMisses\" divided by \"exitsFrom\"; see those metrids for further details.",
//                        "misses/invoc", // units when showing current values
//                        "misses/invoc secs", // units when showing total values (???)
//                        complexMetric::normalized,
//                        complexMetric::avg,
//                        icountedICacheVMisses,
//                        iAdd1AtExitSimpleMetric) {
//     }
//  };

// ------------------------------------------------------------

//  class icache_vhitratio : public icache_hitandmiss_base {
//   private:
//     double interpretPerfectBucketValForVisiShipment(const pdvector<uint64_t> &vals,
//                                                     unsigned /*mhz*/) const {
//        assert(vals.size() == 2);
//        const uint64_t num_vhits = vals[0];
//        const uint64_t num_vrefs = vals[1];

//        if (num_vrefs == 0)
//           return 0.0;
//        else
//           return (double)num_vhits / (double)num_vrefs;
//     }

//   public:
//     icache_vhitratio(unsigned iid, unsigned iclusterid,
//                      const countedICacheVHits &iicachehits_smf,
//                      const countedICacheVRefs &iicacherefs_smf) :
//        icache_hitandmiss_base(iid, iclusterid,
//                               "I-$ VHitRatio",
//                               "This metric calculates the hit rate of the L1 I-Cache (number of hits divided by number of accesses) for a desired piece of code, which can be a function or basic block.\n\n\tThis metric is virtualized, so I-Cache hits and references made while that piece of code is switched out (blocked on I/O, for example) are intentially not counted.",
//                               "frac",
//                               "frac seconds", // undefined?
//                               complexMetric::normalized,
//                               complexMetric::avg, // how to fold
//                               iicachehits_smf,
//                               iicacherefs_smf) {
//     }
//  };

//  class icache_vmissratio : public icache_hitandmiss_base {
//   private:
//     double interpretPerfectBucketValForVisiShipment(const pdvector<uint64_t> &vals,
//                                                     unsigned /*mhz*/) const {
//        assert(vals.size() == 2);
//        const double num_vhits = vals[0];
//        const double num_vrefs = vals[1];

//        if (num_vrefs == 0)
//           return 0.0;
//        else {
//           if (num_vhits > num_vrefs)
//              // conceivable due to nested instrumentation code
//              // Just say 100% hit rate --> 0 --> miss rate
//              return 0.0;
//           else {
//              const double result = 1.0 - ((double)num_vhits / (double)num_vrefs);
//              assert(result >= 0.0);
//              return result;
//           }
//        }
//     }

//   public:
//     icache_vmissratio(unsigned iid, unsigned iclusterid,
//                       const countedICacheVHits &iicachehits_smf,
//                       const countedICacheVRefs &iicacherefs_smf) :
//        icache_hitandmiss_base(iid, iclusterid,
//                               "I-$ VMissRatio",
//                               "This metric calculates the miss ratio of the L1 I-Cache (one minus #hits/#references) for a desired piece of code, which can be a function or basic block.\n\n\tThis metric is virtualized, so I-Cache misses and references made while that piece of code is switched out (blocked on I/O, for example) are intentially not counted.",
//                               "frac",
//                               "frac seconds", // undefined?
//                               complexMetric::normalized,
//                               complexMetric::avg, // how to fold
//                               iicachehits_smf,
//                               iicacherefs_smf) {
//     }
//  };


#endif
