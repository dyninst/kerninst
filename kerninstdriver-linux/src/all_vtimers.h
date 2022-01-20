// all_vtimers.h

// In-kernel code to manage a vector of all vtimer addresses.
// The vector is not sorted in any way.

// No particular information about a vtimer is kept in this vector besides
// the vtimer's kernel address.  Meta-data for a vtimer is stored in the timer
// itself. 

// The vector itself is created and managed by the driver. More specifically, 
// the driver allocates some kernel memory in order to create it. Since the
// driver has write permission, it can also initialize it. When a vtimer is 
// added/deleted, the driver can add/delete to/from the vector (being careful 
// to avoid race conditions).

#ifndef _ALL_VTIMERS_H_
#define _ALL_VTIMERS_H_

#include "kernInstIoctls.h" // kptr_t

#define max_num_vtimers 50

struct all_vtimers {
   kptr_t vtimer_addresses[max_num_vtimers];
      // ends with a sentinel entry of NULL (so in reality only
      // max_num_vtimers-1 timer addresses can be stored)

   kptr_t finish; 
      // points to a NULL entry in vtimer_addresses
      // only exists to make addition quick.
};

// Note that due to a race condition, it's possible for us to see a duplicate 
// entry (if an item is being deleted from the vector while we're traversing 
// it). This doesn't really pose a big problem for us.

// To understand the race condtion, let's outline what happens on insertion
// and deletion. The good news: no locking; the bad news: possible duplicate 
// entry.
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
//
// So when traversing, the traversal code needs to be aware of the possiblity
// for a duplicate entry, and handle things accordingly.  For now, it's not
// really a problem, since it'll result in trying to stop a timer twice, so 
// the second time is guaranteed to be a nop.

// -----------------------------------------------------------------
// Code executed by the driver, but not by kernel instrumentation
// -----------------------------------------------------------------

void initialize_all_vtimers(struct all_vtimers *);
void destroy_all_vtimers(struct all_vtimers *);
int addto_all_vtimers(struct all_vtimers *, void *vtimer_addr);
int removefrom_all_vtimers(struct all_vtimers *, void *vtimer_addr);

#endif
