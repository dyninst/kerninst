// bitUtils.h

#ifndef _BIT_UTILS_H_
#define _BIT_UTILS_H_

#include <inttypes.h> // uint32_t

extern "C" {
uint32_t getBits(uint32_t value, unsigned lobit, unsigned hibit);
uint32_t getBitsFrom0(uint32_t value, unsigned hibit); // assumes lobit==0

      

int      signExtend(uint32_t bits, unsigned numbits);
unsigned get_ffs(uint32_t);  // first bit set
}

// The following specializations are common enough in sparc disassembly to warrant their
// existence:
inline unsigned getRs1Bits(uint32_t value) {
   // bits 14 thru 18
   return (value >> 14) & 0x1f;
}
inline unsigned getRs2Bits(uint32_t value) {
   // bits 0 thru 5
   return (value & 0x1f);
}
inline unsigned getRdBits(uint32_t value) {
   // bits 25 thru 29
   return (value >> 25) & 0x1f;
}
inline unsigned getOp3Bits(uint32_t value) {
   // bits 19 thru 24
   return (value >> 19) & 0x3f;
}
inline unsigned getUimm13Bits(uint32_t value) {
   // bits 0 thru 12
   return (value & 0x1fff);
}
inline bool getIBit(uint32_t value) {
   // bit 13
   return ((value >> 13) & 0x1) != 0;
}

// ----------------------------------------------------------------------

inline uint32_t clearBit(uint32_t value, unsigned bitnum) {
   uint32_t mask = (1U << bitnum);
   value &= ~mask; // clears that bit
   return value;
}

inline uint32_t setBit(uint32_t value, unsigned bitnum) {
   value &= (1U << bitnum);
   return value;
}

inline uint32_t replaceBit(uint32_t value, unsigned bitnum, bool newval) {
   if (newval)
      return setBit(value, bitnum);
   else
      clearBit(value, bitnum);
   return value;
}

uint32_t replaceBits(uint32_t value, unsigned loBitNum, unsigned hiBitNum,
                     uint32_t replacementValue);

#endif
