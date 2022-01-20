// all_vtimers.c
// see .h file for comments

#include "all_vtimers.h"
#include <assert.h>

#ifndef NULL
#define NULL 0
#endif

// ------------------------------------------------------------------------------------
// Code executed by kperfmon and by the test program, but not by kernel instrumentation
// ------------------------------------------------------------------------------------

void initialize_all_vtimers(struct all_vtimers *the_vtimers) {
   /* memory space for the_vtimers has already been allocated.  No sub-allocations
      are needed. */
   void **start = the_vtimers->vtimer_addresses;
   void **iter = start;
   void **finish = start + max_num_vtimers;
   assert(start == &the_vtimers->vtimer_addresses[0]);
   the_vtimers->finish = start;

   while (iter != finish)
      *iter++ = NULL;

   assert(*the_vtimers->finish == NULL);
}

void destroy_all_vtimers(struct all_vtimers *the_vtimers) {
   /* nothing to do.  Just for the heck of it, we'll empty out the vector */
   the_vtimers->finish = the_vtimers->vtimer_addresses;
   the_vtimers->vtimer_addresses[0] = NULL;
}

int addto_all_vtimers(struct all_vtimers *the_vtimers, void *vtimer_addr) {
   /* returns 1 iff successful; 0 iff not (overflow) */
   void **old_finish = the_vtimers->finish;
   assert(*old_finish == NULL);

   assert(old_finish >= (void**)the_vtimers);
      // sanity check [underflow]
   assert(old_finish < finish);
      // sanity check [overflow]

   void **new_finish = old_finish + 1; // ptr arithmetic [4 bytes]

   if (new_finish == &the_vtimers->finish)
      // overflow; return failure
      return 0;

   *new_finish = NULL;

   *old_finish = vtimer_addr;

   the_vtimers->finish = new_finish;

   return 1; // success
}

int removefrom_all_vtimers(struct all_vtimers *the_vtimers, void *vtimer_addr) {
   /* Search to find this item */
   void **iter = the_vtimers->vtimer_addresses;
   void **finish = the_vtimers->finish; /* 1 past last */
   void **last = finish - 1; /* the last non-NULL entry */
   
   while (iter != finish) {
      if (*iter == vtimer_addr) {
         /* found! */
         *iter = *last;
         /* NOTE: a duplicate entry can temporarily be seen */
         *last = NULL;

         --the_vtimers->finish;
         return 1; // success
      }
      else
         ++iter;
   }
   return 0; // failure
}
