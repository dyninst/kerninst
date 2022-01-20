// bitUtils.C

#include <iostream.h> // TEMP FOR DEBUG
#include <assert.h>
#include "bitUtils.h"

unsigned getBits(uint32_t value, unsigned lobit, unsigned hibit) {
   assert(hibit >= lobit);
   const unsigned numbits = hibit - lobit + 1;

   const uint32_t mask = (1U << numbits) - 1;
   return (value >> lobit) & mask;
}

unsigned getBitsFrom0(uint32_t value, unsigned hibit) {
   // Like above routine, but assumes that lobit is 0
   const unsigned numbits = hibit + 1;

   const uint32_t mask = (1U << numbits) - 1;
   return value & mask;
}

int signExtend(uint32_t bits, unsigned numbits) {
   // it's negative iff the highest bit is 1.
   const bool negative = (bits & (1U << (numbits-1))) != 0;
   if (!negative)
      return bits;

   // time to sign-extend
   const uint32_t mask = (1U << numbits) - 1; // all 1's in the used bits; all 0's above
   bits |= ~mask;

   return (int)bits;
}

extern "C" int ffs(int);

unsigned get_ffs(uint32_t bits) {
   assert(bits != 0);

   return ffs(bits)-1;
}

uint32_t replaceBits(uint32_t value, unsigned loBitNum, unsigned hiBitNum,
                     uint32_t replacementValue) {
   // First save away bits 0 thru loBitNum-1, since we're going to clear them:
   const uint32_t savedLowBits = loBitNum == 0 ? 0 : getBitsFrom0(value, loBitNum-1);

   // Shift right by [loBitNum] bits (losing low bits in the process)
   value >>= loBitNum;

   // Clear [numbits] low bits
   const unsigned numBits = hiBitNum - loBitNum + 1;
   value >>= numBits;
   value <<= numBits;

   // Insert the replacement bits
   value |= replacementValue;
   
   // Shift the replacement bits back into the right spot
   value <<= loBitNum;
   
   // Now finally put back the savedLowBits
   value |= savedLowBits;

   return value;
}
