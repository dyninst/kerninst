// switch_in.c

#include "switch_in.h"
#include "hash_table.h"
#include "stacklist.h"
#include "vtimer.h"
#include <assert.h>

#ifndef NULL
#define NULL 0
#endif

void on_switch_in(void) {
   extern struct hash_table theHashTable;
   extern struct stacklistitem **vtimerptrvecs_freelist;

   struct vtimer *theTimer;
   
   // From the hash table, get the stored vector-of-timer-ptrs for this kthread.
   // If none exists, then we don't need to do anything more

   struct vtimer **timers_start = hash_table_remove(&theHashTable, %g7);

   if (timers_start == NULL)
      /* probably the common case */
      return;

   struct vtimer **timers_iter = timers_start

   while ((theTimer = *timers_iter++) != NULL) {
      struct vtimer_total total_field = theTimer->total_field;
      
      /* assert timer state was stopped */
      assert(total_field.state == 0x0);
      
      /* call the restart routine */
      theTimer->restart_routine(theTimer, total_field);
   }

   /* return this vector of pointers to the free list */
   /* But first, just for fun, empty out the vector */
   *timers_start = NULL;

   stacklist_free(vtimerptrvecs_freelist, (void**)timers_start);
}
