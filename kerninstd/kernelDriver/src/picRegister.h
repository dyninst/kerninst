// picRegister.h

#ifndef _PIC_REGISTER_H_
#define _PIC_REGISTER_H_

#include <iostream.h>

struct pic_fields {
   // note: the following ordering is intentional; pic1 is in bits 63..32, pic0 in 31..0
   unsigned pic1; // 32 bits
   unsigned pic0; // 32 bits
};

union pic_union { // you may assert that this is exactly 8 bytes
   unsigned long long raw;
   struct pic_fields f;
};

ostream &operator<<(ostream &os, const pic_fields &f);

#endif
