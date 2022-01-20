// x86_Lcache.h - complex metrics related to x86 caches (L1,L2,L3)

#ifndef _X86_LCACHE_H_
#define _X86_LCACHE_H_

#include "vevents.h"
#include "countedVEvents.h"
#include "veventsPerVEvents.h"

/* ------------------------ L1 Cache Metrics ------------------------- */

class L1_vread_misses : public vevents {
 private:
   const countedL1ReadMisses &theCountedL1VReadMissesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L1_vread_misses(const L1_vread_misses &);
   L1_vread_misses& operator=(const L1_vread_misses &);

 public:
   L1_vread_misses(unsigned iid, unsigned iclusterid,
                const countedL1ReadMisses &icountedL1ReadMisses) :
      vevents(iid, iclusterid,
              "L1-$ Read Misses",
              "This metric calculates the number of L1-cache read misses in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "misses/sec", // units when showing current values
              "misses", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL1ReadMisses
              ),
      theCountedL1VReadMissesSimpleMetric(icountedL1ReadMisses) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L1_vread_misses_perinvoc : public veventsPerInvoc {
 private:
   L1_vread_misses_perinvoc(const L1_vread_misses_perinvoc &);
   L1_vread_misses_perinvoc &operator=(const L1_vread_misses_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_misses = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_misses;
         result /= exits;

         return result;
      }
   }

 public:
   L1_vread_misses_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL1ReadMisses &icountedL1ReadMisses,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L1-$ Read Misses/invoc",
                      "This metric calculates the number of L1-cache read misses, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L1-$ Read Misses\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "misses/invoc", // units when showing current values
                      "misses/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL1ReadMisses,
                      iAdd1AtExitSimpleMetric) {
   }
};

/* ------------------------ L2 Cache Metrics ------------------------- */

class L2_vread_hits_shared : public vevents {
 private:
   const countedL2ReadHitsShr &theCountedL2VReadSharedHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L2_vread_hits_shared(const L2_vread_hits_shared &);
   L2_vread_hits_shared& operator=(const L2_vread_hits_shared &);

 public:
   L2_vread_hits_shared(unsigned iid, unsigned iclusterid,
                        const countedL2ReadHitsShr &icountedL2ReadHitsShr) :
      vevents(iid, iclusterid,
              "L2-$ Read Hits (shared)",
              "This metric calculates the number of L2-cache read hits on cache lines in the shared state in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "hits/sec", // units when showing current values
              "hits", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL2ReadHitsShr
              ),
      theCountedL2VReadSharedHitsSimpleMetric(icountedL2ReadHitsShr) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L2_vread_hits_shared_perinvoc : public veventsPerInvoc {
 private:
   L2_vread_hits_shared_perinvoc(const L2_vread_hits_shared_perinvoc &);
   L2_vread_hits_shared_perinvoc &operator=(const L2_vread_hits_shared_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_hits;
         result /= exits;

         return result;
      }
   }

 public:
   L2_vread_hits_shared_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL2ReadHitsShr &icountedL2ReadHitsShr,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L2-$ Read Hits (shared)/invoc",
                      "This metric calculates the number of L2-cache read hits on cache lines in the shared state, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L2-$ Read Hits (shared)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL2ReadHitsShr,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L2_vread_hits_exclusive : public vevents {
 private:
   const countedL2ReadHitsExcl &theCountedL2VReadExclusiveHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L2_vread_hits_exclusive(const L2_vread_hits_exclusive &);
   L2_vread_hits_exclusive& operator=(const L2_vread_hits_exclusive &);

 public:
   L2_vread_hits_exclusive(unsigned iid, unsigned iclusterid,
                        const countedL2ReadHitsExcl &icountedL2ReadHitsExcl) :
      vevents(iid, iclusterid,
              "L2-$ Read Hits (exclusive)",
              "This metric calculates the number of L2-cache read hits on cache lines in the exclusive state in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "hits/sec", // units when showing current values
              "hits", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL2ReadHitsExcl
              ),
      theCountedL2VReadExclusiveHitsSimpleMetric(icountedL2ReadHitsExcl) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L2_vread_hits_exclusive_perinvoc : public veventsPerInvoc {
 private:
   L2_vread_hits_exclusive_perinvoc(const L2_vread_hits_exclusive_perinvoc &);
   L2_vread_hits_exclusive_perinvoc &operator=(const L2_vread_hits_exclusive_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_hits;
         result /= exits;

         return result;
      }
   }

 public:
   L2_vread_hits_exclusive_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL2ReadHitsExcl &icountedL2ReadHitsExcl,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L2-$ Read Hits (exclusive)/invoc",
                      "This metric calculates the number of L2-cache read hits on cache lines in the exclusive state, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L2-$ Read Hits (exclusive)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL2ReadHitsExcl,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L2_vread_hits_modified : public vevents {
 private:
   const countedL2ReadHitsMod &theCountedL2VReadModifiedHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L2_vread_hits_modified(const L2_vread_hits_modified &);
   L2_vread_hits_modified& operator=(const L2_vread_hits_modified &);

 public:
   L2_vread_hits_modified(unsigned iid, unsigned iclusterid,
                          const countedL2ReadHitsMod &icountedL2ReadHitsMod) :
      vevents(iid, iclusterid,
              "L2-$ Read Hits (modified)",
              "This metric calculates the number of L2-cache read hits on cache lines in the modified state in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "hits/sec", // units when showing current values
              "hits", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL2ReadHitsMod
              ),
      theCountedL2VReadModifiedHitsSimpleMetric(icountedL2ReadHitsMod) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L2_vread_hits_modified_perinvoc : public veventsPerInvoc {
 private:
   L2_vread_hits_modified_perinvoc(const L2_vread_hits_modified_perinvoc &);
   L2_vread_hits_modified_perinvoc &operator=(const L2_vread_hits_modified_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_hits;
         result /= exits;

         return result;
      }
   }

 public:
   L2_vread_hits_modified_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL2ReadHitsMod &icountedL2ReadHitsMod,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L2-$ Read Hits (modified)/invoc",
                      "This metric calculates the number of L2-cache read hits on cache lines in the modified state, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L2-$ Read Hits (modified)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL2ReadHitsMod,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L2_vread_misses : public vevents {
 private:
   const countedL2ReadMisses &theCountedL2VReadMissesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L2_vread_misses(const L2_vread_misses &);
   L2_vread_misses& operator=(const L2_vread_misses &);

 public:
   L2_vread_misses(unsigned iid, unsigned iclusterid,
                const countedL2ReadMisses &icountedL2ReadMisses) :
      vevents(iid, iclusterid,
              "L2-$ Read Misses",
              "This metric calculates the number of L2-cache read misses in a desired piece of code (which can be a function or basic block). Only misses found by the fast detection logic are included in the miss count.\n\n\tAs you would expect, this metric is properly virtualized, so cache misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "misses/sec", // units when showing current values
              "misses", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL2ReadMisses
              ),
      theCountedL2VReadMissesSimpleMetric(icountedL2ReadMisses) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L2_vread_misses_perinvoc : public veventsPerInvoc {
 private:
   L2_vread_misses_perinvoc(const L2_vread_misses_perinvoc &);
   L2_vread_misses_perinvoc &operator=(const L2_vread_misses_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_misses = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_misses;
         result /= exits;

         return result;
      }
   }

 public:
   L2_vread_misses_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL2ReadMisses &icountedL2ReadMisses,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L2-$ Read Misses/invoc",
                      "This metric calculates the number of L2-cache read misses, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L2-$ Read Misses\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "misses/invoc", // units when showing current values
                      "misses/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL2ReadMisses,
                      iAdd1AtExitSimpleMetric) {
   }
};

/* ------------------------ L3 Cache Metrics ------------------------- */

class L3_vread_hits_shared : public vevents {
 private:
   const countedL3ReadHitsShr &theCountedL3VReadSharedHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L3_vread_hits_shared(const L3_vread_hits_shared &);
   L3_vread_hits_shared& operator=(const L3_vread_hits_shared &);

 public:
   L3_vread_hits_shared(unsigned iid, unsigned iclusterid,
                        const countedL3ReadHitsShr &icountedL3ReadHitsShr) :
      vevents(iid, iclusterid,
              "L3-$ Read Hits (shared)",
              "This metric calculates the number of L3-cache read hits on cache lines in the shared state in a desired piece of code (which can be a function or basic block). Note that only Intel Xeon processors (which does not include the Pentium4) have L3 caches.\n\n\tAs you would expect, this metric is properly virtualized, so cache hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "hits/sec", // units when showing current values
              "hits", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL3ReadHitsShr
              ),
      theCountedL3VReadSharedHitsSimpleMetric(icountedL3ReadHitsShr) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L3_vread_hits_shared_perinvoc : public veventsPerInvoc {
 private:
   L3_vread_hits_shared_perinvoc(const L3_vread_hits_shared_perinvoc &);
   L3_vread_hits_shared_perinvoc &operator=(const L3_vread_hits_shared_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_hits;
         result /= exits;

         return result;
      }
   }

 public:
   L3_vread_hits_shared_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL3ReadHitsShr &icountedL3ReadHitsShr,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L3-$ Read Hits (shared)/invoc",
                      "This metric calculates the number of L3-cache read hits on cache lines in the shared state, in a desired piece of code (function or basic block), per invocation of that code. Note that only Intel Xeon processors (which does not include the Pentium4) have L3 caches.\n\nThis metric is basically a combination of two others; \"L3-$ Read Hits (shared)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL3ReadHitsShr,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L3_vread_hits_exclusive : public vevents {
 private:
   const countedL3ReadHitsExcl &theCountedL3VReadExclusiveHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L3_vread_hits_exclusive(const L3_vread_hits_exclusive &);
   L3_vread_hits_exclusive& operator=(const L3_vread_hits_exclusive &);

 public:
   L3_vread_hits_exclusive(unsigned iid, unsigned iclusterid,
                        const countedL3ReadHitsExcl &icountedL3ReadHitsExcl) :
      vevents(iid, iclusterid,
              "L3-$ Read Hits (exclusive)",
              "This metric calculates the number of L3-cache read hits on cache lines in the exclusive state in a desired piece of code (which can be a function or basic block). Note that only Intel Xeon processors (which does not include the Pentium4) have L3 caches.\n\n\tAs you would expect, this metric is properly virtualized, so cache hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "hits/sec", // units when showing current values
              "hits", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL3ReadHitsExcl
              ),
      theCountedL3VReadExclusiveHitsSimpleMetric(icountedL3ReadHitsExcl) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L3_vread_hits_exclusive_perinvoc : public veventsPerInvoc {
 private:
   L3_vread_hits_exclusive_perinvoc(const L3_vread_hits_exclusive_perinvoc &);
   L3_vread_hits_exclusive_perinvoc &operator=(const L3_vread_hits_exclusive_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_hits;
         result /= exits;

         return result;
      }
   }

 public:
   L3_vread_hits_exclusive_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL3ReadHitsExcl &icountedL3ReadHitsExcl,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L3-$ Read Hits (exclusive)/invoc",
                      "This metric calculates the number of L3-cache read hits on cache lines in the exclusive state, in a desired piece of code (function or basic block), per invocation of that code. Note that only Intel Xeon processors (which does not include the Pentium4) have L3 caches.\n\nThis metric is basically a combination of two others; \"L3-$ Read Hits (exclusive)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL3ReadHitsExcl,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L3_vread_hits_modified : public vevents {
 private:
   const countedL3ReadHitsMod &theCountedL3VReadModifiedHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L3_vread_hits_modified(const L3_vread_hits_modified &);
   L3_vread_hits_modified& operator=(const L3_vread_hits_modified &);

 public:
   L3_vread_hits_modified(unsigned iid, unsigned iclusterid,
                          const countedL3ReadHitsMod &icountedL3ReadHitsMod) :
      vevents(iid, iclusterid,
              "L3-$ Read Hits (modified)",
              "This metric calculates the number of L3-cache read hits on cache lines in the modified state in a desired piece of code (which can be a function or basic block). Note that only Intel Xeon processors (which does not include the Pentium4) have L3 caches.\n\n\tAs you would expect, this metric is properly virtualized, so cache hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "hits/sec", // units when showing current values
              "hits", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL3ReadHitsMod
              ),
      theCountedL3VReadModifiedHitsSimpleMetric(icountedL3ReadHitsMod) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L3_vread_hits_modified_perinvoc : public veventsPerInvoc {
 private:
   L3_vread_hits_modified_perinvoc(const L3_vread_hits_modified_perinvoc &);
   L3_vread_hits_modified_perinvoc &operator=(const L3_vread_hits_modified_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_hits;
         result /= exits;

         return result;
      }
   }

 public:
   L3_vread_hits_modified_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL3ReadHitsMod &icountedL3ReadHitsMod,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L3-$ Read Hits (modified)/invoc",
                      "This metric calculates the number of L3-cache read hits on cache lines in the modified state, in a desired piece of code (function or basic block), per invocation of that code. Note that only Intel Xeon processors (which does not include the Pentium4) have L3 caches.\n\nThis metric is basically a combination of two others; \"L3-$ Read Hits (modified)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL3ReadHitsMod,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L3_vread_misses : public vevents {
 private:
   const countedL3ReadMisses &theCountedL3VReadMissesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L3_vread_misses(const L3_vread_misses &);
   L3_vread_misses& operator=(const L3_vread_misses &);

 public:
   L3_vread_misses(unsigned iid, unsigned iclusterid,
                const countedL3ReadMisses &icountedL3ReadMisses) :
      vevents(iid, iclusterid,
              "L3-$ Read Misses",
              "This metric calculates the number of L3-cache read misses in a desired piece of code (which can be a function or basic block). Note that only Intel Xeon processors (which does not include the Pentium4) have L3 caches.\n\n\tAs you would expect, this metric is properly virtualized, so cache misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "misses/sec", // units when showing current values
              "misses", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL3ReadMisses
              ),
      theCountedL3VReadMissesSimpleMetric(icountedL3ReadMisses) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of cache misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class L3_vread_misses_perinvoc : public veventsPerInvoc {
 private:
   L3_vread_misses_perinvoc(const L3_vread_misses_perinvoc &);
   L3_vread_misses_perinvoc &operator=(const L3_vread_misses_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_misses = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_misses;
         result /= exits;

         return result;
      }
   }

 public:
   L3_vread_misses_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL3ReadMisses &icountedL3ReadMisses,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L3-$ Read Misses/invoc",
                      "This metric calculates the number of L3-cache read misses, in a desired piece of code (function or basic block), per invocation of that code. Note that only Intel Xeon processors (which does not include the Pentium4) have L3 caches.\n\nThis metric is basically a combination of two others; \"L3-$ Read Misses\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "misses/invoc", // units when showing current values
                      "misses/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL3ReadMisses,
                      iAdd1AtExitSimpleMetric) {
   }
};

#endif
