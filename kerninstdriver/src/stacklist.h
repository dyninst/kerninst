// stacklist.h

// A stacklist is a way of allocating memory.  In our case, we're managing a
// pre-allocated heap of vector-of-timers.
//
// Division of responsibility:
// driver: creates, initializes, and destroys this heap.  This includes
//         preallocating all of the needed vector-of-timers.
// kernel: calls stacklist_get() and stacklist_free() to allocate & unallocate
//         a vector-of-timers, respectively.

#ifndef _STACK_LIST_H_
#define _STACK_LIST_H_

#include <sys/types.h> // uint64_t
#include "all_vtimers.h" // max_num_vtimers

#define max_num_threads 200

// stacklist pointers are 64-bit even on the 32-bit platform
// this ensures proper alignment of the 64-bit fields (like auxdata)
// Only first 32 bits of this address are actually accessed
typedef uint64_t largeptr_t; 

struct saved_timer {
    largeptr_t addr;  // Even on a 32-bit system we reserve 64 bits for addr
    uint64_t auxdata; // to ensure proper alignment of 64-bit auxdata
};

#define saved_timer_address_offset (0)
#define saved_timer_auxdata_offset (sizeof(largeptr_t))
#define sizeof_saved_timer (sizeof(struct saved_timer))

struct stacklistitem {
   largeptr_t next;
   
   // Now the data for this item, a vector of timers. A timer is specified 
   // by its address. The vector is of fixed size. The last item stored will 
   // be a NULL sentinel, so you can only *really* store max_num_vtimers-1 
   // useful vtimer addresses.
   struct saved_timer timers[max_num_vtimers];
};

#define stacklistitem_nbytes_dataonly (sizeof_saved_timer * max_num_vtimers) 
   // excludes "next" field
#define stacklistitem_nbytes (sizeof(struct stacklistitem))
#define stacklistitem_timers_offset (sizeof(largeptr_t))

struct stacklist {
   // The stacklist is just a vector of stacklistitems, so allocation of
   // the entire stacklist is one big contiguous block of size stacklist_nbytes
   struct stacklistitem theheap[max_num_threads];

   // ...and one more thing: a pointer to the head item
   largeptr_t head; // next free item.  If NULL then heap is full.
};

#define stacklist_nbytes (sizeof(struct stacklist))
#define stacklist_offset_to_head (max_num_threads * stacklistitem_nbytes)
   
// -------------------------------------------------------------
// Routines used by driver, but not by kernel instrumentation
// -------------------------------------------------------------

void stacklist_initialize(struct stacklist *);
   // Memory must have already been allocated in kernel space before
   // calling this routine.
   
void stacklist_destroy(struct stacklist *);
   // Clears, but does not deallocate memory for, the stacklist.

// -------------------------------------------------------------
// Routines used by kernel instrumentation, but not by driver
// -------------------------------------------------------------

void* stacklist_get(struct stacklist *);
   // returns vector of ptrs, or 0 if nothing available

void stacklist_free(struct stacklist *, void *);

#endif
