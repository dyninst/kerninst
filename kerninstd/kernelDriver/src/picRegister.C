// picRegister.C

#include "picRegister.h"

ostream &operator<<(ostream &os, const pic_fields &f) {
   os << "pic0=" << (void*)f.pic0 << ' ';
   os << "pic1=" << (void*)f.pic1 << ' ';

   return os;
}
