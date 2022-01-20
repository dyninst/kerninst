// uimm.C
// unsigned immediate bits
// Ariel Tamches

#include "uimm.h"
#include <stdlib.h> // random()

template <unsigned numbits>
unsigned uimmediate<numbits>::getRandomValue() {
   unsigned mask = ((1U << numbits) - 1);
   
   unsigned num = random();
   num &= mask;
   
   assert(num >= getMinAllowableValue() && num <= getMaxAllowableValue());

   return num;
}

