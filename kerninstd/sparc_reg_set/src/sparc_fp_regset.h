// sparc_fp_regset.h
// There are 64 single-precision regs, 32 double-precision regs, and 16 quad-precision
// regs.  Saying that there are 64 single-precision regs is kinda controversial,
// because there are no single-precision instructions which read or write just
// one register numbered 32-63.  But that's not to say that those regs don't
// exist; they do exist!  It's just that they can only be modified indirectly
// by double or quad instructions, which modify several registers at a time.

#ifndef _SPARC_FP_REGSET_H_
#define _SPARC_FP_REGSET_H_

#include "sparc_reg.h"
#include <inttypes.h>

struct XDR;

class sparc_fp_regset {
 private:
   uint64_t float_bits;
      // %f0 (lsb) to %f63 (msb).  There are 64 fp regs of 32 bits each in SPARC v9.
      // Sure, sometimes 64-bit fp register operations are performed, but they're
      // just operations on consecutive pairs of 32-bit fp registers, so it's no
      // big deal for this class to handle.  However, it does mess up class sparc_reg
      // big-time, because it can't handle the concept of two registers really being
      // one.  That is, assuming it follows our rule that there are 64 fp regs of 32
      // bits each, it can't represent a 64-bit fp register because it would be *two*
      // fp registers as far as it knows, and that class only holds 1 register.
      // So, is class sparc_reg fundamentally broken for fp regs?  Should we
      // remove fp support from sparc_reg completely, forcing everyone to
      // use this class instead?  Maybe.
   
      // Historical note: in SPARC v8, there were just 32 (not 64) fp regs of 32 bits
      // each.  Only SPARC v9's new double- and quad- fp operations can access
      // registers numbered 32 to 64.

   
 public:
   sparc_fp_regset() {
      float_bits = 0;
   }
   
   class Empty {};
   static Empty empty;
   sparc_fp_regset(Empty) {
      float_bits = 0;
   }

   class Full {};
   static Full full;
   sparc_fp_regset(Full) {
      float_bits = 0xffffffffffffffffULL;
   }

   sparc_fp_regset(const sparc_fp_regset &src) :
      float_bits(src.float_bits) {
   }
   void operator=(const sparc_fp_regset &src) {
      float_bits = src.float_bits;
   }
   
   sparc_fp_regset(XDR *xdr);
   bool send(XDR *xdr) const;

   class Raw {};
   static Raw raw;

   sparc_fp_regset(Raw, uint64_t ifloatbits) : float_bits(ifloatbits) {
   }

   class Random {};
   static Random random;
   
   sparc_fp_regset(Random) {
      // Obviously, a (seemingly crazy) routine like this is only used in
      // regression torture test programs.
      uint32_t val = (unsigned)::random();
      float_bits = val;
      float_bits <<= 32;
   
      val = (unsigned)::random();
      float_bits |= val;
   }

   class SingleFloatReg {};
   static SingleFloatReg singleFloatReg;

   sparc_fp_regset(SingleFloatReg, sparc_reg r); // assumes single-precision
   sparc_fp_regset(SingleFloatReg, unsigned precision, sparc_reg r);

   bool isempty() const {
      return float_bits == 0;
   }
   void clear() {
      float_bits = 0;
   }

   bool operator==(const sparc_reg &r) const {
      // assumes r is single-precision.
      // It's OK for a single precision reg to be numbered 32-63; even
      // though no instruction specifically updates *only* 1 fp reg in
      // that range, it doesn't mean that those regs don't exist -- they do!
      assert(r.isFloatReg());
      const sparc_fp_regset r_asSet(singleFloatReg, r);
      return this->operator==(r_asSet);
   }

   bool operator==(const sparc_fp_regset &src) const {
      return float_bits == src.float_bits;
   }
   bool operator!=(const sparc_fp_regset &src) const {
      return float_bits != src.float_bits;
   }

   uint64_t getRawBits() const {
      return float_bits;
   }

   bool exists(const sparc_fp_regset &subset) const {
      return 0ULL == (subset.float_bits & (~float_bits));
   }
   bool existsSinglePrecision(unsigned num) const {
      return (float_bits & (1ULL << num)) != 0;
   }
   bool existsSinglePrecision(const sparc_reg &r) const {
      return existsSinglePrecision(r.getFloatNum());
   }
   bool exists(const sparc_reg &r, unsigned precision) const {
#ifndef NDEBUG
      const unsigned num = r.getFloatNum();
      assert(num < 64);
      assert(precision == 1 ||
             precision == 2 && num % 2 == 0 ||
             precision == 4 && num % 4 == 0);
#endif

      // This could be optimized better:
      const sparc_fp_regset theSet(singleFloatReg, precision, r);
      return exists(theSet);
   }
   bool exists(unsigned num, unsigned precision) const {
      assert(num < 64);
      assert(precision == 1 ||
             precision == 2 && num % 2 == 0 ||
             precision == 4 && num % 4 == 0);
      const sparc_reg r(sparc_reg::f, num);
      return exists(r, precision);
   }

   void operator|=(const sparc_fp_regset &theSet) {
      float_bits |= theSet.float_bits;
   }
   void operator&=(const sparc_fp_regset &theSet) {
      float_bits &= theSet.float_bits;
   }
   void operator-=(const sparc_fp_regset &theSet) {
      float_bits &= ~theSet.float_bits;
   }

   sparc_reg expand1Reg() const; // smartly detects single v. double v. quad precision
   unsigned countAsSinglePrecision() const;
};

#endif
