// stacklist.c

#include "stacklist.h"
#include "compare_and_swap.h"
#include <stdlib.h>

#ifndef NULL
#define NULL 0
#endif

// -------------------------------------------------------------------------------
// Routines used by kperfmon & by test program, but not by kernel instrumentation:
// -------------------------------------------------------------------------------

void stacklist_initialize(struct stacklist *the_heap) {
   const unsigned offset_bytes_to_curr_head = sizeof(stacklistitem) * max_num_threads;

   struct stacklistitem *iter;
   struct stacklistitem *last; // last item (not 1 past the last item)

   // Loop thru the stacklist items, setting their "next" fields
   iter = &the_heap->theheap[0];

   // Might as well set head now.
   the_heap->head = iter;

   last = iter + (max_num_threads-1); // ptr arith
   while (iter < last) {
      struct stacklistitem *next_iter = iter + 1; /* ptr arith */
      iter->next = next_iter;

      /* initialize timer ptrs to NULL?  Nah. */

      iter = next_iter;
   }
   // Handle "last" a little differently
   iter->next = NULL; // instead of "finish"
}

void stacklist_destroy(struct stacklist *the_heap) {
   the_heap->curr_head = NULL;
}

// --------------------------------------------------------------
// Routines used by kernel & by test program, but not by kperfmon
// --------------------------------------------------------------

void **stacklist_get(struct stacklist *the_heap) {
   // returns vec of ptrs, or 0 if nothing is available

   struct stacklistitem *oldhead, *newhead;
   
   void **result;

   oldhead = *the_heap->head;
   if (oldhead == NULL)
      return NULL;

   result = oldhead->timers;

   the_heap->head = oldhead->next; // could be NULL.

   return result;
}

void stacklist_free(struct stacklist *the_heap, void *timers[]) {
   // Translate "timers" parameter into a stacklistitem (subtract 4 bytes):
   struct stacklistitem *newhead =
      (struct stacklistitem *)((char*)timers - sizeof(struct stacklistitem*));

   newhead->next = the_heap->head;
   the_heap->head = newhead;
}
