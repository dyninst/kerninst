// uimm.h
// Ariel Tamches
// template class to manage an unsigned immediate integer of
// less than 32 bits...useful for sparc instruction set, which
// has lots of 13, 22, and 30 bit numbers.  Provides bitwise
// operations between the unsigned immediate and unsigned,
// which is useful when building a sparc instruction.

#ifndef _UIMM_H_
#define _UIMM_H_

#include <assert.h>
#include "bitUtils.h" // replaceBits()

template <unsigned numbits> class uimmediate;

// this templated function has to be declared before being specified 
// as a friend in the class definition of uimmediate below, but we
// require the above forward declaration of uimmediate to do so
template <unsigned numbits>
inline void operator|=(unsigned &result, uimmediate<numbits> imm) {
   // bitwise-or.

   // Unlike the simm class, we don't have any problems with negative bit values
   // being sign-extended all the way to 32 bits.  So this fn is easy.

   // 'imm' must be inrange(), but 'result' does not need to be (on purpose).
   // No need to verify that 'imm' is in range, ctor and operator=() etc. guarantee
   // that for us.  So there's not much assertion checking that can be done; just
   // do the |=

   const unsigned orwithme = imm.value;
   result |= orwithme;
   
//    const unsigned long orwithme = imm.value;
//    // the low <numbits> bits may be set, but the high (32-<numbits>) bits are 0.
//    // let's assert that:

//    unsigned long mask = ~((1UL << numbits) - 1);
//       // all 0's in low <numbits> bits; 1's elsewhere
//    assert(0 == (orwithme & mask));

//    const unsigned long orig_result = result;
//    result |= orwithme;

//    // another assert: since orwithme should only have used the low <numbits> bits,
//    // result should not have changed in the upper (32-<numbits>) bits:
//    assert((result & mask) == (orig_result & mask));

//    return result;
}

template <unsigned numbits>
class uimmediate {
 private:
   // We used to make this a bit-field (unsigned value : numbits), but it lead
   // to purify UMRs/UMCs, and (I believe) slower code.  And, as expected, there
   // was no space savings.
   unsigned value;
//   unsigned value : numbits;

   static bool inrange(unsigned v) {
      const unsigned maxallowablevalue = getMaxAllowableValue();
      return (v <= maxallowablevalue);
   }
   
   
 public:
   static inline unsigned getMaxAllowableValue() {
      return (1U << numbits)-1; // can be nicely optimized
   }
   static inline unsigned getMinAllowableValue() {
      return 0;
   }
   static unsigned getRandomValue();

   uimmediate(const uimmediate &src) : value(src.value) {}
   uimmediate(unsigned ivalue) : value(ivalue) {
      // we've done the assignment; now let's check to make sure it was w/in range
      assert(inrange(ivalue));
   }

   uimmediate& operator=(unsigned newvalue) {
      assert(inrange(newvalue));
      value = newvalue;
      return *this;
   }
   uimmediate& operator=(const uimmediate &src) {
      value = src.value; // no need to protect against x=x here, I think
      return *this;
   }
   //this conversion operator is very useful for power_instr -Igor
   operator unsigned() const { return value; }
	   

   // Note that we want to support bitwise or with a 32-bit raw (unsigned) value,
   // but only when we are the right side.  Therefore, we have no operator| that
   // takes in an unsigned, but we do have the equivalent with the argument
   // order reversed:
   friend unsigned operator|(unsigned orig, uimmediate<numbits> imm) {
      orig |= imm; // operator|=(unsigned &, imm)
      return orig;
   }

   // ...similarly, we want to be able to apply |=(uimmediate<numbits> &) to an
   // existing 32-bit raw (unsigned) value:
   friend void operator|= <>(unsigned &result, uimmediate<numbits>);

   friend uint32_t replaceBits(uint32_t orig,
                               unsigned loBit,
                               uimmediate<numbits> uimm) {
      const unsigned bitsToUseAsReplacement = uimm.value;
      return ::replaceBits(orig, loBit, loBit+numbits-1, bitsToUseAsReplacement);
   }

   // A very useful operation to apply to a 32 bit raw value:
   // shift the raw value left by <numbits> bits, and then bitwise-or
   // an uimmediate<numbits>.
   friend void rollin(unsigned &result, uimmediate<numbits> imm) {
      result <<= numbits;
         // since numbits is template constant, this can be nicely optimized
      result |= imm; // operator|=(unsigned &, imm), above.
   }
};

#endif
