// popc.h
// Count the number of 1 bits in an ***unsigned*** type of integer
// (unsigned, unsigned long, unsigned long long)

#ifndef _POP_C_H_
#define _POP_C_H_

#include <inttypes.h> // uint32_t, etc.

template <class T>
inline unsigned do_popc(T num) {
   // This is one of the coolest algorithms.  So cool!  I didn't create it.
   // Heck, I don't even know why it works (though I have a pretty good idea).
   // But it's so fast!  Time is proportional to the number of 1 bits actually in
   // "num", not proportional to the number of bits in an unsigned.  So cool!

   // How it works (I think):  Consider num compared to num-1.
   // The latter is the same as the former, with the following changes: the rightmost
   // 1 bit in num is now 0, and all bits to the right of *that* are now 1.
   // Consider then num & (num-1): The 1 bit is now 0, so in the and-ed version it'll
   // be 0.  The bits to the right of that were 0 in the original num, so despite the
   // fact that they're all 1's in (num-1), the and-ed version will have all zeros
   // for them.  So num & num-1 is the same as num except the rightmost 1 has been
   // changed to a zero.  Now you can see why that's perfect for this algorithm;
   // num &= num-1 until num==0, return the number of steps it took because that's
   // the number of 1 bits there were. --ari

   register unsigned result = 0;
   for (; num; num &= num-1) // so cool!
      result++;
   return result;
}

inline unsigned ari_popc(uint8_t num) {
   return do_popc(num);
}

inline unsigned ari_popc(uint16_t num) {
   return do_popc(num);
}

inline unsigned ari_popc(uint32_t num) {
   return do_popc(num);
}

inline unsigned ari_popc(uint64_t num) {
   return do_popc(num);
}

// --------------------------------------------------------------

inline int ari_popc(int8_t inum) {
   uint8_t num = inum;
   return ari_popc(num);
}

inline int ari_popc(int16_t inum) {
   uint16_t num = inum;
   return ari_popc(num);
}

inline int ari_popc(int32_t inum) {
   uint32_t num = inum;
   return ari_popc(num);
}

inline int ari_popc(int64_t inum) {
   uint64_t num = inum;
   return ari_popc(num);
}

#endif
