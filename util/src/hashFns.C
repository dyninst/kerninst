// hashFns.C

#include "util/h/hashFns.h"

// use when all bits of the address are valid (i.e. not necessarily zero)
unsigned addrHash(const uint32_t &addr) {
   uint32_t val = addr;
   unsigned result = 0;

   while (val) {
      // strip off 2 bits at a time
      result = (result << 1) + result + (val & 0x3);
      val >>= 2;
   }

   return result;
}

unsigned addrHash(const uint64_t &addr) {
   uint64_t val = addr;
   unsigned result = 0;

   while (val) {
      // strip off 2 bits at a time
      result = (result << 1) + result + (unsigned)(val & 0x3);
      val >>= 2;
   }

   return result;
}

// use when the low 2 bits are sure to be zero and thus won't contribute anything
// to a decent hash function.
unsigned addrHash4(const uint32_t &addr) {
   uint32_t new_addr = (addr >> 2);
   return addrHash(new_addr);
}

unsigned addrHash4(const uint64_t &addr) {
   uint64_t new_addr = (addr >> 2);
   return addrHash(new_addr);
}

// use when the low 4 bits are sure to be zero and thus won't contribute anything
// to a decent hash function.
unsigned addrHash16(const uint32_t &addr) {
   uint32_t new_addr = (addr >> 4);
   return addrHash(new_addr);
}

unsigned addrHash16(const uint64_t &addr) {
   uint64_t new_addr = (addr >> 4);
   return addrHash(new_addr);
}

unsigned stringHash(const pdstring &str) {
  return (pdstring::hash(str));
}

unsigned cStrHash(const cstr_t &istr) {
   unsigned result = 0;

   const char *str = istr;
   while (*str != '\0')
      result = (result << 1) + result + *str++;
   
   return result;
}
