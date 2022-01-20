// compare_and_swap.h

#ifndef _COMPARE_AND_SWAP_H_
#define _COMPARE_AND_SWAP_H_

int compare_and_swap32(void *address,
                       unsigned oldval,
                       unsigned newval);
   // returns true iff successful

#endif
