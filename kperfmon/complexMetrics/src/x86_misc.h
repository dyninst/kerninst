// x86_misc.h - miscellaneous x86 complex metrics

#ifndef _X86_MISC_H_
#define _X86_MISC_H_

#include "vevents.h"
#include "countedVEvents.h"
#include "veventsPerVEvents.h"

// ************************ Memory Access metrics **************************

class Memory_vloads : public vevents {
 private:
   const countedMemLoads &theCountedMemoryVLoadsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Memory_vloads(const Memory_vloads &);
   Memory_vloads& operator=(const Memory_vloads &);

 public:
   Memory_vloads(unsigned iid, unsigned iclusterid,
                const countedMemLoads &icountedMemLoads) :
      vevents(iid, iclusterid,
              "Memory Loads",
              "This metric calculates the number of memory load micro-ops in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so loads accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "loads/sec", // units when showing current values
              "loads", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedMemLoads
              ),
      theCountedMemoryVLoadsSimpleMetric(icountedMemLoads) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of loads (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class Memory_vloads_perinvoc : public veventsPerInvoc {
 private:
   Memory_vloads_perinvoc(const Memory_vloads_perinvoc &);
   Memory_vloads_perinvoc &operator=(const Memory_vloads_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#loads);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t loads = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = loads;
         result /= exits;

         return result;
      }
   }

 public:
   Memory_vloads_perinvoc(unsigned iid, unsigned iclusterid,
                     const countedMemLoads &icountedMemLoads,
                     const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Memory Loads/invoc",
                      "This metric calculates the number of memory load micro-ops, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Memory Loads\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "loads/invoc", // units when showing current values
                      "loads/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedMemLoads,
                      iAdd1AtExitSimpleMetric) {
   }
};

class Memory_vstores : public vevents {
 private:
   const countedMemStores &theCountedMemoryVStoresSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Memory_vstores(const Memory_vstores &);
   Memory_vstores& operator=(const Memory_vstores &);

 public:
   Memory_vstores(unsigned iid, unsigned iclusterid,
                  const countedMemStores &icountedMemStores) :
      vevents(iid, iclusterid,
              "Memory Stores",
              "This metric calculates the number of memory store micro-ops in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so stores accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "stores/sec", // units when showing current values
              "stores", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedMemStores
              ),
      theCountedMemoryVStoresSimpleMetric(icountedMemStores) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of stores (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class Memory_vstores_perinvoc : public veventsPerInvoc {
 private:
   Memory_vstores_perinvoc(const Memory_vstores_perinvoc &);
   Memory_vstores_perinvoc &operator=(const Memory_vstores_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#stores);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t stores = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = stores;
         result /= exits;

         return result;
      }
   }

 public:
   Memory_vstores_perinvoc(unsigned iid, unsigned iclusterid,
                           const countedMemStores &icountedMemStores,
                           const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Memory Stores/invoc",
                      "This metric calculates the number of memory store micro-ops, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Memory Stores\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "stores/invoc", // units when showing current values
                      "stores/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedMemStores,
                      iAdd1AtExitSimpleMetric) {
   }
};

/* ------------------------ Instruction Metrics ------------------------- */

class insn_vuops : public vevents {
 private:
   const countedVuops &theCountedInsnVuopsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   insn_vuops(const insn_vuops &);
   insn_vuops& operator=(const insn_vuops &);

 public:
   insn_vuops(unsigned iid, unsigned iclusterid,
              const countedVuops &icountedVuops) :
      vevents(iid, iclusterid,
              "Instruction micro-ops",
              "This metric calculates the number of instruction micro-ops retired in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so micro-ops accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "uops/sec", // units when showing current values
              "uops", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedVuops
              ),
      theCountedInsnVuopsSimpleMetric(icountedVuops) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of uops (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class insn_vuops_perinvoc : public veventsPerInvoc {
 private:
   insn_vuops_perinvoc(const insn_vuops_perinvoc &);
   insn_vuops_perinvoc &operator=(const insn_vuops_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#uops);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t uops = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = uops;
         result /= exits;

         return result;
      }
   }

 public:
   insn_vuops_perinvoc(unsigned iid, unsigned iclusterid,
                       const countedVuops &icountedVuops,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Instruction Micro-ops/invoc",
                      "This metric calculates the number of instruction micro-ops retired, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Instruction Micro-ops\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "uops/invoc", // units when showing current values
                      "uops/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedVuops,
                      iAdd1AtExitSimpleMetric) {
   }
};

class insn_vretired : public vevents {
 private:
   const countedVInsns &theCountedVInsnsRetiredSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   insn_vretired(const insn_vretired &);
   insn_vretired& operator=(const insn_vretired &);

 public:
   insn_vretired(unsigned iid, unsigned iclusterid,
                 const countedVInsns &icountedVInsns) :
      vevents(iid, iclusterid,
              "Instructions retired",
              "This metric calculates the number of retired instructions in a desired piece of code (which can be a function or basic block). The metric may count more than once for some IA-32 instructions with complex uop flows that were interrupted before retirement.\n\n\tAs you would expect, this metric is properly virtualized, so retires accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "insns/sec", // units when showing current values
              "insns", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedVInsns
              ),
      theCountedVInsnsRetiredSimpleMetric(icountedVInsns) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of insns (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class insn_vretired_perinvoc : public veventsPerInvoc {
 private:
   insn_vretired_perinvoc(const insn_vretired_perinvoc &);
   insn_vretired_perinvoc &operator=(const insn_vretired_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#insns);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t insns = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = insns;
         result /= exits;

         return result;
      }
   }

 public:
   insn_vretired_perinvoc(unsigned iid, unsigned iclusterid,
                       const countedVInsns &icountedVInsns,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Instructions retired/invoc",
                      "This metric calculates the number of retired instructions, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Instructions retired\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "insns/invoc", // units when showing current values
                      "insns/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedVInsns,
                      iAdd1AtExitSimpleMetric) {
   }
};

class insn_vcompleted : public vevents {
 private:
   const countedVInsnsCompleted &theCountedVInsnsCompletedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   insn_vcompleted(const insn_vcompleted &);
   insn_vcompleted& operator=(const insn_vcompleted &);

 public:
   insn_vcompleted(unsigned iid, unsigned iclusterid,
                 const countedVInsnsCompleted &icountedVInsnsCompleted) :
      vevents(iid, iclusterid,
              "Instructions completed",
              "This metric calculates the number of completed instructions retired in a desired piece of code (which can be a function or basic block). This metric is available only on P4/Xeon systems with model encoding 3.\n\n\tAs you would expect, this metric is properly virtualized, so retires accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "insns/sec", // units when showing current values
              "insns", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedVInsnsCompleted
              ),
      theCountedVInsnsCompletedSimpleMetric(icountedVInsnsCompleted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of insns (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class insn_vcompleted_perinvoc : public veventsPerInvoc {
 private:
   insn_vcompleted_perinvoc(const insn_vcompleted_perinvoc &);
   insn_vcompleted_perinvoc &operator=(const insn_vcompleted_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#insns);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t insns = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = insns;
         result /= exits;

         return result;
      }
   }

 public:
   insn_vcompleted_perinvoc(unsigned iid, unsigned iclusterid,
                       const countedVInsnsCompleted &icountedVInsnsCompleted,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Instructions completed/invoc",
                      "This metric calculates the number of completed instructions retired, in a desired piece of code (function or basic block), per invocation of that code. This metric is available only on Pentium4 systems with model encoding 3.\n\nThis metric is basically a combination of two others; \"Instructions completed\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "insns/invoc", // units when showing current values
                      "insns/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedVInsnsCompleted,
                      iAdd1AtExitSimpleMetric) {
   }
};

class pipeline_vclears : public vevents {
 private:
   const countedPipelineClears &theCountedPipelineClearsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   pipeline_vclears(const pipeline_vclears &);
   pipeline_vclears& operator=(const pipeline_vclears &);

 public:
   pipeline_vclears(unsigned iid, unsigned iclusterid,
                  const countedPipelineClears &icountedPipelineClears) :
      vevents(iid, iclusterid,
              "Pipeline Clears",
              "This metric calculates the number of instruction pipeline clears in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so clears accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "clears/sec", // units when showing current values
              "clears", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedPipelineClears
              ),
      theCountedPipelineClearsSimpleMetric(icountedPipelineClears) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of clears (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class pipeline_vclears_perinvoc : public veventsPerInvoc {
 private:
   pipeline_vclears_perinvoc(const pipeline_vclears_perinvoc &);
   pipeline_vclears_perinvoc &operator=(const pipeline_vclears_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#clears);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t clears = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = clears;
         result /= exits;

         return result;
      }
   }

 public:
   pipeline_vclears_perinvoc(unsigned iid, unsigned iclusterid,
                             const countedPipelineClears &icountedPipelineClears,
                             const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Pipeline Clears/invoc",
                      "This metric calculates the number of instruction pipeline clears, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Pipeline Clears\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "clears/invoc", // units when showing current values
                      "clears/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedPipelineClears,
                      iAdd1AtExitSimpleMetric) {
   }
};

#endif
