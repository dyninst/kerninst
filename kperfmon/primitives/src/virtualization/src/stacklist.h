// stacklist.h
// A stacklist is a way of allocating memory.  In our case, we're managing a
// pre-allocated heap of vector-of-timers.
//
// Division of responsibility:
// kperfmon: creates, initializes, and destroys this heap.  This includes
//           preallocating all of the needed vector-of-timers.
// kernel: calls stacklist_get() and stacklist_free() to allocate & unallocate
//         a vector-of-timers, respectively.

#ifndef _STACK_LIST_H_
#define _STACK_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "util/h/kdrvtypes.h"
#include "all_vtimers.h" // just to get max_num_vtimers

#define max_num_threads 200

// stacklist pointers are 64-bit even on the 32-bit platform
// this ensures proper alignment of the 64-bit fields (like auxdata)
// Only first 32 bits of this address are actually accessed
typedef union {
    uint64_t space;
    kptr_t   ptr;
} largeptr_t; 

struct saved_timer {
    largeptr_t addr;  // Even on a 32-bit system we reserve 64 bits for addr
    uint64_t auxdata; // to ensure proper alignment of 64-bit auxdata
};

#define saved_timer_address_offset (0)
#define saved_timer_auxdata_offset (sizeof(largeptr_t))

#define sizeof_saved_timer (sizeof(struct saved_timer))
/* excludes "next" field */
#define stacklistitem_nbytes_dataonly (sizeof_saved_timer * max_num_vtimers) 
#define stacklistitem_nbytes (sizeof(largeptr_t)+stacklistitem_nbytes_dataonly)
#define stacklistitem_timers_offset (sizeof(largeptr_t))
struct stacklistitem {
   largeptr_t next;
   
   // Now the data for this item
   // For us, it's a vector of timers.  A timer is specified by its address.
   // The vector is of fixed size.  The last item stored will be a NULL sentinel,
   // so you can only *really* store max_num_vtimers-1 useful vtimer addresses.
   struct saved_timer timers[max_num_vtimers];
};

#define stacklist_nbytes ((max_num_threads * stacklistitem_nbytes) + \
                          sizeof(largeptr_t))
#define stacklist_offset_to_head (max_num_threads * stacklistitem_nbytes)
struct stacklist {
   // The stacklist is just a vector of stacklistitems, so allocation of
   // the entire stacklist is one big contiguous block, of size stacklist_nbytes
   stacklistitem theheap[max_num_threads];

   // ...and one more thing: a pointer to the head item
   largeptr_t head; // next free item.  If NULL then heap is full.
};
   
// -------------------------------------------------------------------------------
// Routines used by kperfmon & by test program, but not by kernel instrumentation:
// -------------------------------------------------------------------------------
void stacklist_initialize(struct stacklist *);
   // Memory must have already been allocated in kernel space before
   // calling this routine.
   
void stacklist_destroy(struct stacklist *);
   // Clears, but does not deallocate memory for, the stacklist.

// --------------------------------------------------------------
// Routines used by kernel & by test program, but not by kperfmon
// --------------------------------------------------------------
void **stacklist_get(struct stacklist *);
   // returns vector of ptrs, or 0 if nothing available
void stacklist_free(struct stack *, void **timers);

#ifdef __cplusplus
}
#endif

#endif
