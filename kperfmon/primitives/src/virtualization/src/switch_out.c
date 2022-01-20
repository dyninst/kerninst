// switch_out.c

#include <sys/thread.h> // for threadp() [simply returns %g7]
#include "switch_out.h"
#include "all_vtimers.h" /* for struct all_vtimers and max_num_vtimers */
#include "vtimer.h" /* for struct vtimer */
#include "stacklist.h"
#include "hash_table.h"
#include <assert.h>

#ifndef NULL
#define NULL 0
#endif

void on_switch_out(void) {
   extern struct stacklistitem **vtimerptrvecs_freelist;
      // unresolved at compile time; gets resolved at run time
   extern struct all_vtimers allVTimers;

   int grabbed_freelistentry_yet = 0;

   void **iter = allVTimers.vtimer_addresses;
   void **finish = allVTimers.finish;

   void **vec_of_timers = NULL; /* for now...until it gets allocated in the loop */
   void **vec_of_timers_iter = NULL; /* for now */
   void **vec_of_timers_finish = NULL; /* for now */

   // Loop through all vtimers
   while (iter != finish) {
      struct vtimer *theVTimer = (struct vtimer *)*iter;

      // read total field to get the state
      switch (theVTimer->total_field.state) {
         case 0x0: // already stopped; do nothing
            break;
         case 0x1: // trying to start.  Change it stopped
            theVTimer->total_field.state = 0x0; // make sure atomic write
            break;
         case 0x2: // started
            theVTimer->stop_routine(theVTimer, theVTimer->total_field);
               // make sure both arguments are in a register.
               // beware that the compiler will place the 64 bit total field
               // into two registers.  Oy.

            if (!grabbed_freelistentry_yet) {
               vec_of_timers = stacklist_get(vtimerptrvecs_freelist);
               vec_of_timers_iter = vec_of_timers;
               vec_of_timers_finish = vec_of_timers_iter + max_num_vtimers;
               grabbed_freelistentry_yet = 1;
            }
            // Add to working list:
            assert(vec_of_timers_iter < vec_of_timers_finish);
            *vec_of_timers_iter++ = (void*)theVTimer;
            break;
         case 0x3:
            assert(0);
            break;
         default:
            assert(0);
            break;
      }
   }
   if (grabbed_freelistentry_yet) { // was anything done?
      extern struct hash_table theHashTable;
      // What to set to key to?  Address of the kthread_t structure (indeed, there
      // is no id field; this is what the kernel itself uses for a thread id)
      unsigned theKey = (unsigned)threadp();

      assert(vec_of_timers_iter < vec_of_timers_finish);
      *vec_of_timers_iter++ = NULL; // mark the end of the list

      // Add to the hash table!
      if (!hash_table_add_new(&theHashTable, vec_of_timers, // not vec_of_timers_iter!
                              theKey))
         assert(0);
   }
}
