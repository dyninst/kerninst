#include "calleeCounter.h"

#include <linux/vmalloc.h> 
#include <linux/spinlock.h> // spinlock_t

#include <linux/vmalloc.h>
extern uint32_t kerninst_debug_bits;

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

static spinlock_t kerninst_callee_htable_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t kerninst_callee_count_lock = SPIN_LOCK_UNLOCKED;

void kerninst_init_callee_counter(void)
{
   if(htable != NULL) {
      printk(KERN_WARNING "kerninst_init_callee_counter: htable != NULL");
   }
   htable = (callee_info_t *)vmalloc(NUM_TOTAL * sizeof(callee_info_t));
   if(htable == NULL) {
      printk(KERN_ERR "kerninst_init_callee_counter: out of memory");
   }
}

void kerninst_delete_callee_counter(void)
{
   if(htable == NULL) {
      printk(KERN_ERR "kerninst_delete_callee_counter: htable == NULL");
   }
   vfree(htable);
   htable = NULL;
}

void kerninst_reset_callee_counter(void)
{
   unsigned i;

   /* Initialize header nodes */
   for(i=0; i<NUM_BINS; i++) {
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

   while(val) {
      /* strip off 2 bits at a time */
      result = (result << 1) + result + (unsigned)(val & 0x3);
      val >>= 2;
   }

   return (result & NUM_BINS_MASK);
}

void kerninst_update_callee_count(kptr_t site, kptr_t callee)
{
   unsigned idx = hfunc(site);
   callee_info_t *curr = &htable[idx];

   if(kerninst_debug_bits & KERNINST_DEBUG_CALLEES)
      printk(KERN_INFO "kerninst: updating callee count for callsite 0x%08lx, destination 0x%08lx\n", (unsigned long)site, (unsigned long)callee);

   while(curr->next != NULL && (curr->next->site_addr != site || 
                                   curr->next->callee_addr != callee)) {
      spin_lock(&kerninst_callee_htable_lock);
      curr = curr->next;
      spin_unlock(&kerninst_callee_htable_lock);
   }
   if(curr->next == NULL) {      
      /* This (site,callee) pair was not found, lets add it */
      spin_lock(&kerninst_callee_htable_lock);
      idx = (free_pos < (NUM_TOTAL-1) ? ++free_pos : 0);
      if (idx == 0) {
         spin_unlock(&kerninst_callee_htable_lock);
         return; /* Out of memory */
      }
      htable[idx].site_addr = site;
      htable[idx].callee_addr = callee;
      htable[idx].call_count = 1;
      
      /* Do htable[idx].next = curr->next, curr->next = &htable[idx] */
      htable[idx].next = curr->next;
      curr->next = &htable[idx];
      spin_unlock(&kerninst_callee_htable_lock);
   }
   else {
      /* Found this (site,callee) pair, increment its call count */
      spin_lock(&kerninst_callee_count_lock);
      curr->next->call_count++;
      spin_unlock(&kerninst_callee_count_lock);
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
   callee_info_t *curr = htable[idx].next;

   while(curr != NULL) {
      if(curr->site_addr == site_addr)
         rv++;
      spin_lock(&kerninst_callee_htable_lock);
      curr = curr->next;
      spin_unlock(&kerninst_callee_htable_lock);
   }

   return rv;
}

void kerninst_get_callees_for_site(kptr_t site_addr, unsigned num_addrs,
				   kptr_t *buffer)
{
   unsigned idx = hfunc(site_addr);
   callee_info_t *curr = htable[idx].next;
   unsigned num_entries = 2 * num_addrs; /* (addr,count) pairs */
   unsigned j = 0;
   
   while(curr != NULL && j < num_entries) {
      if(curr->site_addr == site_addr) {
         buffer[j++] = curr->callee_addr;
         buffer[j++] = curr->call_count;
      }
      spin_lock(&kerninst_callee_htable_lock);
      curr = curr->next;
      spin_unlock(&kerninst_callee_htable_lock);
   }
}

void kerninst_dump_known(void)
{
   unsigned i;

   for(i=0; i<NUM_BINS; i++) {
      callee_info_t *curr = htable[i].next;
      while(curr != NULL) {
         printk(KERN_INFO "kerninst_dump_known: count[0x%08lx->0x%08lx] = %u\n",
                (unsigned long)curr->site_addr, 
	        (unsigned long)curr->callee_addr,
                curr->call_count);
         spin_lock(&kerninst_callee_htable_lock);
         curr = curr->next;
         spin_unlock(&kerninst_callee_htable_lock);
      }
   }
}
