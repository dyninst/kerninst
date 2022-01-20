// ffs.h
// Finds the first bit set in an ***unsigned*** type of integer
// (uint8_t, uint16_t, uint32_t, uint64_t).
// Counts from the right (least significant bit).  The LSB is considered
// bit 0.  In other words, ari_ffs(1U) returns 0, ari_ffs(2U) returns 1.
// NOTE: ari_ffs(0U) is undefined!
//
// Compare to the somewhat-standard "ffs()" for performance, and
// use ffs() instead if it's faster than what we give you.
// But I bet it's slower.

#ifndef _FFS_H_
#define _FFS_H_

#include "util/h/popc.h"

inline unsigned ari_ffs(uint8_t num) {
   return ari_popc(num ^ (~ (-num))) - 1;
}

inline unsigned ari_ffs(uint16_t num) {
   return ari_popc(num ^ (~ (-num))) - 1;
}
   
inline unsigned ari_ffs(uint32_t num) {
   // WARNING: undefined if num==0!
   // xnor num with its 2's complement
   // then popc that result.
   // We're pretty much done then.  But be advised that
   // the result we have now considers the bits to be
   // numbered from 1, not 0.  For example, it thinks a 32-bit
   // number has bits 1 thru 32.  I prefer calling them 0 thru 31,
   // and so does ffs(), the library call in <strings.h>, so we
   // subtract 1.
   return ari_popc(num ^ (~ (-num))) - 1;
}

inline unsigned ari_ffs(uint64_t num) {
   // WARNING: undefined if num==0!
   // xnor num with its 2's complement
   // then popc that result.
   // We're pretty much done then.  But be advised that
   // the result we have now considers the bits to be
   // numbered from 1, not 0.  For example, it thinks a 32-bit
   // number has bits 1 thru 32.  I prefer calling them 0 thru 31,
   // and so does ffs(), the library call in <strings.h>, so we
   // subtract 1.
   return ari_popc(num ^ (~ (-num))) - 1;
}

#endif
