#include "calleeCounter.h"

#include <sys/ksynch.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>

typedef struct callee_info {
    kptr_t site_addr;
    kptr_t callee_addr;
    unsigned call_count;
    struct callee_info *next;
} callee_info_t;

static const unsigned NUM_TOTAL = 8192;
static const unsigned NUM_BINS = 512;
static const unsigned NUM_BINS_MASK = 0x1FF; /* To avoid expensive % oper */

static callee_info_t *htable = NULL;
static unsigned free_pos = 0;

void kerninst_init_callee_counter()
{
    if (htable != NULL) {
	cmn_err(CE_WARN, "kerninst_init_callee_counter: htable != NULL");
    }
    htable = (callee_info_t *)kmem_alloc(NUM_TOTAL * sizeof(callee_info_t),
					 KM_SLEEP);
    if (htable == NULL) {
	cmn_err(CE_PANIC, "kerninst_init_callee_counter: out of memory");
    }
}

void kerninst_delete_callee_counter()
{
    if (htable == NULL) {
	cmn_err(CE_PANIC, "kerninst_delete_callee_counter: htable == NULL");
    }
    kmem_free(htable, NUM_TOTAL * sizeof(callee_info_t));
    htable = NULL;
}

void kerninst_reset_callee_counter()
{
    unsigned i;

    /* Initialize header nodes */
    for (i=0; i<NUM_BINS; i++) {
	htable[i].site_addr = 0;
	htable[i].callee_addr = 0;
	htable[i].call_count = 0;
	htable[i].next = 0;
    }
    /* Free space comes right after the headers */
    free_pos = NUM_BINS;
}

/* Follows addrHash4 in util/src/hashFns.C */
static unsigned hfunc(kptr_t site_addr)
{
    kptr_t val = (site_addr >> 2);
    unsigned result = 0;

    while (val) {
	/* strip off 2 bits at a time */
	result = (result << 1) + result + (unsigned)(val & 0x3);
	val >>= 2;
    }

    return (result & NUM_BINS_MASK);
}

void kerninst_update_callee_count(kptr_t site, kptr_t callee)
{
    unsigned idx = hfunc(site);
    callee_info_t *current = &htable[idx];

    while (current->next != NULL && (current->next->site_addr != site || 
				     current->next->callee_addr != callee)) {
	current = current->next;
    }
    if (current->next == NULL) {
	/* This (site,callee) pair was not found, lets add it */
	idx = kerninst_reserve_cell(&free_pos, NUM_TOTAL); /* Atomic */
	if (idx == 0) {
	    return; /* Out of memory */
	}
	htable[idx].site_addr = site;
	htable[idx].callee_addr = callee;
	htable[idx].call_count = 1;

	/* Do htable[idx].next = current->next, current->next = &htable[idx]
	   atomically */
	kerninst_chain_cell(&current->next, &htable[idx].next, &htable[idx]);
    }
    else {
	/* Found this (site,callee) pair, increment its call count */
	kerninst_inc_call_count(&current->next->call_count); /* Atomic */
    }
    /*
      There is one problem with the approach above. We first check if the pair
      already exists and then add it to the list. If another thread adds this
      pair after our check, we would end up adding it twice. It is not a big
      deal, but be prepared to handle it.
    */
}

unsigned kerninst_num_callees_for_site(kptr_t site_addr)
{
    unsigned rv = 0;
    unsigned idx = hfunc(site_addr);
    callee_info_t *current = htable[idx].next;

    while (current != NULL) {
	if  (current->site_addr == site_addr) {
	    rv++;
	}
	current = current->next;
    }

    return rv;
}

void kerninst_get_callees_for_site(kptr_t site_addr, unsigned num_addrs,
				   kptr_t *buffer)
{
    unsigned idx = hfunc(site_addr);
    callee_info_t *current = htable[idx].next;
    unsigned num_entries = 2 * num_addrs; /* (addr,count) pairs */
    unsigned j = 0;

    while (current != NULL && j < num_entries) {
	if  (current->site_addr == site_addr) {
	    buffer[j++] = current->callee_addr;
	    buffer[j++] = current->call_count;
	}
	current = current->next;
    }
}

void kerninst_dump_known()
{
    unsigned i;

    for (i=0; i<NUM_BINS; i++) {
	callee_info_t *current = htable[i].next;
	while (current != NULL) {
	    cmn_err(CE_CONT, "kerninst_dump_known: count[0x%lx->0x%lx] = %u\n",
		    current->site_addr, current->callee_addr,
		    current->call_count);
	    current = current->next;
	}
    }
}
