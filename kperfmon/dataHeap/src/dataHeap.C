// dataHeap.C
// See top of .h file for comments

#include "dataHeap.h"
#include "vtimer.h"

dataHeap::dataHeap(unsigned num_cpus,
		   unsigned inum_uint8_elems,
                   unsigned inum_timer16_elems,
                   unsigned inum_vtimer_elems) :
    uint8heap(inum_uint8_elems, num_cpus),
    timer16heap(inum_timer16_elems, num_cpus),
    vtimerheap(inum_vtimer_elems, num_cpus)
{
   assert(sizeof(timer16) == 16);
}

dataHeap::~dataHeap() {
   // dtors for the sub-heaps do the real work (& assertion checks)
}
