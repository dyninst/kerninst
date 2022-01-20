// x86_branch.h - complex metrics related to x86 branches

#ifndef _X86_BRANCH_H_
#define _X86_BRANCH_H_

#include "vevents.h"
#include "countedVEvents.h"
#include "veventsPerVEvents.h"

/* -------------------- Retired Branches By Type Metrics --------------------*/

class CondBranch_mispredicts : public vevents {
 private:
   const countedCondBranchesMispredicted &theCondBranchMispredictsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   CondBranch_mispredicts(const CondBranch_mispredicts &);
   CondBranch_mispredicts& operator=(const CondBranch_mispredicts &);

 public:
   CondBranch_mispredicts(unsigned iid, unsigned iclusterid,
              const countedCondBranchesMispredicted &iCondBranchMispredicted) :
      vevents(iid, iclusterid,
              "Conditional Branches (mispredicted)",
              "This metric calculates the number of retired mispredicted conditional branches in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so mispredicted branches accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "mispredicted branches/sec", // units when showing current values
              "mispredicted branches", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCondBranchMispredicted
              ),
      theCondBranchMispredictsSimpleMetric(iCondBranchMispredicted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of mispredicted branches (not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class CondBranch_mispredicts_perinvoc : public veventsPerInvoc {
 private:
   CondBranch_mispredicts_perinvoc(const CondBranch_mispredicts_perinvoc &);
   CondBranch_mispredicts_perinvoc &operator=(const CondBranch_mispredicts_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#mispredicts);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t mispredicts = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = mispredicts;
         result /= exits;

         return result;
      }
   }

 public:
   CondBranch_mispredicts_perinvoc(unsigned iid, unsigned iclusterid,
                const countedCondBranchesMispredicted &iCondBranchMispredicted,
                                   const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Conditional Branches (mispredicted)/invoc",
                      "This metric calculates the number of retired mispredicted conditional branches, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Conditional Branches (mispredicted)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "mispredicted branches/invoc", // units when showing current values
                      "mispredicted branches/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCondBranchMispredicted,
                      iAdd1AtExitSimpleMetric) {
   }
};

class CondBranches : public vevents {
 private:
   const countedCondBranches &theCondBranchesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   CondBranches(const CondBranches &);
   CondBranches& operator=(const CondBranches &);

 public:
   CondBranches(unsigned iid, unsigned iclusterid,
              const countedCondBranches &iCondBranches) :
      vevents(iid, iclusterid,
              "Conditional Branches",
              "This metric calculates the number of retired conditional branches in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so branches accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "conditional branches/sec", // units when showing current values
              "conditional branches", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCondBranches
              ),
      theCondBranchesSimpleMetric(iCondBranches) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of branches (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class CondBranches_perinvoc : public veventsPerInvoc {
 private:
   CondBranches_perinvoc(const CondBranches_perinvoc &);
   CondBranches_perinvoc &operator=(const CondBranches_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#branches);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t branches = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = branches;
         result /= exits;

         return result;
      }
   }

 public:
   CondBranches_perinvoc(unsigned iid, unsigned iclusterid,
                         const countedCondBranches &iCondBranches,
                         const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Conditional Branches/invoc",
                      "This metric calculates the number of retired conditional branches, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Conditional Branches\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "conditional branches/invoc", // units when showing current values
                      "conditional branches/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCondBranches,
                      iAdd1AtExitSimpleMetric) {
   }
};

class Call_mispredicts : public vevents {
 private:
   const countedCallsMispredicted &theCallMispredictsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Call_mispredicts(const Call_mispredicts &);
   Call_mispredicts& operator=(const Call_mispredicts &);

 public:
   Call_mispredicts(unsigned iid, unsigned iclusterid,
                    const countedCallsMispredicted &iCallsMispredicted) :
      vevents(iid, iclusterid,
              "Indirect Calls (mispredicted)",
              "This metric calculates the number of retired mispredicted indirect calls in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so mispredicted calls accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "mispredicted calls/sec", // units when showing current values
              "mispredicted calls", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCallsMispredicted
              ),
      theCallMispredictsSimpleMetric(iCallsMispredicted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of mispredicted calls (not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class Call_mispredicts_perinvoc : public veventsPerInvoc {
 private:
   Call_mispredicts_perinvoc(const Call_mispredicts_perinvoc &);
   Call_mispredicts_perinvoc &operator=(const Call_mispredicts_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#mispredicts);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t mispredicts = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = mispredicts;
         result /= exits;

         return result;
      }
   }

 public:
   Call_mispredicts_perinvoc(unsigned iid, unsigned iclusterid,
                           const countedCallsMispredicted &iCallsMispredicted,
                             const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Indirect Calls (mispredicted)/invoc",
                      "This metric calculates the number of retired mispredicted indirect calls, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Indirect Calls (mispredicted)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "mispredicted calls/invoc", // units when showing current values
                      "mispredicted calls/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCallsMispredicted,
                      iAdd1AtExitSimpleMetric) {
   }
};

class Calls : public vevents {
 private:
   const countedCalls &theCallsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Calls(const Calls &);
   Calls& operator=(const Calls &);

 public:
   Calls(unsigned iid, unsigned iclusterid,
                 const countedCalls &iCalls) :
      vevents(iid, iclusterid,
              "Calls",
              "This metric calculates the number of retired direct or indirect calls in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so calls accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "calls/sec", // units when showing current values
              "calls", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCalls
              ),
      theCallsSimpleMetric(iCalls) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of calls (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class Calls_perinvoc : public veventsPerInvoc {
 private:
   Calls_perinvoc(const Calls_perinvoc &);
   Calls_perinvoc &operator=(const Calls_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#calls);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t calls = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = calls;
         result /= exits;

         return result;
      }
   }

 public:
   Calls_perinvoc(unsigned iid, unsigned iclusterid,
                  const countedCalls &iCalls,
                  const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Calls/invoc",
                      "This metric calculates the number of retired direct or indirect calls, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Calls\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "calls/invoc", // units when showing current values
                      "calls/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCalls,
                      iAdd1AtExitSimpleMetric) {
   }
};

class Return_mispredicts : public vevents {
 private:
   const countedReturnsMispredicted &theReturnMispredictsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Return_mispredicts(const Return_mispredicts &);
   Return_mispredicts& operator=(const Return_mispredicts &);

 public:
   Return_mispredicts(unsigned iid, unsigned iclusterid,
                      const countedReturnsMispredicted &iReturnMispredicted) :
      vevents(iid, iclusterid,
              "Returns (mispredicted)",
              "This metric calculates the number of retired mispredicted return branches in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so mispredicted returns accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "mispredicted returns/sec", // units when showing current values
              "mispredicted returns", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iReturnMispredicted
              ),
      theReturnMispredictsSimpleMetric(iReturnMispredicted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of mispredicted branches (not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class Return_mispredicts_perinvoc : public veventsPerInvoc {
 private:
   Return_mispredicts_perinvoc(const Return_mispredicts_perinvoc &);
   Return_mispredicts_perinvoc &operator=(const Return_mispredicts_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#mispredicts);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t mispredicts = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = mispredicts;
         result /= exits;

         return result;
      }
   }

 public:
   Return_mispredicts_perinvoc(unsigned iid, unsigned iclusterid,
                        const countedReturnsMispredicted &iReturnMispredicted,
                               const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Returns (mispredicted)/invoc",
                      "This metric calculates the number of retired mispredicted return branches, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Returns (mispredicted)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "mispredicted returns/invoc", // units when showing current values
                      "mispredicted returns/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iReturnMispredicted,
                      iAdd1AtExitSimpleMetric) {
   }
};

class Returns : public vevents {
 private:
   const countedReturns &theReturnsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Returns(const Returns &);
   Returns& operator=(const Returns &);

 public:
   Returns(unsigned iid, unsigned iclusterid,
              const countedReturns &iReturns) :
      vevents(iid, iclusterid,
              "Returns",
              "This metric calculates the number of retired return branches in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so returns accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "return branches/sec", // units when showing current values
              "return branches", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iReturns
              ),
      theReturnsSimpleMetric(iReturns) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of returns (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class Returns_perinvoc : public veventsPerInvoc {
 private:
   Returns_perinvoc(const Returns_perinvoc &);
   Returns_perinvoc &operator=(const Returns_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#returns);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t branches = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = branches;
         result /= exits;

         return result;
      }
   }

 public:
   Returns_perinvoc(unsigned iid, unsigned iclusterid,
                    const countedReturns &iReturns,
                    const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Returns/invoc",
                      "This metric calculates the number of retired return branches, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Returns\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "return branches/invoc", // units when showing current values
                      "return branches/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iReturns,
                      iAdd1AtExitSimpleMetric) {
   }
};

class Indirect_mispredicts : public vevents {
 private:
   const countedIndirectsMispredicted &theIndirectMispredictsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Indirect_mispredicts(const Indirect_mispredicts &);
   Indirect_mispredicts& operator=(const Indirect_mispredicts &);

 public:
   Indirect_mispredicts(unsigned iid, unsigned iclusterid,
                  const countedIndirectsMispredicted &iIndirectsMispredicted) :
      vevents(iid, iclusterid,
              "Indirects (mispredicted)",
              "This metric calculates the number of retired mispredicted returns, indirect calls, and indirect jumps in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so mispredicted indirects accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "mispredicted indirects/sec", // units when showing current values
              "mispredicted indirects", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iIndirectsMispredicted
              ),
      theIndirectMispredictsSimpleMetric(iIndirectsMispredicted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of mispredicted indirects (not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class Indirect_mispredicts_perinvoc : public veventsPerInvoc {
 private:
   Indirect_mispredicts_perinvoc(const Indirect_mispredicts_perinvoc &);
   Indirect_mispredicts_perinvoc &operator=(const Indirect_mispredicts_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#mispredicts);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t mispredicts = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = mispredicts;
         result /= exits;

         return result;
      }
   }

 public:
   Indirect_mispredicts_perinvoc(unsigned iid, unsigned iclusterid,
                    const countedIndirectsMispredicted &iIndirectsMispredicted,
                                 const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Indirects (mispredicted)/invoc",
                      "This metric calculates the number of retired mispredicted returns, indirect calls, and indirect jumps, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Indirect Calls (mispredicted)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "mispredicted indirects/invoc", // units when showing current values
                      "mispredicted indirects/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iIndirectsMispredicted,
                      iAdd1AtExitSimpleMetric) {
   }
};

class Indirects : public vevents {
 private:
   const countedIndirects &theIndirectsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Indirects(const Indirects &);
   Indirects& operator=(const Indirects &);

 public:
   Indirects(unsigned iid, unsigned iclusterid,
             const countedIndirects &iIndirects) :
      vevents(iid, iclusterid,
              "Indirects",
              "This metric calculates the number of retired returns, indirect calls, and indirect jumps, in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so indirects accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "indirects/sec", // units when showing current values
              "indirects", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iIndirects
              ),
      theIndirectsSimpleMetric(iIndirects) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of indirects (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class Indirects_perinvoc : public veventsPerInvoc {
 private:
   Indirects_perinvoc(const Indirects_perinvoc &);
   Indirects_perinvoc &operator=(const Indirects_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#indirects);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t indirects = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = indirects;
         result /= exits;

         return result;
      }
   }

 public:
   Indirects_perinvoc(unsigned iid, unsigned iclusterid,
                      const countedIndirects &iIndirects,
                      const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Indirects/invoc",
                      "This metric calculates the number of retired returns, indirect calls, and indirect jumps, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Indirects\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "indirects/invoc", // units when showing current values
                      "indirects/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iIndirects,
                      iAdd1AtExitSimpleMetric) {
   }
};

/* ----------------- Retired Branches (taken/predicted) ------------------*/

class BranchesTakenPredicted : public vevents {
 private:
   const countedBranchesTakenPredicted &theBranchesTakenPredictedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   BranchesTakenPredicted(const BranchesTakenPredicted &);
   BranchesTakenPredicted& operator=(const BranchesTakenPredicted &);

 public:
   BranchesTakenPredicted(unsigned iid, unsigned iclusterid,
              const countedBranchesTakenPredicted &iBranchesTakenPredicted) :
      vevents(iid, iclusterid,
              "Branches Taken (predicted)",
              "This metric calculates the number of retired predicted taken branches in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so branches accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "branches/sec", // units when showing current values
              "branches", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iBranchesTakenPredicted
              ),
      theBranchesTakenPredictedSimpleMetric(iBranchesTakenPredicted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of branches (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class BranchesTakenPredicted_perinvoc : public veventsPerInvoc {
 private:
   BranchesTakenPredicted_perinvoc(const BranchesTakenPredicted_perinvoc &);
   BranchesTakenPredicted_perinvoc &operator=(const BranchesTakenPredicted_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#branches);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t branches = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = branches;
         result /= exits;

         return result;
      }
   }

 public:
   BranchesTakenPredicted_perinvoc(unsigned iid, unsigned iclusterid,
                 const countedBranchesTakenPredicted &iBranchesTakenPredicted,
                                   const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Branches Taken (predicted)/invoc",
                      "This metric calculates the number of retired predicted taken branches, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Branches Taken (predicted)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "branches/invoc", // units when showing current values
                      "branches/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iBranchesTakenPredicted,
                      iAdd1AtExitSimpleMetric) {
   }
};

class BranchesTakenMispredicted : public vevents {
 private:
   const countedBranchesTakenMispredicted &theBranchesTakenMispredictedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   BranchesTakenMispredicted(const BranchesTakenMispredicted &);
   BranchesTakenMispredicted& operator=(const BranchesTakenMispredicted &);

 public:
   BranchesTakenMispredicted(unsigned iid, unsigned iclusterid,
         const countedBranchesTakenMispredicted &iBranchesTakenMispredicted) :
      vevents(iid, iclusterid,
              "Branches Taken (mispredicted)",
              "This metric calculates the number of retired mispredicted taken branches in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so branches accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "branches/sec", // units when showing current values
              "branches", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iBranchesTakenMispredicted
              ),
      theBranchesTakenMispredictedSimpleMetric(iBranchesTakenMispredicted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of branches (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class BranchesTakenMispredicted_perinvoc : public veventsPerInvoc {
 private:
   BranchesTakenMispredicted_perinvoc(const BranchesTakenMispredicted_perinvoc &);
   BranchesTakenMispredicted_perinvoc &operator=(const BranchesTakenMispredicted_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#branches);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t branches = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = branches;
         result /= exits;

         return result;
      }
   }

 public:
   BranchesTakenMispredicted_perinvoc(unsigned iid, unsigned iclusterid,
            const countedBranchesTakenMispredicted &iBranchesTakenMispredicted,
            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Branches Taken (mispredicted)/invoc",
                      "This metric calculates the number of retired mispredicted taken branches, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Branches Taken (mispredicted)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "branches/invoc", // units when showing current values
                      "branches/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iBranchesTakenMispredicted,
                      iAdd1AtExitSimpleMetric) {
   }
};

class BranchesNotTakenPredicted : public vevents {
 private:
   const countedBranchesNotTakenPredicted &theBranchesNotTakenPredictedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   BranchesNotTakenPredicted(const BranchesNotTakenPredicted &);
   BranchesNotTakenPredicted& operator=(const BranchesNotTakenPredicted &);

 public:
   BranchesNotTakenPredicted(unsigned iid, unsigned iclusterid,
         const countedBranchesNotTakenPredicted &iBranchesNotTakenPredicted) :
      vevents(iid, iclusterid,
              "Branches Not-Taken (predicted)",
              "This metric calculates the number of retired predicted not-taken branches in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so branches accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "branches/sec", // units when showing current values
              "branches", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iBranchesNotTakenPredicted
              ),
      theBranchesNotTakenPredictedSimpleMetric(iBranchesNotTakenPredicted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of branches (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class BranchesNotTakenPredicted_perinvoc : public veventsPerInvoc {
 private:
   BranchesNotTakenPredicted_perinvoc(const BranchesNotTakenPredicted_perinvoc &);
   BranchesNotTakenPredicted_perinvoc &operator=(const BranchesNotTakenPredicted_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#branches);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t branches = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = branches;
         result /= exits;

         return result;
      }
   }

 public:
   BranchesNotTakenPredicted_perinvoc(unsigned iid, unsigned iclusterid,
           const countedBranchesNotTakenPredicted &iBranchesNotTakenPredicted,
           const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Branches Not-Taken (predicted)/invoc",
                      "This metric calculates the number of retired predicted not-taken branches, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Branches Not-Taken (predicted)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "branches/invoc", // units when showing current values
                      "branches/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iBranchesNotTakenPredicted,
                      iAdd1AtExitSimpleMetric) {
   }
};

class BranchesNotTakenMispredicted : public vevents {
 private:
   const countedBranchesNotTakenMispredicted &theBranchesNotTakenMispredictedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   BranchesNotTakenMispredicted(const BranchesNotTakenMispredicted &);
   BranchesNotTakenMispredicted& operator=(const BranchesNotTakenMispredicted &);

 public:
   BranchesNotTakenMispredicted(unsigned iid, unsigned iclusterid,
         const countedBranchesNotTakenMispredicted &iBranchesNotTakenMispredicted) :
      vevents(iid, iclusterid,
              "Branches Not-Taken (mispredicted)",
              "This metric calculates the number of retired mispredicted not-taken branches in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so branches accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "branches/sec", // units when showing current values
              "branches", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iBranchesNotTakenMispredicted
              ),
      theBranchesNotTakenMispredictedSimpleMetric(iBranchesNotTakenMispredicted) {
   }

   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /*mhz*/) const {
      // sampled raw value is number of branches (events, not cycles) 
      // during this bucket, so we don't divide by mhz or anything like that.

      assert(getNormalizedFlag() == complexMetric::unnormalized);
      assert(vals.size() == 1);
      return vals[0];
   }
};

class BranchesNotTakenMispredicted_perinvoc : public veventsPerInvoc {
 private:
   BranchesNotTakenMispredicted_perinvoc(const BranchesNotTakenMispredicted_perinvoc &);
   BranchesNotTakenMispredicted_perinvoc &operator=(const BranchesNotTakenMispredicted_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#branches);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t branches = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = branches;
         result /= exits;

         return result;
      }
   }

 public:
   BranchesNotTakenMispredicted_perinvoc(unsigned iid, unsigned iclusterid,
      const countedBranchesNotTakenMispredicted &iBranchesNotTakenMispredicted,
      const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Branches Not-Taken (mispredicted)/invoc",
                      "This metric calculates the number of retired mispredicted not-taken branches, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Branches Not-Taken (mispredicted)\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "branches/invoc", // units when showing current values
                      "branches/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iBranchesNotTakenMispredicted,
                      iAdd1AtExitSimpleMetric) {
   }
};

#endif
