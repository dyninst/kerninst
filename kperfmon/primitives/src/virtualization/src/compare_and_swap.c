// compare_and_swap.c

#include "compare_and_swap.h"

int compare_and_swap32(void *address, unsigned oldval, unsigned newval) {
   // temporary:
   *(unsigned*)address = newval;
   return 1; // success
}
