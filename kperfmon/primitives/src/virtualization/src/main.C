#include "hash_table.h"
#include "stacklist.h"
#include "all_vtimers.h"
#include <assert.h>

const unsigned max_num_elems = 100;
const unsigned lg_num_bins = 6;

#ifndef NULL
#define NULL 0
#endif

static void do_test1(struct hash_table *theHashTablePtr) {
   /* add 100 elements, with unique keys */
   unsigned lcv;
   
   for (lcv=0; lcv < max_num_elems; ++lcv) {
      assert(0 == hash_table_get(theHashTablePtr, lcv));

      if (!hash_table_add_new(theHashTablePtr,
                              (void*)(lcv + 0x1000),  /* data */
                              lcv /* key */
                              ))
         assert(false);

      assert((void*)(lcv + 0x1000) == hash_table_get(theHashTablePtr, lcv));
   }

   // Re-check for all of the keys
   for (lcv=0; lcv < max_num_elems; ++lcv) {
      assert((void*)(lcv + 0x1000) == hash_table_get(theHashTablePtr, lcv));
   }

   // Delete all of the keys
   for (lcv=0; lcv < max_num_elems; ++lcv) {
      assert(0 != hash_table_get(theHashTablePtr, lcv));

      if (!hash_table_remove(theHashTablePtr, lcv))
         assert(false);
      
      assert(0 == hash_table_get(theHashTablePtr, lcv));
   }

   for (lcv=0; lcv < max_num_elems; ++lcv)
      assert(0 == hash_table_get(theHashTablePtr, lcv));
}

static void do_test2(struct hash_table *theHashTablePtr) {
   /* A test using tougher keys */
   unsigned lcv;
   
   for (lcv=0; lcv < max_num_elems; ++lcv) {
      unsigned key = 1 + (lcv * 5381);
      void *val = (void*)(key + 1);

      assert(0 == hash_table_get(theHashTablePtr, key));

      if (!hash_table_add_new(theHashTablePtr, val, key))
         assert(false);
      
      assert(val == hash_table_get(theHashTablePtr, key));

      if (!hash_table_remove(theHashTablePtr, key))
         assert(false);
      
      assert(0 == hash_table_get(theHashTablePtr, key));
   }
}

static void do_test3() {
   struct stacklistitem *head;
   unsigned lcv, outer;
   const unsigned max_num_threads = 50;
   
   stacklist_initialize(&head, max_num_threads);

   for (outer=0; outer < 1000; ++outer) {
      for (lcv=0; lcv < max_num_threads; ++lcv) {
         void **timers = stacklist_get(&head);
         assert(timers != NULL);
         stacklist_free(&head, timers);
      }
   }
   
   for (lcv=0; lcv < max_num_threads; ++lcv) {
      void **timers = stacklist_get(&head);
      assert(timers != NULL);
   }
   assert(NULL == stacklist_get(&head));

   stacklist_destroy(&head);
}

static void do_test4() {
   struct all_vtimers all_vtimers;
   
   initialize_all_vtimers(&all_vtimers);

   /* The number of vtimers that may be added: max_num_vtimers minus 1 */
   const unsigned num = max_num_vtimers - 1;

   for (unsigned lcv=0; lcv < num; ++lcv) {
      void *addr = (void*)(0x2000 + lcv);
      
      addto_all_vtimers(&all_vtimers, addr);
      removefrom_all_vtimers(&all_vtimers, addr);

      assert(all_vtimers.finish == all_vtimers.vtimer_addresses);
   }
   
   destroy_all_vtimers(&all_vtimers);
}

int main(int argc, char **argv) {
   struct hash_table theHashTable;
   hash_table_elem all_elems_space[max_num_elems];
   unsigned bins_space[1U << lg_num_bins];
   
   hash_table_initialize(&theHashTable,
                         all_elems_space,
                         max_num_elems,
                         bins_space,
                         lg_num_bins
                         );

   do_test1(&theHashTable);
   do_test1(&theHashTable);
   do_test2(&theHashTable);
   do_test2(&theHashTable);
   
   do_test1(&theHashTable);
   do_test1(&theHashTable);
   do_test2(&theHashTable);
   do_test2(&theHashTable);

   hash_table_destroy(&theHashTable);

   do_test3();

   /* test all_vtimers */
   do_test4();
}
