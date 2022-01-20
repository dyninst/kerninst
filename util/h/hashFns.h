// hashFns.h
// A few common hash functions for the dictionary_hash class

#ifndef _HASH_FNS_H_
#define _HASH_FNS_H_

#include <inttypes.h> // uint32_t, etc.
#include "common/h/String.h"

// use when all bits of the address are valid (i.e. not necessarily zero)
unsigned addrHash(const uint32_t &addr);
unsigned addrHash(const uint64_t &addr);

// use when the low 2 bits are sure to be zero and thus won't contribute
// anything to a decent hash function.
unsigned addrHash4(const uint32_t &addr);
unsigned addrHash4(const uint64_t &addr);

// use when the low 4 bits are sure to be zero and thus won't contribute
// anything to a decent hash function.
unsigned addrHash16(const uint32_t &addr);
unsigned addrHash16(const uint64_t &addr);

inline unsigned unsignedIdentityHash(const unsigned &num) {
   return num;
}

//unsigned cropeHash(const crope &);
typedef const char *cstr_t ;
unsigned stringHash(const pdstring &);
unsigned cStrHash(const cstr_t&); // same algorithm as stringHash()

#endif
