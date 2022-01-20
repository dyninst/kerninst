// hash_table.h

// Code for managing a hash table of void* entries with an unsigned key.

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#include <sys/types.h>

#define ht_elem_key_offset 0
#define ht_elem_removed_offset (sizeof(unsigned))
#define ht_elem_data_offset (2*sizeof(unsigned))
#define ht_elem_next_offset (2*sizeof(unsigned) + sizeof(void*))
#define ht_elem_nbytes (2*sizeof(unsigned) + 2*sizeof(void*))

/* size of a pointer, basically */
#define ht_bin_nbytes (sizeof(void*))

struct hash_table_elem {
   uint64_t the_key;
   unsigned removed; /* 1 --> removed; 0 --> not removed */
   void *the_data;
      /* in our case, a vector of vtimer addresses, but that hardly matters */
   void *next; 
      /* making this a ptr instead of an ndx into all_elems[]
         does indeed appear to speed things up nicely */
};

#define ht_max_num_elems 256
#define ht_num_bins 256
#define ht_mask_for_mod_numbins 255
   // bit-and with this constant to achieve the effect of:
   // x % table_table_num_bins

#define ht_nbytes ((ht_max_num_elems * ht_elem_nbytes) + (ht_num_bins * ht_bin_nbytes) + sizeof(unsigned))

#define ht_bins_offset (ht_max_num_elems * ht_elem_nbytes)
#define ht_numelems_offset (ht_bins_offset + sizeof(void*) * ht_num_bins)

struct hash_table {
   struct hash_table_elem all_elems[ht_max_num_elems];
   /* 
      for each bin, points to the element (within all_elems[]) that's at the
      head of the bin.  If NULL then the bin is empty.  Using pointers
      instead of ndx's into all_elems[] makes for faster code 
   */
   void *bins[ht_num_bins];
   unsigned num_elems; // tells us how much of all_elems is presently defined
};

void hash_table_initialize(struct hash_table *ht);
   /* assumes that allocation has already been done */

void hash_table_destroy(struct hash_table *ht);

int hash_table_add(struct hash_table *ht, void *data, uint64_t key);
   /* returns 0 iff successful, -1 otherwise */

void* hash_table_remove(struct hash_table *ht, uint64_t key);
   /* returns data of element removed, iff successful; else returns NULL */

void* hash_table_lookup(struct hash_table *ht, uint64_t key);
   /* returns data of element if found, else NULL */

int hash_table_update(struct hash_table *ht, uint64_t key, void *data);
   /* updates data of element and returns 0 if found, else returns -1 */

void hash_table_pack(struct hash_table *ht);
   /* compacts the all_elems array by removing "removed" elements */

#endif
