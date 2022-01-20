// simm.C
// signed immediate bits
// Ariel Tamches

#include "simm.h"
#include <stdlib.h> // random()

template <unsigned numbits>
int simmediate<numbits>::getRandomValue() {
   const unsigned numPossibilities = (1U << numbits);
   const unsigned mask = numPossibilities - 1;
   
   unsigned num = random() & mask;

   int result = getMinAllowableValue() + num;

   assert(result >= getMinAllowableValue());
   assert(result <= getMaxAllowableValue());
   
   return result;
}

