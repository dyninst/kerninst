// StaticString.C

#include "util/h/StaticString.h"

ostream &operator<<(ostream &os, const StaticString &src) {
   return os << src.c_str();
}

debug_ostream &operator<<(debug_ostream &os, const StaticString &src) {
   return os << src.c_str();
}

