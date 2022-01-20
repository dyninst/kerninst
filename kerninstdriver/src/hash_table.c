// hash_table.c

/*
   ***No memory allocation whatsoever is performed by this code***
   ***Everything's preallocated***
*/

#include "hash_table.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

void hash_table_initialize(struct hash_table *ht) {
   // Allocation has already been done
   void **bins_finish, **bins_iter;

   ht->num_elems = 0;
   bins_iter = &ht->bins[0];
   bins_finish = bins_iter + ht_num_bins; // ptr arithmetic

   while (bins_iter != bins_finish)
      *bins_iter++ = NULL;
}

void hash_table_destroy(struct hash_table *ht) {
   // we don't unallocate memory held by ht, since we're not the ones
   // who allocated it in the first place, and we don't even know how
   // it was allocated. so, we just make it unusable, and the init
   // routine is as good as any to accomplish that
   hash_table_initialize(ht);
}

static unsigned hash_table_hash_func(uint64_t key) {
   // kind of expensive, but at least it doesn't involve any memory ops.
   unsigned result = 0;

   while (key) {
      result = (result << 1) + result + (key & 0x3);
      key >>= 2;
   }
   return result;
}

static struct hash_table_elem* hash_table_keyandbin2elem(struct hash_table *ht,
                                                         uint64_t key,
                                                         unsigned bin) {
   // returns element if found; NULL if not found.

   // We've got a bin, now search this bin
   void *ndx = ht->bins[bin];
   while (ndx != NULL) {
      struct hash_table_elem *e = (struct hash_table_elem *)ndx;
      if (e->the_key == key)
         return e;
      ndx = e->next;
   }

   return NULL;
}

void hash_table_pack(struct hash_table *ht) {
   // Remove "removed" elements by compacting the all_elems array
   struct hash_table_elem *to, *from;
   unsigned ndx, bin;
   unsigned to_ndx = 0;

   // Compact all_elems
   to = &ht->all_elems[0];
   from = &ht->all_elems[ht->num_elems - 1];
   while(to != from) {
      if(!to->removed) {
         to++;
         to_ndx++;
      }
      else {
         if(!from->removed) {
            to->the_key = from->the_key;
            to->the_data = from->the_data;
            to->next = NULL;
            to->removed = 0;
            to++;
            to_ndx++;
            if(to != from) from--;
         }
         else
            from--;
      }
   }

   // Now recreate contents of bins
   for(ndx = 0; ndx < ht_num_bins; ndx++) {
      ht->bins[ndx] = NULL;
   }
   ht->num_elems = 0;
   for(ndx = 0; ndx <= to_ndx; ndx++) {
      to = &ht->all_elems[ndx];
      if(!to->removed) {
         bin = hash_table_hash_func(to->the_key);
         bin &= ht_mask_for_mod_numbins;
         to->next = ht->bins[bin]; /* could be NULL */
         ht->bins[bin] = to;   
         ht->num_elems++;
      }
   }
}

int hash_table_add(struct hash_table *ht, void *data, uint64_t key) {
   // returns 0 iff successful, 
   //         -1 if error (already exists, or perhaps overflow)

   unsigned bin;
   struct hash_table_elem *e, *prev;

   // Pack if necessary
   if (ht->num_elems == ht_max_num_elems) {
      hash_table_pack(ht);
      // and check for overflow
      if (ht->num_elems == ht_max_num_elems)
         return -1;
   }

   bin = hash_table_hash_func(key);
   bin &= ht_mask_for_mod_numbins;

   e = &ht->all_elems[ht->num_elems];
   prev = hash_table_keyandbin2elem(ht, key, bin);
   if(prev != NULL) { 
      if(prev->removed != 1)
         return -1; // already exists
      else
         e = prev; // overwrite removed
   }

   // Append to all_elems[] and put at the start of the chosen bin
   e->the_key = key;
   e->the_data = data;
   e->removed = 0;

   if(e != prev) {
      e->next = ht->bins[bin]; /* could be NULL */
      ht->bins[bin] = e;   
      ht->num_elems++;
   }

   return 0; // success
}

void* hash_table_remove(struct hash_table *ht, uint64_t key) {
   unsigned bin = hash_table_hash_func(key) & ht_mask_for_mod_numbins;
   struct hash_table_elem *e = hash_table_keyandbin2elem(ht, key, bin);
      // returns element if found, NULL if not found

   if (e == NULL)
      // failure
      return e;

   e->removed = 1;

   // Note that we don't decrement ht->num_elems, since we do adds by grabbing
   // the element at all_elems[num_elems] or by overwriting a previously
   // removed element with the same key. If we did decrement num_elems, then
   // we would most likely end up overwriting a valid (key,data)

   return e->the_data;
}

void* hash_table_lookup(struct hash_table *ht, uint64_t key) {
   unsigned bin = hash_table_hash_func(key) & ht_mask_for_mod_numbins;
   struct hash_table_elem *e = hash_table_keyandbin2elem(ht, key, bin);
      // returns element if found, NULL if not found

   if (e == NULL)
      // failure
      return NULL;

   return e->the_data;
}

int hash_table_update(struct hash_table *ht, uint64_t key, void *data) {
   unsigned bin = hash_table_hash_func(key) & ht_mask_for_mod_numbins;
   struct hash_table_elem *e = hash_table_keyandbin2elem(ht, key, bin);
      // returns element if found, NULL if not found

   if (e == NULL)
      // failure
      return -1;

   e->the_data = data;
   return 0;
}

