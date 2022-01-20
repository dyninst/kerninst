// x86_tlb.h - complex metrics related to x86 TLBs

#ifndef _X86_TLB_H_
#define _X86_TLB_H_

#include "vevents.h"
#include "countedVEvents.h"
#include "veventsPerVEvents.h"

// ************************ I-TLB metrics **************************

class ITLB_vhits : public vevents {
 private:
   const countedITLBHits &theCountedITLBVHitsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   ITLB_vhits(const ITLB_vhits &);
   ITLB_vhits& operator=(const ITLB_vhits &);

 public:
   ITLB_vhits(unsigned iid, unsigned iclusterid,
                const countedITLBHits &icountedITLBHits) :
      vevents(iid, iclusterid,
              "I-TLB Hits",
              "This metric calculates the number of I-TLB hits in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "hits/sec", // units when showing current values
              "hits", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedITLBHits
              ),
      theCountedITLBVHitsSimpleMetric(icountedITLBHits) {
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

class ITLB_vhits_perinvoc : public veventsPerInvoc {
 private:
   ITLB_vhits_perinvoc(const ITLB_vhits_perinvoc &);
   ITLB_vhits_perinvoc &operator=(const ITLB_vhits_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t itlb_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = itlb_hits;
         result /= exits;

         return result;
      }
   }

 public:
   ITLB_vhits_perinvoc(unsigned iid, unsigned iclusterid,
                       const countedITLBHits &icountedITLBHits,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "I-TLB Hits/invoc",
                      "This metric calculates the number of I-TLB hits, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"I-TLB Hits\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedITLBHits,
                      iAdd1AtExitSimpleMetric) {
   }
};

class ITLB_vhits_uncacheable : public vevents {
 private:
   const countedITLBHitsUC &theCountedITLBVHitsUncacheableSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   ITLB_vhits_uncacheable(const ITLB_vhits_uncacheable &);
   ITLB_vhits_uncacheable& operator=(const ITLB_vhits_uncacheable &);

 public:
   ITLB_vhits_uncacheable(unsigned iid, unsigned iclusterid,
                const countedITLBHitsUC &icountedITLBHitsUC) :
      vevents(iid, iclusterid,
              "I-TLB Hits (uncacheable)",
              "This metric calculates the number of I-TLB hits for uncacheable memory addresses in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so hits accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "hits/sec", // units when showing current values
              "hits", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedITLBHitsUC
              ),
      theCountedITLBVHitsUncacheableSimpleMetric(icountedITLBHitsUC) {
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

class ITLB_vhits_uncacheable_perinvoc : public veventsPerInvoc {
 private:
   ITLB_vhits_uncacheable_perinvoc(const ITLB_vhits_uncacheable_perinvoc &);
   ITLB_vhits_uncacheable_perinvoc &operator=(const ITLB_vhits_uncacheable_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t itlb_hits = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = itlb_hits;
         result /= exits;

         return result;
      }
   }

 public:
   ITLB_vhits_uncacheable_perinvoc(unsigned iid, unsigned iclusterid,
                                   const countedITLBHitsUC &icountedITLBHitsUC,
                                   const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "I-TLB Hits (uncacheable)/invoc",
                      "This metric calculates the number of I-TLB hits for uncacheable memory addresses, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"I-TLB Hits (uncacheable)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "hits/invoc", // units when showing current values
                      "hits/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedITLBHitsUC,
                      iAdd1AtExitSimpleMetric) {
   }
};

class ITLB_vmisses : public vevents {
 private:
   const countedITLBMisses &theCountedITLBVMissesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   ITLB_vmisses(const ITLB_vmisses &);
   ITLB_vmisses& operator=(const ITLB_vmisses &);

 public:
   ITLB_vmisses(unsigned iid, unsigned iclusterid,
                const countedITLBMisses &icountedITLBMisses) :
      vevents(iid, iclusterid,
              "I-TLB Misses",
              "This metric calculates the number of I-TLB misses in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "misses/sec", // units when showing current values
              "misses", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedITLBMisses
              ),
      theCountedITLBVMissesSimpleMetric(icountedITLBMisses) {
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

class ITLB_vmisses_perinvoc : public veventsPerInvoc {
 private:
   ITLB_vmisses_perinvoc(const ITLB_vmisses_perinvoc &);
   ITLB_vmisses_perinvoc &operator=(const ITLB_vmisses_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#misses);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t itlb_misses = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = itlb_misses;
         result /= exits;

         return result;
      }
   }

 public:
   ITLB_vmisses_perinvoc(unsigned iid, unsigned iclusterid,
                       const countedITLBMisses &icountedITLBMisses,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "I-TLB Misses/invoc",
                      "This metric calculates the number of I-TLB misses, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"I-TLB Misses\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "misses/invoc", // units when showing current values
                      "misses/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedITLBMisses,
                      iAdd1AtExitSimpleMetric) {
   }
};

class ITLB_vmiss_page_walks : public vevents {
 private:
   const countedITLBMissPageWalks &theCountedITLBVMissPageWalksSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   ITLB_vmiss_page_walks(const ITLB_vmiss_page_walks &);
   ITLB_vmiss_page_walks& operator=(const ITLB_vmiss_page_walks &);

 public:
   ITLB_vmiss_page_walks(unsigned iid, unsigned iclusterid,
                const countedITLBMissPageWalks &icountedITLBMissPageWalks) :
      vevents(iid, iclusterid,
              "I-TLB Miss Page Walks",
              "This metric calculates the number of I-TLB miss page walks in a desired piece of code (which can be a function or basic block). Contrast this with the I-TLB miss metric, which counts strictly the number of misses in the TLB, whereas this metric counts only those misses that cause a page to be fetched.\n\n\tAs you would expect, this metric is properly virtualized, so miss page walks accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "page walks/sec", // units when showing current values
              "page walks", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedITLBMissPageWalks
              ),
      theCountedITLBVMissPageWalksSimpleMetric(icountedITLBMissPageWalks) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of miss page walks (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class ITLB_vmiss_page_walks_perinvoc : public veventsPerInvoc {
 private:
   ITLB_vmiss_page_walks_perinvoc(const ITLB_vmiss_page_walks_perinvoc &);
   ITLB_vmiss_page_walks_perinvoc &operator=(const ITLB_vmiss_page_walks_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#miss_page_walks);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t itlb_miss_page_walks = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = itlb_miss_page_walks;
         result /= exits;

         return result;
      }
   }

 public:
   ITLB_vmiss_page_walks_perinvoc(unsigned iid, unsigned iclusterid,
                     const countedITLBMissPageWalks &icountedITLBMissPageWalks,
                     const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "I-TLB Miss Page Walks/invoc",
                      "This metric calculates the number of I-TLB miss page walks, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"I-TLB Miss Page Walks\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "page walks/invoc", // units when showing current values
                      "page walks/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedITLBMissPageWalks,
                      iAdd1AtExitSimpleMetric) {
   }
};

// ************************ D-TLB metrics **************************

class DTLB_vmiss_page_walks : public vevents {
 private:
   const countedDTLBMissPageWalks &theCountedDTLBVMissPageWalksSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   DTLB_vmiss_page_walks(const DTLB_vmiss_page_walks &);
   DTLB_vmiss_page_walks& operator=(const DTLB_vmiss_page_walks &);

 public:
   DTLB_vmiss_page_walks(unsigned iid, unsigned iclusterid,
                         const countedDTLBMissPageWalks &icountedDTLBMissPageWalks) :
      vevents(iid, iclusterid,
              "D-TLB Miss Page Walks",
              "This metric calculates the number of D-TLB miss page walks in a desired piece of code (which can be a function or basic block). Contrast this with the D-TLB load/store miss metrics, which count strictly the number of load/store misses in the TLB, whereas this metric counts all misses that cause a page to be fetched.\n\n\tAs you would expect, this metric is properly virtualized, so miss_page_walks accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "page walks/sec", // units when showing current values
              "page walks", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedDTLBMissPageWalks
              ),
      theCountedDTLBVMissPageWalksSimpleMetric(icountedDTLBMissPageWalks) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of miss page walks (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class DTLB_vmiss_page_walks_perinvoc : public veventsPerInvoc {
 private:
   DTLB_vmiss_page_walks_perinvoc(const DTLB_vmiss_page_walks_perinvoc &);
   DTLB_vmiss_page_walks_perinvoc &operator=(const DTLB_vmiss_page_walks_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#miss_page_walks);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t dtlb_miss_page_walks = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = dtlb_miss_page_walks;
         result /= exits;

         return result;
      }
   }

 public:
   DTLB_vmiss_page_walks_perinvoc(unsigned iid, unsigned iclusterid,
                     const countedDTLBMissPageWalks &icountedDTLBMissPageWalks,
                     const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "D-TLB Miss Page Walks/invoc",
                      "This metric calculates the number of D-TLB miss page walks, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"D-TLB Miss Page Walks\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "page walks/invoc", // units when showing current values
                      "page walks/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedDTLBMissPageWalks,
                      iAdd1AtExitSimpleMetric) {
   }
};

class DTLB_vload_misses : public vevents {
 private:
   const countedDTLBLoadMisses &theCountedDTLBVLoadMissesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   DTLB_vload_misses(const DTLB_vload_misses &);
   DTLB_vload_misses& operator=(const DTLB_vload_misses &);

 public:
   DTLB_vload_misses(unsigned iid, unsigned iclusterid,
                const countedDTLBLoadMisses &icountedDTLBLoadMisses) :
      vevents(iid, iclusterid,
              "D-TLB Load Misses",
              "This metric calculates the number of D-TLB load misses in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so load misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "misses/sec", // units when showing current values
              "misses", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedDTLBLoadMisses
              ),
      theCountedDTLBVLoadMissesSimpleMetric(icountedDTLBLoadMisses) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of load misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class DTLB_vload_misses_perinvoc : public veventsPerInvoc {
 private:
   DTLB_vload_misses_perinvoc(const DTLB_vload_misses_perinvoc &);
   DTLB_vload_misses_perinvoc &operator=(const DTLB_vload_misses_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#load_misses);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t dtlb_load_misses = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = dtlb_load_misses;
         result /= exits;

         return result;
      }
   }

 public:
   DTLB_vload_misses_perinvoc(unsigned iid, unsigned iclusterid,
                     const countedDTLBLoadMisses &icountedDTLBLoadMisses,
                     const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "D-TLB Load Misses/invoc",
                      "This metric calculates the number of D-TLB load misses, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"D-TLB Load Misses\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "misses/invoc", // units when showing current values
                      "misses/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedDTLBLoadMisses,
                      iAdd1AtExitSimpleMetric) {
   }
};

class DTLB_vstore_misses : public vevents {
 private:
   const countedDTLBStoreMisses &theCountedDTLBVStoreMissesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   DTLB_vstore_misses(const DTLB_vstore_misses &);
   DTLB_vstore_misses& operator=(const DTLB_vstore_misses &);

 public:
   DTLB_vstore_misses(unsigned iid, unsigned iclusterid,
                const countedDTLBStoreMisses &icountedDTLBStoreMisses) :
      vevents(iid, iclusterid,
              "D-TLB Store Misses",
              "This metric calculates the number of D-TLB store misses in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so store misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "misses/sec", // units when showing current values
              "misses", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedDTLBStoreMisses
              ),
      theCountedDTLBVStoreMissesSimpleMetric(icountedDTLBStoreMisses) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of store misses (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class DTLB_vstore_misses_perinvoc : public veventsPerInvoc {
 private:
   DTLB_vstore_misses_perinvoc(const DTLB_vstore_misses_perinvoc &);
   DTLB_vstore_misses_perinvoc &operator=(const DTLB_vstore_misses_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#store_misses);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t dtlb_store_misses = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = dtlb_store_misses;
         result /= exits;

         return result;
      }
   }

 public:
   DTLB_vstore_misses_perinvoc(unsigned iid, unsigned iclusterid,
                     const countedDTLBStoreMisses &icountedDTLBStoreMisses,
                     const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "D-TLB Store Misses/invoc",
                      "This metric calculates the number of D-TLB store misses, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"D-TLB Store Misses\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "misses/invoc", // units when showing current values
                      "misses/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedDTLBStoreMisses,
                      iAdd1AtExitSimpleMetric) {
   }
};


#endif
