// simm.h
// Ariel Tamches
// template class to manage a signed immediate integer of
// less than 32 bits...useful for sparc instruction set, which
// has lots of 13, 22, and 30 bit numbers.  Provides bitwise
// operations between the signed immediate and unsigned,
// which is useful when building a sparc instruction.

#ifndef _SIMM_H_
#define _SIMM_H_

#include <assert.h>
#include "bitUtils.h" // replaceBits()

template <unsigned numbits> class simmediate;

// this templated function has to be declared before being specified 
// as a friend in the class definition of simmediate below, but we
// require the above forward declaration of simmediate to do so
template <unsigned numbits>
void operator|=(unsigned &result, simmediate<numbits> imm) {
   // bitwise-or.

   // Naive (and buggy) implementation is:
   //    result |= imm.value
   // The problem here is that imm.value gets signed-extended all the way to 32 bits
   // before the bit-or operation, which is NOT what we want.
   //
   // Fortunately there's a simple solution.  First of all let's recognize that the
   // problem only happens if imm is negative (if positive, the sign-extension produces
   // 0's in the high bits).  If imm is negative then we just need to strip the high
   // (32-<numbits>) bits off of the buggy result obtained by the above method.
   // In fact this works for positive imm values too, though it's unnecessary since 0's
   // already exist in those bits.

   // mask: contains all 1's in the lower <numbits> bits, and all 0's in the
   //       upper bits.
   unsigned mask = (1U << numbits) - 1; // yes, even works if numbits==32 (yields -1)
      // easily optimized since numbits is a template parameter
   const unsigned properly_signextended_imm = (unsigned)imm.value & mask;

#ifndef NDEBUG
   const unsigned orig_result = result;
#endif

   result |= properly_signextended_imm;

   // assert check: since properly_signextended_imm uses only the low <numbits> bits,
   // the high 32-<numbits> bits should not have changed.
   mask = ~mask; // now, it's all 0's in the low <numbits> bits, all 1's above.

#ifndef NDEBUG
   assert((orig_result & mask) == (result & mask));
#endif
}

template <unsigned numbits>
class simmediate {
 private:
   // We used to have a bit-field (signed int value : numbits), but that caused
   // purify UMRs and UMCs, with no gain in performance (in fact a decrease) and no
   // savings in storage space.
//   signed int value : numbits;
   int value;

   void assign(long newvalue);

#if 0 //if 32-(or 64-)bit v is positive and > 2^(n-1), it still might  
      //be < 2^n, so requiring v to be <= max _positive_ value in
      //n bits is too strong a requirement. 
   static bool inrange(long v) {
      return ( v >= getMinAllowableValue() && v <= getMaxAllowableValue());
   }
#endif

   static bool inrange(long v ) {
      if (v > 0)
         return ((unsigned int)v <= getMaxUnsignedAllowableValue());
      else
          return (v >= getMinAllowableValue() );
   } 
 public:
   static inline unsigned int getMaxUnsignedAllowableValue() { 
      //largest unsigned value is 2^n - 1
      return (1L << (numbits)) - 1;
   }
   static inline int getMaxAllowableValue() {
      // largest allowed value is 2^(n-1) - 1
      return (1L << (numbits-1)) - 1;
   }
   static inline int getMinAllowableValue() {
      // smallest allowed value is -2^(n-1)
      return -(1L << (numbits-1));
   }
   static int getRandomValue();
   
   simmediate(const simmediate &src) : value(src.value) {}
   
   simmediate(long ivalue) : value(ivalue) {
      assert(inrange(ivalue));
   }

   simmediate& operator=(long newvalue) {
      assert(inrange(newvalue));
      value = newvalue;
      return *this;
   }
   simmediate& operator=(const simmediate &src) {
      value = src.value; // no need to check for x=x in this case, I think
      return *this;
   }

   //this conversion operator is very useful for power_instr -Igor
   operator signed() const { return value; }
	   
   // Note that we want to support bitwise or with a 32-bit raw (unsigned) value,
   // but only when we are the right side.  Therefore, we have no operator| that
   // takes in an unsigned, but we do have the equivalent with the argument
   // order reversed:
   friend uint32_t operator|(uint32_t orig, simmediate<numbits> imm) {
      orig |= imm; // see below
      return orig;
   }

   // ...similarly, we want to be able to apply |=(simmediate<numbits> &) to an
   // existing 32-bit raw (unsigned) value:
   friend void operator|= <>(unsigned &result, simmediate<numbits>);

   friend uint32_t replaceBits(uint32_t orig,
                               unsigned loBit,
                               simmediate<numbits> imm) {
      const uint32_t mask = (1U << numbits)-1;
      const uint32_t bitsToUseAsReplacement = imm.value & mask;
      
      return ::replaceBits(orig, loBit, loBit+numbits-1, bitsToUseAsReplacement);
   }
   

   // A very useful operation to apply to a 32 bit raw value:
   // shift the raw value left by <numbits> bits, and then bitwise-or
   // an simmediate<numbits>.  What to name this?
   friend void rollin(unsigned &result, simmediate<numbits> imm) {
      result <<= numbits;
      result |= imm; // operator|=(unsigned &, imm);
   }
};

#endif
