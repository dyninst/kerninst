/* hash_table.c: code to manage a hash table, indexed by thread id,
   of ptrs (to vector of vtimer addresses).

   ***No memory allocation whatsoever is performed by this code***
   ***Everything's preallocated***
*/

#include "hash_table.h"
#include <limits.h>
#include <assert.h>

// -----------------------------------------------------------------------------------
// These are only called by test program & by kperfmon, not by kernel instrumentation:
// -----------------------------------------------------------------------------------
void hash_table_initialize(struct hash_table *ht) {
   // Allocation has already been done

   unsigned *bins_finish, *bins_iter;

   ht->num_elems = 0;

   bins_iter = ht->bins;
   bins_finish = bins_iter + hash_table_num_bins; // ptr arithmetic

   while (bins_ptr != bins_end_ptr)
      *bins_ptr++ = NULL;
}

void hash_table_destroy(struct hash_table *ht) {
   // Nothing to do; memory for bins[] and all_elems[] were allocated by the
   // outside world and passed in to hash_table_initialize().  To be consistent,
   // since we didn't allocate those things, we shouldn't presume that we know
   // how to delete them either.
}

// -----------------------------------------------------------------------------------
// These are only called by kernel instrumentation & test program, not by kperfmon:
// -----------------------------------------------------------------------------------

unsigned hash_table_hash_func(unsigned key) {
   // kind of expensive, but at least it doesn't involve any memory ops.

   unsigned result = 0;

   while (key) {
      result = (result << 1) + result + (key & 0x3);
      key >>= 2;
   }

   return result;
}

void hash_table_pack(struct hash_table *ht) {
   xxx;
}


int hash_table_add_new(struct hash_table *ht, void *data, unsigned key) {
   // returns true iff successful, false if error (already exists, or perhaps ovflw)

   unsigned bin;
   struct hash_table_elem *e;

   // Pack, if needed
   if (ht->num_elems >= hash_table_max_num_elems)
      hash_table_pack(ht);
   
   // Check for overflow
   if (ht->num_elems >= hash_table_max_num_elems)
      return 0;

   // Append to all_elems[] and put at the start of the chosen bin
   e = &ht->all_elems[ht->num_elems];
   e->the_key = key;
   e->the_data = data;
   e->removed = 0;

   // Note that we deferred the call to hash_func() until now.  There's a good
   // reason: at this point in the function, the data and key input params are
   // no longer needed, so it's possible for both us *and* hash_table_hash_func
   // to be optimized leaf fns!

   bin = hash_table_hash_func(key);
   bin &= hash_table_mask_for_mod_numbins;

   e->next = ht->bins[bin]; /* could be NULL */
   ht->bins[bin] = e;
   
   ht->num_elems++;

   return 1; // success
}

static hash_table_elem* hash_table_keyandbin2elem(struct hash_table *ht,
                                                  unsigned key,
                                                  unsigned bin) {
   // returns element if found; NULL if not found.

   // We've got a bin, now search this bin
   unsigned ndx = ht->bins[bin];
   while (ndx != UINT_MAX) {
      struct hash_table_elem *e = &ht->all_elems[ndx];
      if (e->the_key == key)
         return e;
      ndx = e->next;
   }

   return 0;
}

static hash_table_elem* hash_table_lookup(struct hash_table *ht, unsigned key) {
   // returns element if found; NULL if not found.

   const unsigned bin = hash_table_hash_func(key) & hash_table_mask_for_mod_numbins;
   return hash_table_keyandbin2elem(ht, key, bin);
      // returns element if found; NULL if not found.
}

void *hash_table_remove(unsigned key) {
   unsigned bin = hashfunc(key) & hash_table_mask_for_mod_numbins;
   hash_table_elem *e = hash_table_keyandbin2elem(ht, key, bin);
      // returns element if found, NULL if not found
   if (e == NULL)
      // failure
      return 0;

   e->removed = true;
   return e->the_data;
}

