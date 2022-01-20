// all_vtimers.c
// see .h file for comments

#include "kerninst.h" // struct hwprfunit_t
#include "all_vtimers.h"
#include "hash_table.h" // struct hash_table
#include "stacklist.h" // struct saved_vtimer

/* These must always be included last: */
#include <sys/ddi.h>
#include <sys/sunddi.h>

// -----------------------------------------------------------------
// Code executed by the driver, but not by kernel instrumentation
// -----------------------------------------------------------------

extern void *statep;

void initialize_all_vtimers(struct all_vtimers *the_vtimers)
{
   // Memory space for the_vtimers has already been allocated. 
   // No sub-allocations are needed.
   void **start = (void**)the_vtimers->vtimer_addresses;
   void **iter = start;
   void **finish = (void**)(the_vtimers->vtimer_addresses + max_num_vtimers);

   the_vtimers->finish = (kptr_t)start;

   while (iter != finish)
      *iter++ = NULL;
}

void destroy_all_vtimers(struct all_vtimers *the_vtimers) 
{
   // Nothing to do.  Just for the heck of it, we'll empty out the vector
   the_vtimers->finish = (kptr_t)the_vtimers->vtimer_addresses;
   the_vtimers->vtimer_addresses[0] = (kptr_t)NULL;
}

int addto_all_vtimers(struct all_vtimers *the_vtimers, void *vtimer_addr) 
{
   // Returns 0 iff successful; -1 iff not (overflow)
   void **new_finish;
   void **old_finish;

   int instance = 0; /* May be safer to do a getminor(dev)!!! */
   hwprfunit_t* usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);

   uint32_t old_pil_level = grab_cswitch_lock(&usp->kerninst_cswitch_lock);

   old_finish = (void**)the_vtimers->finish;
   if(*old_finish != NULL) {
      cmn_err(CE_WARN, "kerninst: addto_all_vtimers - *all_vtimers->finish != NULL\n");
      release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
      return -1;
   }

   if(old_finish < (void**)the_vtimers) {
      // sanity check [underflow]
      cmn_err(CE_WARN, "kerninst: addto_all_vtimers - all_vtimers->finish < &all_vtimers->vtimer_addresses[0]\n");
      release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
      return -1;
   }

   new_finish = old_finish + 1; // ptr arithmetic

   if (new_finish == (void**)&the_vtimers->vtimer_addresses[max_num_vtimers]) {
      release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
      return 1; // overflow; return failure
   }

   *new_finish = NULL;
   *old_finish = vtimer_addr;
   the_vtimers->finish = (kptr_t)new_finish;

   release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
   return 0; // success
}

void removefrom_hashtable(struct hash_table *ht, void *vtimer_addr)
{
   unsigned iter;
   struct hash_table_elem *e;
   struct saved_timer *st_iter, *st_next, *st_finish;
   for(iter = 0; iter < ht->num_elems; iter++) {
      e = &ht->all_elems[iter];
      if(! e->removed) {
         st_iter = (struct saved_timer *)e->the_data;
         st_finish = st_iter + max_num_vtimers;
         while(st_iter != st_finish) {
            if((void *)((kptr_t)st_iter->addr) == vtimer_addr) {
               st_next = st_iter + 1;
               while(st_next != st_finish) {
                  st_iter->addr = st_next->addr;
                  st_iter->auxdata = st_next->auxdata;
                  st_next++;
                  st_iter++;
               }
               st_iter->addr = (largeptr_t)NULL;
               st_iter->auxdata = 0;
            }
            st_iter++;
         }
      }
   }
}

int removefrom_all_vtimers(struct all_vtimers *the_vtimers, void *vtimer_addr) 
{
   void **iter, **finish, **last;

   int instance = 0; /* May be safer to do a getminor(dev)!!! */
   hwprfunit_t* usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);

   uint32_t old_pil_level = grab_cswitch_lock(&usp->kerninst_cswitch_lock);

   // Search to find this item
   iter = (void**)the_vtimers->vtimer_addresses;
   finish = (void**)the_vtimers->finish; // 1 past last
   last = finish - 1; // the last non-NULL entry
   
   while (iter != finish) {
      if (*iter == vtimer_addr) { // found!
         *iter = *last;
         // NOTE: a duplicate entry can temporarily be seen
         *last = NULL;
         the_vtimers->finish = (kptr_t)last;
         
         // Now remove vtimer from any saved_vtimers
         removefrom_hashtable(&usp->ht_vtimers, vtimer_addr);

         release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
         return 0; // success
      }
      else
         ++iter;
   }
   
   release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
   return -1; // failure
}

//------------------------------------------------------------
// Following routines defined here for lack of a better place
//------------------------------------------------------------

extern uint32_t ari_raise_pil(uint32_t level);
extern void ari_set_pil(uint32_t level);
extern uint32_t get_disp_level();

// Raise PIL and grab cswitch lock 
uint32_t grab_cswitch_lock(kmutex_t *the_lock)
{
   uint32_t disp_level = get_disp_level();
   uint32_t old_level = ari_raise_pil(disp_level);
   mutex_enter(the_lock);
   return old_level;
}

// Reset PIL and release cswitch lock
void release_cswitch_lock(uint32_t orig_level, kmutex_t *the_lock)
{
   mutex_exit(the_lock);
   ari_set_pil(orig_level);
}
