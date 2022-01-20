// power_fp_regset.h
//There are 32 double-precison floating point registers, though
//half can always be used for single precision operations.


#include <rpc/xdr.h>
#ifndef _POWER_FP_REGSET_H_
#define _POWER_FP_REGSET_H_

#include "power_reg.h"
#include <rpc/xdr.h>
#include <inttypes.h>

struct XDR;

class power_fp_regset {
 private:
   uint32_t float_bits;
    
 public:
   power_fp_regset() {
      float_bits = 0;
   }
   
   class Empty {};
   static Empty empty;
   power_fp_regset(Empty) {
      float_bits = 0;
   }

   class Full {};
   static Full full;
   power_fp_regset(Full) {
      float_bits = 0xffffffffU;
   }

   power_fp_regset(const power_fp_regset &src) :
      float_bits(src.float_bits) {
   }
   void operator=(const power_fp_regset &src) {
      float_bits = src.float_bits;
   }
   
   power_fp_regset(XDR *xdr);
   bool send(XDR *xdr) const;

   class Raw {};
   static Raw raw;

   power_fp_regset(Raw, uint32_t ifloatbits) : float_bits(ifloatbits) {
   }

   class Random {};
   static Random random;
   
   power_fp_regset(Random) {
      // Obviously, a (seemingly crazy) routine like this is only used in
      // regression torture test programs.
      uint32_t val = (unsigned)::random();
      float_bits = val;
   }

   class SingleFloatReg {};
   static SingleFloatReg singleFloatReg;

   power_fp_regset(SingleFloatReg, unsigned rnum);
   power_fp_regset(SingleFloatReg, const power_reg &r);
   bool isempty() const {
      return float_bits == 0;
   }
   void clear() {
      float_bits = 0;
   }

  

   bool operator==(const power_fp_regset &src) const {
      return float_bits == src.float_bits;
   }
   bool operator==(const power_reg &r) const {
      return r.isFloatReg() && (1U << r.getFloatNum() == float_bits);
   }
   
   bool operator!=(const power_fp_regset &src) const {
      return float_bits != src.float_bits;
   }

   uint32_t getRawBits() const {
      return float_bits;
   }

   bool exists(const power_fp_regset &subset) const {
      return 0U == (subset.float_bits & (~float_bits));
   }
 
   bool exists(const power_reg &r) const {
      // This could be optimized better:
      const power_fp_regset theSet(singleFloatReg, r);
      return exists(theSet);
   }
   bool exists(unsigned num) const {
      assert(num < 32);
      const power_reg r(power_reg::f, num);
      return exists(r);
   }

   void operator|=(const power_fp_regset &theSet) {
      float_bits |= theSet.float_bits;
   }
   void operator&=(const power_fp_regset &theSet) {
      float_bits &= theSet.float_bits;
   }
   void operator-=(const power_fp_regset &theSet) {
      float_bits &= ~theSet.float_bits;
   }

   power_reg expand1() const; 
   unsigned count() const;
  
};

#endif
