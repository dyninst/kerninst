// cswitch_inout.c
#include "cswitch_inout.h"
#include "all_vtimers.h"
#include "vtimer.h"
#include "stacklist.h"
#include "hash_table.h"
#ifdef i386_unknown_linux2_4
#include "hwcounter_p4.h"
#elif defined(ppc64_unknown_linux2_4)
#include <asm/processor.h> //tb()
#endif

// NOTE: Do not put printk's in any of the functions in this file, since
//       printk can end up calling schedule() again, and the kernel will
//       lock up trying to obtain the kerninst_cswitch_lock that we 
//       already hold

#include <asm/current.h>
#include <linux/sched.h> // struct task_struct
#include <linux/spinlock.h>
#include <linux/stddef.h> // NULL
#include <linux/smp.h> // smp_processor_id()
#include <linux/version.h> // LINUX_VERSION_CODE

extern struct all_vtimers av_vtimers;
extern struct hash_table ht_vtimers;
extern struct stacklist sl_heap;
extern spinlock_t kerninst_cswitch_lock;
extern uint32_t kerninst_enable_bits;


// -------------------- Generic vstop/vrestart Routines ---------------------
unsigned generic_vstop_routine(struct vtimer *vtimer, 
			       struct saved_timer *stimer,
			       unsigned type)
{
   uint64_t counter = 0;
#ifdef i386_unknown_linux2_4
   unsigned counter_low=0, counter_high=0;
   unsigned ctr_addr = 0;
#endif

   unsigned kind = KIND_OF(type);
   struct vtimer_start *vt_start;
   vt_start = (struct vtimer_start*)&stimer->auxdata;
   // Note: stimer may be NULL, since we have a compile-time limit on the
   //       number of saved_timer's. We still want to vstop the vtimer in 
   //       this case, but don't write any data to vt_start

#ifdef i386_unknown_linux2_4
   // Read appropriate hw_counter based on type
   if(kind == 1) /* HWC_TICKS */ {
      rdtsc(counter_low, counter_high);
   }
   else if((kind != 0) && 
           (kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS)) {
      ctr_addr = COUNTER_ADDR(CTR_OF(type));
      __asm__ __volatile__ ("rdmsr"
                            : "=a" (counter_low), "=d" (counter_high)
                            : "c" (ctr_addr));
   }
   else { // this shouldn't happen, but we'll be safe
      vtimer->start_field.start = 0;
      vtimer->start_field.rcount = 0;
      return 0; /* don't add saved timer to curr thr */
   }
   counter = (uint64_t)(counter_high & 0x0000FFFFUL);
   counter <<= 32;
   counter |= (uint64_t)counter_low;
#elif defined(ppc64_unknown_linux2_4) 
   if(kind == 1) /* HWC_TICKS */ {
      counter = mftb() & 0x0000FFFFFFFFFFFF; 
   }  else if((kind != 0) &&
           (kerninst_enable_bits & KERNINST_ENABLE_POWER4COUNTERS)) {
      counter = read_pmc(CTR_OF(type)); 
   } else { // this shouldn't happen, but we'll be safe
      vtimer->start_field.start = 0;
      vtimer->start_field.rcount = 0;
      return 0; /* don't add saved timer to curr thr */
   }
#else
#error "unknown architecture"
#endif

   // Store previous rcount and stop time in saved timer
   if(stimer != NULL) {
      vt_start->start = counter;
      vt_start->rcount = vtimer->start_field.rcount;
   }

   // Update vtimer total and zero out start_field; protect against rollback
   if(counter > vtimer->start_field.start)
      vtimer->total_field += (counter - vtimer->start_field.start);
   vtimer->start_field.start = 0;
   vtimer->start_field.rcount = 0;

   return 1; /* add saved timer to curr thr */
}

void generic_vrestart_routine(struct vtimer *vtimer, uint64_t auxdata,
                              unsigned type)
{
   struct vtimer_start *old_start;
   uint64_t counter = 0;
#ifdef i386_unknown_linux2_4   
   unsigned counter_low=0, counter_high=0;
   unsigned ctr_addr = 0;
#endif
   unsigned kind = KIND_OF(type);
   old_start = (struct vtimer_start*)&auxdata;
#ifdef i386_unknown_linux2_4
   // Read appropriate hw_counter based on type
   if(kind == 1) { /* HWC_TICKS */
      rdtsc(counter_low, counter_high);
   }
   else if((kind != 0) && 
           (kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS)) {
      ctr_addr = COUNTER_ADDR(CTR_OF(type));
      __asm__ __volatile__ ("rdmsr"
			    : "=a" (counter_low), "=d" (counter_high)
			    : "c" (ctr_addr));
   }
   else { // this shouldn't happen, but we'll be safe
      vtimer->start_field.start = old_start->start;
      vtimer->start_field.rcount = old_start->rcount;
      return;
   }

   counter = (uint64_t)(counter_high & 0x0000FFFFUL);
   counter <<= 32;
   counter |= (uint64_t)counter_low;
#elif defined(ppc64_unknown_linux2_4) 
   if(kind == 1) { /* HWC_TICKS */
     counter = mftb();   
   } else  if((kind != 0) &&
           (kerninst_enable_bits & KERNINST_ENABLE_POWER4COUNTERS)) {
     counter =  read_pmc(CTR_OF(type));
   } else { // this shouldn't happen, but we'll be safe
      vtimer->start_field.start = old_start->start;
      vtimer->start_field.rcount = old_start->rcount;
      return;
   }
#else
#error "unknown architecture"
#endif

   vtimer->start_field.start = counter;
   vtimer->start_field.rcount = old_start->rcount;

   // If wall timer, add events switched out to total
   if(IS_WALL_TIMER(type) && (counter > old_start->start)) {
      vtimer->total_field += (counter - old_start->start);
   }   
}

// -------------------- Context Switch In/Out Routines ---------------------

// The following define is based on knowledge of how vtimers are allocated
// by the global data heap of kperfmon. If that allocation method ever changes,
// we will need to make sure we're still able to get the right vtimer for
// the given cpu.
#define vtimer_primitive_stride_in_bytes 64 

struct vtimer* getCPUTimer(struct vtimer *vt_base, unsigned cpu) {
   char *tmp = (char *)vt_base;
   tmp += (cpu * vtimer_primitive_stride_in_bytes);
   return (struct vtimer*)tmp;
}

void on_switch_in(void)
{
   struct vtimer *theTimer, *cpu_vt;
   struct saved_timer *timers_iter, *timers_start;
   uint64_t auxdata;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
   struct task_struct *curr = current;
   unsigned curr_thr = (unsigned)curr->pid;
#else
   unsigned curr_thr = (unsigned)current_thread_info();
#endif

   unsigned curr_cpu = (unsigned)smp_processor_id();

   if(kerninst_enable_bits & KERNINST_ENABLE_CONTEXTSWITCH) {
      spin_lock(&kerninst_cswitch_lock);
   
      // From the hash table, get the stored vector-of-timer-ptrs for current
      // thread. If none exists, then we don't need to do anything more

      timers_start = hash_table_remove(&ht_vtimers, curr_thr);

      if (timers_start == NULL) { // probably the common case
         spin_unlock(&kerninst_cswitch_lock);
         return;
      }

      timers_iter = timers_start;
      theTimer = (struct vtimer *)(timers_iter->addr);
      auxdata = timers_iter->auxdata;

      while(theTimer != NULL) {
         cpu_vt = getCPUTimer(theTimer, curr_cpu);
         
         // call the restart routine
         generic_vrestart_routine(cpu_vt, auxdata, theTimer->counter_type);

         timers_iter++;
         theTimer = (struct vtimer *)(timers_iter->addr);
         auxdata = timers_iter->auxdata;
      }

      // Return this vector of pointers to the free list
      stacklist_free(&sl_heap, timers_start);

      spin_unlock(&kerninst_cswitch_lock);
   }
}

extern unsigned number_unsaved_timers;

void on_switch_out(void)
{
   int grabbed_freelistentry_yet = 0;

   void **iter, **finish;

   struct saved_timer *vec_of_timers = NULL;
   struct saved_timer *vec_of_timers_iter = NULL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
   struct task_struct *curr = current;
   unsigned curr_thr = (unsigned)curr->pid;
#else
   unsigned curr_thr = (unsigned)current_thread_info();
#endif

   unsigned curr_cpu = (unsigned)smp_processor_id();

   unsigned add_saved_timer = 0;

   /* TODO: We need to find a way to identify processes/threads that are
            being switched out for the final time (i.e., they're exiting).
            In such cases, we shouldn't save any vtimers for that
            process/thread.
   */

   if(kerninst_enable_bits & KERNINST_ENABLE_CONTEXTSWITCH) {
      spin_lock(&kerninst_cswitch_lock);

      iter = (void**)av_vtimers.vtimer_addresses;
      finish = (void**)av_vtimers.finish;

      // Loop through all vtimers
      while (iter != finish) {
         struct vtimer *theTimer = (struct vtimer *)*iter++;
         struct vtimer *cpu_vt = getCPUTimer(theTimer, curr_cpu);
         // read start field to get the state
         if(cpu_vt->start_field.rcount != 0) { // timer started
            if (!grabbed_freelistentry_yet) {
               vec_of_timers = (struct saved_timer*)stacklist_get(&sl_heap);
               if(vec_of_timers != NULL) {
                  vec_of_timers_iter = vec_of_timers;
                  grabbed_freelistentry_yet = 1;
               }
               else // stacklist is full, can't save any more timers
                  number_unsaved_timers++;
            }
            // Call stop_routine, which updates saved_timer.auxdata, 
            // vtimer.total_field, and vtimer.start_field
            add_saved_timer = generic_vstop_routine(cpu_vt, 
						    vec_of_timers_iter, 
						    theTimer->counter_type);
            
            // Add to working list:
            if((vec_of_timers != NULL) && add_saved_timer) {
               (struct vtimer*)(vec_of_timers_iter->addr) = theTimer;
               vec_of_timers_iter++;
            }
         }
      }
      if (grabbed_freelistentry_yet) { // was anything done?
         // What to set the key to? id of current thread
         unsigned theKey = curr_thr;
	 
         // Mark the end of the list
         (struct vtimer*)(vec_of_timers_iter->addr) = NULL;

         // Add to the hash table!
         hash_table_add(&ht_vtimers, 
                        vec_of_timers, // not vec_of_timers_iter!
                        theKey);
      }
      spin_unlock(&kerninst_cswitch_lock);
   }
}
