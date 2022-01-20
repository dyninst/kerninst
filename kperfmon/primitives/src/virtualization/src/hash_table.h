// hash_table.h
// In-kernel code for managing a hash table of void* entries with an unsigned key.

// -----------------------------------------------------------------------------
// Into kperfmon: hash_table_initialize and hash_table_destroy
//
// Into kernel: hash_table_add_new, hash_table_remove,
//              hash_table_hash_func, and hash_table_keyandbin2elem
// -----------------------------------------------------------------------------

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef kptr_t uinthash_t;

#define hash_table_elem_key_field_offset (0)
#define hash_table_elem_removed_field_offset (sizeof(uinthash_t))
#define hash_table_elem_data_field_offset (2*sizeof(uinthash_t))
#define hash_table_elem_next_field_offset (2*sizeof(uinthash_t) + sizeof(kptr_t))
#define hash_table_elem_nbytes (2*sizeof(uinthash_t) + 2*sizeof(kptr_t))

/* size of a pointer, basically */
#define hashtable_bin_nbytes (sizeof(kptr_t))

/* Compute log(size) for efficient multiplication/division */
#ifndef _KDRV64_
   #define hash_table_elem_log_nbytes (4)
   #define hashtable_bin_log_nbytes (2)
#else
   #define hash_table_elem_log_nbytes (5)
   #define hashtable_bin_log_nbytes (3)
#endif

struct hash_table_elem {
   uinthash_t the_key;
   uinthash_t removed; /* 1 --> removed; 0 --> not removed */
   kptr_t the_data;
      /* in our case, a vector of vtimer addresses, but that hardly matters */
   kptr_t next; /* making this a ptr instead of an ndx into all_elems[]
		   does indeed appear to speed things up nicely */
};

#define hash_table_max_num_elems (256)
#define hash_table_num_bins (256)
#define hash_table_mask_for_mod_numbins (255)
   // bit-and with this constant to achieve the effect of:
   // x % table_table_num_bins

#define hash_table_nbytes ((hash_table_max_num_elems * hash_table_elem_nbytes) + (hash_table_num_bins * hashtable_bin_nbytes) + sizeof(uinthash_t))

#define hash_table_offset_to_bins (hash_table_max_num_elems * hash_table_elem_nbytes)
#define hash_table_offset_to_numelems (hash_table_offset_to_bins + sizeof(kptr_t) * hash_table_num_bins)

struct hash_table {
   hash_table_elem all_elems[hash_table_max_num_elems];
   /* 
      for each bin, points to the element (within all_elems[]) that's at the
      head of the bin.  If NULL then the bin is empty.  Using pointers
      instead of ndx's into all_elems[] makes for faster code 
   */
   kptr_t bins[hash_table_num_bins];
   uinthash_t num_elems; // tells us how much of all_elems is presently defined
};

// -----------------------------------------------------------------------------------
// These are only called by test program & by kperfmon, not by kernel instrumentation:
// -----------------------------------------------------------------------------------

void hash_table_initialize(struct hash_table *ht);
   // assumes that allocation has already been done
void hash_table_destroy(struct hash_table *ht);

// -----------------------------------------------------------------------------------
// These are only called by kernel instrumentation & test program, not by kperfmon:
// -----------------------------------------------------------------------------------

hash_table_elem* hash_table_keyandbin2elem(struct hash_table *ht,
                                           uinthash_t key, uinthash_t bin);
unsigned hash_table_hash_func(uinthash_t key);
int hash_table_add_new(struct hash_table *ht, kptr_t data, uinthash_t key);
   /* returns true iff successful */
void* hash_table_remove(uinthash_t key);
   /* To save a robust reg, the hash table address is assumed, not passed in */
   /* returns data of the element it removed, iff successful; else returns NULL */

#ifdef __cplusplus
}
#endif

#endif
