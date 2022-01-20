// all_vtimers.h
// In-kernel code to traverse a vector of all vtimer addresses.
// The vector is not sorted in any way.
// No particular information about a vtimer is kept in this vector besides
// the vtimer's kernel address.  Meta-data for a vtimer is stored in the timer
// itself. 
// The vector itself is created and managed by kperfmon
// More specifically, kperfmon allocates some (mmapped-for-writing) kernel memory
// in order to create it.  Since it has write permission, it can also
// initialize it.  Furthermore, when a vtimer is added, it (kperfmon) can add
// to the vector (being careful for race conditions, naturally).
// Furthermore, when a vtimer is deleted, it can delete from the vector.

#ifndef _ALL_VTIMERS_H_
#define _ALL_VTIMERS_H_

#include "util/h/kdrvtypes.h"

#define max_num_vtimers 50

#define sizeof_allvtimers ((max_num_vtimers * sizeof(kptr_t)) + sizeof(kptr_t))

#define all_vtimers_offset_to_finish (max_num_vtimers * sizeof(kptr_t))

struct all_vtimers {
   kptr_t vtimer_addresses[max_num_vtimers];
   // ends with a sentinel entry of NULL (so in reality only
   // max_num_vtimers-1 timer addresses can really be stored)

   kptr_t finish; // points to a NULL entry in vtimer_addresses
      // only exists to make addition quick.
      // With that in mind, and since only kperfmon does addition,
      // is "finish" a kerninstd address or a kernel address?
};

// We (the kernel) don't need an "initialize" method (kperfmon handles that)
// We don't need an "add to vector" method (kperfmon handles that)
// We don't need a "delete from vector" method (kperfmon handles that)
// All that we need is the ability to loop through the entries of the vector.
// Note that due to a race condition, it's possible for us to see a duplicate entry
// (if kperfmon is deleting an item from the vector while we're traversing it).
// This doesn't really pose a big problem for us.
// To understand the race condtion, let's outline what happens on insertion
// and deletion.  The good news: no locking; the bad news: possible duplicate entry.
//
// Assume we start off with entries A,B,C,NULL [sentinel].
// When adding D, kperfmon does the following:
// 
// A,B,C,NULL --> A,B,C,NULL,NULL --> A,B,C,D,NULL
// This looks good -- no race conditions
//
// Now let's say that kperfmon removes B.  Here's what it does; you can see the
// race condition
// A,B,C,D,NULL --> A,D,C,D,NULL (temporary duplicate entry D can be seen)
// --> A,D,C,NULL,NULL
// So when traversing, you (the kernel traversal code) need to be aware of the
// possibility for a duplicate entry, and handle things accordingly.  For now,
// it's not really a problem, since it'll result in trying to stop a timer twice,
// so the second time is guaranteed to be a nop.

// ------------------------------------------------------------------------------------
// Code executed by kperfmon and by the test program, but not by kernel instrumentation
// ------------------------------------------------------------------------------------

void initialize_all_vtimers(struct all_vtimers *);
void destroy_all_vtimers(struct all_vtimers *);
void addto_all_vtimers(struct all_vtimers *, void *vtimer_addr);
void removefrom_all_vtimers(struct all_vtimers *, void *vtimer_addr);

#endif
