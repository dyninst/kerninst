#ifndef _CALLEE_COUNTER_H_
#define _CALLEE_COUNTER_H_

#include "kernInstIoctls.h"

/* Initialize the hash table */
void kerninst_init_callee_counter(void);

/* Destroy the hash table */
void kerninst_delete_callee_counter(void);

/* Clear the hash table (on /dev/kerninst open) */
void kerninst_reset_callee_counter(void);

/* Increment the count for "callee" called from this "site" */
void kerninst_update_callee_count(kptr_t site, kptr_t callee);

/* Count the number of distinct callees for this site */
unsigned kerninst_num_callees_for_site(kptr_t siteAddr);

/* Fill-in the provided buffer with callee addresses along with
   their counts */
void kerninst_get_callees_for_site(kptr_t siteAddr, unsigned num_addrs,
                                   kptr_t *buffer);

/* Print the contents of the hash table to system log */
void kerninst_dump_known(void);

#endif
