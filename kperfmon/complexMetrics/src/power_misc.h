class insn_vcompleted : public vevents {
 private:
   const countedInsnsCompleted &theCountedInsnsCompletedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   insn_vcompleted(const insn_vcompleted &);
   insn_vcompleted& operator=(const insn_vcompleted &);

 public:
   insn_vcompleted(unsigned iid, unsigned iclusterid,
                  const countedInsnsCompleted &iCountedInsnsCompleted) :
      vevents(iid, iclusterid,
              "Instructions Completed",
              "This metric calculates the number of instructions completed in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so instructions completed while your code is (for example) blocked waiting for I/O are not counted.\n",
              "insns/sec", // units when showing current values
              "insns", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCountedInsnsCompleted
              ),
      theCountedInsnsCompletedSimpleMetric(iCountedInsnsCompleted) {
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


class insn_vcompleted_perinvoc : public veventsPerInvoc {
 private:
   insn_vcompleted_perinvoc(const insn_vcompleted_perinvoc &);
   insn_vcompleted_perinvoc &operator=(const insn_vcompleted_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#clears);
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
                       const countedInsnsCompleted &iCountedInsnsCompleted,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Instructions completed/invoc",
                      "This metric calculates the number of instructions completed in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Instructions Completed\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "insns/invoc", // units when showing current values
                      "insns/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedInsnsCompleted,
                      iAdd1AtExitSimpleMetric) {
   }
};


class insn_vdispatched : public vevents {
 private:
   const countedInsnsDispatched &theCountedInsnsDispatchedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   insn_vdispatched(const insn_vcompleted &);
   insn_vdispatched& operator=(const insn_vdispatched &);

 public:
   insn_vdispatched(unsigned iid, unsigned iclusterid,
                  const countedInsnsDispatched &iCountedInsnsDispatched) :
      vevents(iid, iclusterid,
              "Instructions Dispatched",
              "This metric calculates the number of instructions dispatched in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so instructions dispatched while your code is (for example) blocked waiting for I/O are not counted.\n",
              "insns/sec", // units when showing current values
              "insns", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCountedInsnsDispatched
              ),
      theCountedInsnsDispatchedSimpleMetric(iCountedInsnsDispatched) {
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


class insn_vdispatched_perinvoc : public veventsPerInvoc {
 private:
   insn_vdispatched_perinvoc(const insn_vdispatched_perinvoc &);
   insn_vdispatched_perinvoc &operator=(const insn_vdispatched_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#clears);
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
   insn_vdispatched_perinvoc(unsigned iid, unsigned iclusterid,
                       const countedInsnsDispatched &iCountedInsnsDispatched,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Instructions dispatched/invoc",
                      "This metric calculates the number of instructions dispatched in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Instructions Dispatched\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "insns/invoc", // units when showing current values
                      "insns/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedInsnsDispatched,
                      iAdd1AtExitSimpleMetric) {
   }
};

class insn_vprefetched : public vevents {
 private:
   const countedInsnsPrefetched &theCountedInsnsPrefetchedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   insn_vprefetched(const insn_vcompleted &);
   insn_vprefetched& operator=(const insn_vprefetched &);

 public:
   insn_vprefetched(unsigned iid, unsigned iclusterid,
                  const countedInsnsPrefetched &iCountedInsnsPrefetched) :
      vevents(iid, iclusterid,
              "Instructions Prefetched",
              "This metric calculates the number of instructions prefetched in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so instructions prefetched while your code is (for example) blocked waiting for I/O are not counted.\n",
              "insns/sec", // units when showing current values
              "insns", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCountedInsnsPrefetched
              ),
      theCountedInsnsPrefetchedSimpleMetric(iCountedInsnsPrefetched) {
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


class insn_vprefetched_perinvoc : public veventsPerInvoc {
 private:
   insn_vprefetched_perinvoc(const insn_vprefetched_perinvoc &);
   insn_vprefetched_perinvoc &operator=(const insn_vprefetched_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#clears);
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
   insn_vprefetched_perinvoc(unsigned iid, unsigned iclusterid,
                       const countedInsnsPrefetched &iCountedInsnsPrefetched,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Instructions prefetched/invoc",
                      "This metric calculates the number of instructions prefetched in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Instructions prefetched\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "insns/invoc", // units when showing current values
                      "insns/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedInsnsPrefetched,
                      iAdd1AtExitSimpleMetric) {
   }
};


class insn_vunfetched : public vevents {
 private:
   const countedInsnsUnfetched &theCountedInsnsUnfetchedSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   insn_vunfetched(const insn_vunfetched &);
   insn_vunfetched& operator=(const insn_vunfetched &);

 public:
   insn_vunfetched(unsigned iid, unsigned iclusterid,
                  const countedInsnsUnfetched &iCountedInsnsUnfetched) :
      vevents(iid, iclusterid,
              "No fetch cycles",
              "This metric calculates the number of cycle during which no instructions were fetched in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cycles while your code is (for example) blocked waiting for I/O are not counted.\n",
              "cycles/sec", // units when showing current values
              "cycles", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              iCountedInsnsUnfetched
              ),
      theCountedInsnsUnfetchedSimpleMetric(iCountedInsnsUnfetched) {
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


class insn_vunfetched_perinvoc : public veventsPerInvoc {
 private:
   insn_vunfetched_perinvoc(const insn_vunfetched_perinvoc &);
   insn_vunfetched_perinvoc &operator=(const insn_vunfetched_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#clears);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cycles = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cycles;
         result /= exits;

         return result;
      }
   }

 public:
   insn_vunfetched_perinvoc(unsigned iid, unsigned iclusterid,
                       const countedInsnsUnfetched &iCountedInsnsUnfetched,
                       const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "No Fetch cycles/invoc",
                      "This metric calculates the number of cycles during which no instructions were fetched in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"No fetch cycles\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "cycles/invoc", // units when showing current values
                      "cycles/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      iCountedInsnsUnfetched,
                      iAdd1AtExitSimpleMetric) {
   }
};

