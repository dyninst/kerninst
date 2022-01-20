// pcrRegister.h

#ifndef _PCR_REGISTER_H_
#define _PCR_REGISTER_H_

#include <inttypes.h> // uint64_t
#include <iostream.h>

struct pcr_fields {
   unsigned long long unused1 : 47;
   unsigned s1 : 6;
   unsigned unused2 : 1;
   unsigned s0 : 6;
   unsigned unused3 : 1;
   unsigned ut : 1;
   unsigned st : 1;
   unsigned priv : 1;
};

union pcr_union { // you may assert that this is exactly 8 bytes
    uint64_t rawdata;
    struct pcr_fields f;
    
    pcr_union(const pcr_union &src) : rawdata(src.rawdata) {}
    pcr_union(uint64_t iraw) : rawdata (iraw) {}
};

ostream &operator<<(ostream &os, const pcr_fields &f);

#endif
