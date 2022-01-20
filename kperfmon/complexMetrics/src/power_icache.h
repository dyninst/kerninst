class L1_instruction_vreads : public vevents {
 private:
   const countedL1InstructionReads &theCountedL1InstructionReadsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L1_instruction_vreads(const L1_instruction_vreads &);
   L1_instruction_vreads& operator=(const L1_instruction_vreads &);

 public:
   L1_instruction_vreads(unsigned iid, unsigned iclusterid,
                const countedL1InstructionReads &icountedL1InstructionReads) :
      vevents(iid, iclusterid,
              "L1-I-$ Reads",
              "This metric calculates the total number of L1 I-cache read references in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache references accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "reads/sec", // units when showing current values
              "reads", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL1InstructionReads
              ),
      theCountedL1InstructionReadsSimpleMetric(icountedL1InstructionReads) {
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

class L1_instruction_vreads_perinvoc : public veventsPerInvoc {
 private:
   L1_instruction_vreads_perinvoc(const L1_instruction_vreads_perinvoc &);
   L1_instruction_vreads_perinvoc &operator=(const L1_instruction_vreads_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_refs = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_refs;
         result /= exits;

         return result;
      }
   }

 public:
   L1_instruction_vreads_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL1InstructionReads &icountedL1InstructionReads,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L1-I-$ Reads/invoc",
                      "This metric calculates the number of total L1 I-cache read references, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L1-I-$ Reads\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "reads/invoc", // units when showing current values
                      "reads/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL1InstructionReads,
                      iAdd1AtExitSimpleMetric) {
   }
};



class L2_instruction_vreads : public vevents {
 private:
   const countedL2InstructionReads &theCountedL2InstructionReadsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L2_instruction_vreads(const L2_instruction_vreads &);
   L2_instruction_vreads& operator=(const L2_instruction_vreads &);

 public:
   L2_instruction_vreads(unsigned iid, unsigned iclusterid,
                const countedL2InstructionReads &icountedL2InstructionReads) :
      vevents(iid, iclusterid,
              "L2-I-$ Reads",
              "This metric calculates the total number of L1 I-cache read references in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache references accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "reads/sec", // units when showing current values
              "reads", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL2InstructionReads
              ),
      theCountedL2InstructionReadsSimpleMetric(icountedL2InstructionReads) {
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

class L2_instruction_vreads_perinvoc : public veventsPerInvoc {
 private:
   L2_instruction_vreads_perinvoc(const L2_instruction_vreads_perinvoc &);
   L2_instruction_vreads_perinvoc &operator=(const L2_instruction_vreads_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_refs = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_refs;
         result /= exits;

         return result;
      }
   }

 public:
   L2_instruction_vreads_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL2InstructionReads &icountedL2InstructionReads,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L2-I-$ Reads/invoc",
                      "This metric calculates the number of total L2 I-cache read references, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L2-I-$ Reads\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "reads/invoc", // units when showing current values
                      "reads/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL2InstructionReads,
                      iAdd1AtExitSimpleMetric) {
   }
};


class L3_instruction_vreads : public vevents {
 private:
   const countedL3InstructionReads &theCountedL3InstructionReadsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L3_instruction_vreads(const L3_instruction_vreads &);
   L3_instruction_vreads& operator=(const L3_instruction_vreads &);

 public:
   L3_instruction_vreads(unsigned iid, unsigned iclusterid,
                const countedL3InstructionReads &icountedL3InstructionReads) :
      vevents(iid, iclusterid,
              "L3-I-$ Reads",
              "This metric calculates the total number of L3 I-cache read references in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache references accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "reads/sec", // units when showing current values
              "reads", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL3InstructionReads
              ),
      theCountedL3InstructionReadsSimpleMetric(icountedL3InstructionReads) {
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

class L3_instruction_vreads_perinvoc : public veventsPerInvoc {
 private:
   L3_instruction_vreads_perinvoc(const L3_instruction_vreads_perinvoc &);
   L3_instruction_vreads_perinvoc &operator=(const L3_instruction_vreads_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_refs = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_refs;
         result /= exits;

         return result;
      }
   }

 public:
   L3_instruction_vreads_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL3InstructionReads &icountedL3InstructionReads,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L3-I-$ Reads/invoc",
                      "This metric calculates the number of total L3 I-cache read references, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L3-I-$ Reads\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "reads/invoc", // units when showing current values
                      "reads/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL3InstructionReads,
                      iAdd1AtExitSimpleMetric) {
   }
};

class Mem_instruction_vreads : public vevents {
 private:
   const countedMemInstructionReads &theCountedMemInstructionReadsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   Mem_instruction_vreads(const Mem_instruction_vreads &);
   Mem_instruction_vreads& operator=(const Mem_instruction_vreads &);

 public:
   Mem_instruction_vreads(unsigned iid, unsigned iclusterid,
                const countedMemInstructionReads &icountedMemInstructionReads) :
      vevents(iid, iclusterid,
              "Memory Instr. Reads",
              "This metric calculates the total number of instruction read references that go to memory in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache references accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "reads/sec", // units when showing current values
              "reads", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedMemInstructionReads
              ),
      theCountedMemInstructionReadsSimpleMetric(icountedMemInstructionReads) {
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

class Mem_instruction_vreads_perinvoc : public veventsPerInvoc {
 private:
   Mem_instruction_vreads_perinvoc(const Mem_instruction_vreads_perinvoc &);
   Mem_instruction_vreads_perinvoc &operator=(const Mem_instruction_vreads_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t mem_refs = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = mem_refs;
         result /= exits;

         return result;
      }
   }

 public:
   Mem_instruction_vreads_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedMemInstructionReads &icountedMemInstructionReads,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "Mem Instr. Reads/invoc",
                      "This metric calculates the number of total instruction read references that to go memory, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"Mem Instr. Reads\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "reads/invoc", // units when showing current values
                      "reads/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedMemInstructionReads,
                      iAdd1AtExitSimpleMetric) {
   }
};
