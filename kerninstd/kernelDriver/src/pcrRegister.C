// pcrRegister.C

#include "pcrRegister.h"

ostream &operator<<(ostream &os, const pcr_fields &f) {
   os << "s0=" << (void*)f.s0 << ' ';
   os << "s1=" << (void*)f.s1 << ' ';
   os << "user=" << (bool)f.ut << ' ';
   os << "sys=" << (bool)f.st << ' ';
   os << "priv=" << (bool)f.priv << ' ';
   return os;
}
