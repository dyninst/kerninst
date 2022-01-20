class L1_vread_misses : public vevents {
 private:
   const countedL1ReadMisses &theCountedL1ReadMissesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L1_vread_misses(const L1_vread_misses &);
   L1_vread_misses& operator=(const L1_vread_misses &);

 public:
   L1_vread_misses(unsigned iid, unsigned iclusterid,
                const countedL1ReadMisses &icountedL1ReadMisses) :
      vevents(iid, iclusterid,
              "L1-D-$ Misses",
              "This metric calculates the total number of L1 data cache misses in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache misses accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "misses/sec", // units when showing current values
              "misses", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL1ReadMisses
              ),
      theCountedL1ReadMissesSimpleMetric(icountedL1ReadMisses) {
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
                      "L1-D-$ Misses/invoc",
                      "This metric calculates the number of total L1 data cache misses, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L1-D-$ Misses\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "misses/invoc", // units when showing current values
                      "misses/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL1ReadMisses,
                      iAdd1AtExitSimpleMetric) {
   }
};



class L2_vcastouts : public vevents {
 private:
   const countedL2Castouts &theCountedL2CastoutsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L2_vcastouts(const L2_vcastouts &);
   L2_vcastouts& operator=(const L2_vcastouts &);

 public:
   L2_vcastouts(unsigned iid, unsigned iclusterid,
                const countedL2Castouts &icountedL2Castouts) :
      vevents(iid, iclusterid,
              "L2-D-$ Castouts",
              "This metric calculates the total number of L2 data cache castouts in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache castouts accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "castouts/sec", // units when showing current values
              "castouts", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL2Castouts
              ),
      theCountedL2CastoutsSimpleMetric(icountedL2Castouts) {
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

class L2_vcastouts_perinvoc : public veventsPerInvoc {
 private:
   L2_vcastouts_perinvoc(const L2_vcastouts_perinvoc &);
   L2_vcastouts_perinvoc &operator=(const L2_vcastouts_perinvoc &);
   
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
   L2_vcastouts_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL2Castouts &icountedL2Castouts,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L2-D-$ Castouts/invoc",
                      "This metric calculates the number of total L2 data cache castouts, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L2-D-$ castouts\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "castouts/invoc", // units when showing current values
                      "castouts/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL2Castouts,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L1_vreads : public vevents {
 private:
   const countedL1Reads &theCountedL1ReadsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L1_vreads(const L1_vreads &);
   L1_vreads& operator=(const L1_vreads &);

 public:
   L1_vreads(unsigned iid, unsigned iclusterid,
                const countedL1Reads &icountedL1Reads) :
      vevents(iid, iclusterid,
              "L1-D-$ Reads",
              "This metric calculates the total number of L1 data cache read references in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache references accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "reads/sec", // units when showing current values
              "reads", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL1Reads
              ),
      theCountedL1ReadsSimpleMetric(icountedL1Reads) {
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

class L1_vreads_perinvoc : public veventsPerInvoc {
 private:
   L1_vreads_perinvoc(const L1_vreads_perinvoc &);
   L1_vreads_perinvoc &operator=(const L1_vreads_perinvoc &);
   
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
   L1_vreads_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL1Reads &icountedL1Reads,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L1-D-$ Reads/invoc",
                      "This metric calculates the number of total L1 data cache read references, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L1-D-$ Reads\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "reads/invoc", // units when showing current values
                      "reads/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL1Reads,
                      iAdd1AtExitSimpleMetric) {
   }
};


class L1_vwrites : public vevents {
 private:
   const countedL1Writes &theCountedL1WritesSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L1_vwrites(const L1_vwrites &);
   L1_vwrites& operator=(const L1_vwrites &);

 public:
   L1_vwrites(unsigned iid, unsigned iclusterid,
                const countedL1Writes &icountedL1Writes) :
      vevents(iid, iclusterid,
              "L1-D-$ Writes",
              "This metric calculates the total number of L1 data cache write references in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache references accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "writes/sec", // units when showing current values
              "writes", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL1Writes
              ),
      theCountedL1WritesSimpleMetric(icountedL1Writes) {
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

class L1_vwrites_perinvoc : public veventsPerInvoc {
 private:
   L1_vwrites_perinvoc(const L1_vwrites_perinvoc &);
   L1_vwrites_perinvoc &operator=(const L1_vwrites_perinvoc &);
   
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
   L1_vwrites_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL1Writes &icountedL1Writes,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L1-D-$ Writes/invoc",
                      "This metric calculates the number of total L1 data cache write references, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L1-D-$ Writes\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "writes/invoc", // units when showing current values
                      "writes/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL1Writes,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L2_vreads : public vevents {
 private:
   const countedL2Reads &theCountedL2ReadsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L2_vreads(const L2_vreads &);
   L2_vreads& operator=(const L2_vreads &);

 public:
   L2_vreads(unsigned iid, unsigned iclusterid,
                const countedL2Reads &icountedL2Reads) :
      vevents(iid, iclusterid,
              "L2-D-$ Reads",
              "This metric calculates the total number of L2 data cache reads in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache reads accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "reads/sec", // units when showing current values
              "reads", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL2Reads
              ),
      theCountedL2ReadsSimpleMetric(icountedL2Reads) {
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

class L2_vreads_perinvoc : public veventsPerInvoc {
 private:
   L2_vreads_perinvoc(const L2_vreads_perinvoc &);
   L2_vreads_perinvoc &operator=(const L2_vreads_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_reads = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_reads;
         result /= exits;

         return result;
      }
   }

 public:
   L2_vreads_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL2Reads &icountedL2Reads,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L2-D-$ Reads/invoc",
                      "This metric calculates the number of total L2 data cache reads, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L2-D-$ Reads\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "reads/invoc", // units when showing current values
                      "reads/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL2Reads,
                      iAdd1AtExitSimpleMetric) {
   }
};

class L3_vreads : public vevents {
 private:
   const countedL3Reads &theCountedL3ReadsSimpleMetric;
      // base class also keeps this, but as a different type (base).  Keeping
      // the derived type allows us to avoid an ugly dynamic_cast<>

   // prevent copying:
   L3_vreads(const L3_vreads &);
   L3_vreads& operator=(const L3_vreads &);

 public:
   L3_vreads(unsigned iid, unsigned iclusterid,
                const countedL3Reads &icountedL3Reads) :
      vevents(iid, iclusterid,
              "L3-D-$ Reads",
              "This metric calculates the total number of L3 data cache reads in a desired piece of code (which can be a function or basic block).\n\n\tAs you would expect, this metric is properly virtualized, so cache reads accumulated while your code is (for example) blocked waiting for I/O are not counted.\n",
              "reads/sec", // units when showing current values
              "reads", // units when showing total values
              complexMetric::unnormalized,
              complexMetric::sum, // this is how to fold
              icountedL3Reads
              ),
      theCountedL3ReadsSimpleMetric(icountedL3Reads) {
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

class L3_vreads_perinvoc : public veventsPerInvoc {
 private:
   L3_vreads_perinvoc(const L3_vreads_perinvoc &);
   L3_vreads_perinvoc &operator=(const L3_vreads_perinvoc &);
   
   double interpretValForVisiShipment(const pdvector<uint64_t> &vals,
                                      unsigned /* mhz */) const {
      // There are two values.  The first is the numerator (#icache hits);
      // the second is the denominator (#invocs)

      assert(getNormalizedFlag() == complexMetric::normalized);
      
      const uint64_t cache_reads = vals[0];
      const uint64_t exits = vals[1];
      if (exits == 0)
         return 0.0;
      else {
         double result = cache_reads;
         result /= exits;

         return result;
      }
   }

 public:
   L3_vreads_perinvoc(unsigned iid, unsigned iclusterid,
                            const countedL3Reads &icountedL3Reads,
                            const add1AtExit &iAdd1AtExitSimpleMetric) :
      veventsPerInvoc(iid, iclusterid,
                      "L3-D-$ Reads/invoc",
                      "This metric calculates the number of total L3 data cache reads, in a desired piece of code (function or basic block), per invocation of that code.\n\nThis metric is basically a combination of two others; \"L3-D-$ Reads\" divided by \"exitsFrom\"; see those metrics for further details.\n",
                      "reads/invoc", // units when showing current values
                      "reads/invoc secs", // units when showing total values (???)
                      complexMetric::normalized,
                      complexMetric::avg,
                      icountedL3Reads,
                      iAdd1AtExitSimpleMetric) {
   }
};
