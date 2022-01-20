// stacklist.c

#include "stacklist.h"

#ifndef NULL
#define NULL 0
#endif

void stacklist_initialize(struct stacklist *the_heap)
{
   struct stacklistitem *iter;
   struct stacklistitem *last; // last item (not 1 past the last item)
   largeptr_t dummy;

   // Loop thru the stacklist items, setting their "next" fields
   iter = &the_heap->theheap[0];

   // Might as well set head now.
   dummy = (kptr_t)iter;
   the_heap->head = dummy;

   last = iter + (max_num_threads-1); // ptr arith
   while (iter < last) {
      dummy = (kptr_t)(iter + 1); // ptr arith
      iter->next = dummy;
      ++iter;
   }
   // Handle "last" a little differently
   iter->next = (largeptr_t)NULL;
}

void stacklist_destroy(struct stacklist *the_heap)
{
   the_heap->head = (largeptr_t)NULL;
}

void* stacklist_get(struct stacklist *the_heap)
{
   // returns vec of ptrs, or 0 if nothing is available

   struct stacklistitem *oldhead;
   void *result;

   oldhead = (struct stacklistitem *)the_heap->head;
   if (oldhead == NULL)
      return NULL;

   result = (void*)oldhead->timers;

   the_heap->head = oldhead->next; // could be NULL.

   return result;
}

void stacklist_free(struct stacklist *the_heap, void *timers)
{
   // Translate "timers" parameter into a stacklistitem (subtract 4 bytes):
   struct stacklistitem *newhead =
      (struct stacklistitem *)((char*)timers - stacklistitem_timers_offset);

   newhead->next = the_heap->head;
#ifdef i386_unknown_linux2_4   
   the_heap->head = (largeptr_t)((uint32_t)newhead);
#else
   the_heap->head = (largeptr_t)(newhead);
#endif
}
