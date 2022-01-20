#ifndef _CALLEE_COUNTER_H_
#define _CALLEE_COUNTER_H_

#include "util/h/kdrvtypes.h"

#ifdef __cplusplus
extern "C" {
#endif
    /* Initialize the hash table (on _init) */
    void kerninst_init_callee_counter();
    /* Destroy the hash table (on _fini) */
    void kerninst_delete_callee_counter();
    /* Clear the hash table (on /dev/kerninst open) */
    void kerninst_reset_callee_counter();

    /* Increment the count for "callee" called from this "site" */
    void kerninst_update_callee_count(kptr_t site, kptr_t callee);

    /* Count the number of distinct callees for this site */
    unsigned kerninst_num_callees_for_site(kptr_t siteAddr);

    /* Fill-in the provided buffer with callee addresses along with
       their counts */
    void kerninst_get_callees_for_site(kptr_t siteAddr, unsigned num_addrs,
				       kptr_t *buffer);
    /* Print the contents of the hash table to syslog */
    void kerninst_dump_known();

    /* Assembly routines for atomic hash table manipulation: */
    
    /* Reserve a free spot in the hash table */
    unsigned kerninst_reserve_cell(unsigned *pos, unsigned num_total);

    /* Insert a cell in the linked list atomically:
       new->next = current->next, current->next = new */
    void kerninst_chain_cell(void *addr_current_next, void *addr_new_next,
			     void *addr_new);
    
    /* Increment count atomically */
    void kerninst_inc_call_count(unsigned *count);
#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif
