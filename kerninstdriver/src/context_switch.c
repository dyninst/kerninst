// context_switch.c

#include "kerninst.h" // struct hwprfunit_t
#include "all_vtimers.h"
#include "vtimer.h"
#include "stacklist.h"
#include "hash_table.h"

#include <sys/thread.h>
#include <sys/cpuvar.h>

/* These must always be included last: */
#include <sys/ddi.h>
#include <sys/sunddi.h>

extern void *statep;
extern uint64_t module_load_time;

typedef struct vfunc_stats {
   unsigned entries_to;
   unsigned kind_is_tick;
   unsigned kind_is_pic0;
   unsigned kind_is_pic1;
   unsigned bad_start_val;
   unsigned bad_tick_val;
   unsigned bad_kind;
} vfunc_stats_t;

vfunc_stats_t vstop_stats = {0,0,0,0,0,0,0};
vfunc_stats_t vrestart_stats = {0,0,0,0,0,0,0};
unsigned long long vstop_stats_addr = (unsigned long long)&vstop_stats;
unsigned long long vrestart_stats_addr = (unsigned long long)&vrestart_stats;

// -------------------- Generic vstop/vrestart Routines ---------------------
unsigned generic_vstop_routine(struct vtimer *vtimer, 
			       struct saved_timer *stimer,
			       unsigned type)
{
   uint64_t counter = 0;
   unsigned hwctr = 0;
   unsigned kind = KIND_OF(type);
   struct vtimer_start *vt_start = (struct vtimer_start*)&stimer->auxdata;
   // Note: stimer may be NULL, since we have a compile-time limit on the
   //       number of saved_timer's. We still want to vstop the vtimer in 
   //       this case, but don't write any data to vt_start

   vstop_stats.entries_to += 1;

   // Read appropriate hw_counter based on type
   if(kind == 1) /* HWC_TICKS */ {
      vstop_stats.kind_is_tick += 1;
      // read current time
      __asm__ __volatile__ ("rd %%tick, %0"
                            : "=r" (counter)
                            : /* no inputs */);
      if(counter < module_load_time)
	 vstop_stats.bad_tick_val += 1;

      // Mask counter to 48 bits
      counter <<= 16;
      counter >>= 16;
   }
   else if(kind != 0) {
      // read appropriate hw counter
      hwctr = CTR_OF(type);
      if(hwctr == 0) { /* PIC0 */
         vstop_stats.kind_is_pic0 += 1;
         // we want low 32 bits
         __asm__ __volatile__ ("rd %%pic, %0\n\t"
                               "srl %0, 0, %0" /* zeroes hi 32 bits */
                               : "=r" (counter)
                               : /* no inputs */);
      }
      else if(hwctr == 1) { /* PIC1 */
         vstop_stats.kind_is_pic1 += 1;
         // we want high 32 bits
         __asm__ __volatile__ ("rd %%pic, %0\n\t"
                               "srlx %0, 32, %0"
                               : "=r" (counter)
                               : /* no inputs */);
      }
      else { 
         // this shouldn't happen, but we'll be safe
	 vstop_stats.bad_kind += 1;
         vtimer->start_field.start = 0;
         vtimer->start_field.rcount = 0;
         return 0; /* don't add saved timer to curr thr */
      }
   }
   else { // this shouldn't happen, but we'll be safe
      vstop_stats.bad_kind += 1; 
      vtimer->start_field.start = 0;
      vtimer->start_field.rcount = 0;
      return 0; /* don't add saved timer to curr thr */
   }

   if(stimer != NULL) {
      // Store previous rcount and stop time in saved timer
      vt_start->start = counter;
      vt_start->rcount = vtimer->start_field.rcount;
   }

   // Update vtimer total
   if(kind != 1) { 
      //protect against pic rollover
      counter -= vtimer->start_field.start;
      counter &= (uint64_t)0xFFFFFFFF; /* same as 'shl 32; shr 32' */
      vtimer->total_field += counter;
   }
   else {
      if(vtimer->start_field.start < module_load_time) {
         // this is bad, somehow the start time got set to a bad value,
         // equal to before the driver was even loaded
         vstop_stats.bad_start_val += 1;
      }
      else {
         // protect against time rollback
         if(counter > vtimer->start_field.start)
            vtimer->total_field += (counter - vtimer->start_field.start);
      }
   }

   // Reset cpu vtimer start_field
   vtimer->start_field.start = 0;
   vtimer->start_field.rcount = 0;

   return 1; /* add saved timer to curr thr */
}

void generic_vrestart_routine(struct vtimer *vtimer, uint64_t auxdata,
                              unsigned type)
{
   struct vtimer_start *old_start = (struct vtimer_start*)&auxdata;
   uint64_t counter = 0;
   unsigned kind = KIND_OF(type);
   unsigned hwctr = 0;
   
   vrestart_stats.entries_to += 1;

   // Read appropriate hw_counter based on type
   if(kind == 1) { /* HWC_TICKS */
      vrestart_stats.kind_is_tick += 1;
      __asm__ __volatile__ ("rd %%tick, %0"
                            : "=r" (counter)
                            : /* no inputs */);
      if(counter < module_load_time)
	 vrestart_stats.bad_tick_val += 1;

      // Mask counter to 48 bits
      counter <<= 16;
      counter >>= 16;
   }
   else if(kind != 0) {
      hwctr = CTR_OF(type);
      if(hwctr == 0) { /* PIC0 */
         vrestart_stats.kind_is_pic0 += 1;
         // we want low 32 bits
         __asm__ __volatile__ ("rd %%pic, %0\n\t"
                               "srl %0, 0, %0" /* zeroes hi 32 bits */
                               : "=r" (counter)
                               : /* no inputs */);
      }
      else if(hwctr == 1) { /* PIC1 */
         vrestart_stats.kind_is_pic1 += 1;
         // we want high 32 bits
         __asm__ __volatile__ ("rd %%pic, %0\n\t"
                               "srlx %0, 32, %0"
                               : "=r" (counter)
                               : /* no inputs */);
      }
      else { 
         // this shouldn't happen, but we'll be safe
         vrestart_stats.bad_kind += 1;
         vtimer->start_field.start = old_start->start;
         vtimer->start_field.rcount = old_start->rcount;
         return;
      }
   }
   else { // this shouldn't happen, but we'll be safe
      vrestart_stats.bad_kind += 1;
      vtimer->start_field.start = old_start->start;
      vtimer->start_field.rcount = old_start->rcount;
      return;
   }

   vtimer->start_field.start = counter;
   vtimer->start_field.rcount = old_start->rcount;

   // If wall timer, add events switched out to total
   if(IS_WALL_TIMER(type)) {
      if(kind != 1) {
         // protect against counter rollover
         counter -= old_start->start;
         counter &= (uint64_t)0xFFFFFFFF; /* same as 'shl 32; shr 32' */
         vtimer->total_field += counter;
      }
      else {
         if(old_start->start < module_load_time)
            vrestart_stats.bad_start_val += 1;
         else {
            // protect against time rollback
            if(counter > old_start->start)
               vtimer->total_field += (counter - old_start->start);
         }
      }
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
   kthread_t *curr = curthread;
   uint64_t curr_thr = (uint64_t)curr;
   unsigned curr_cpu = (unsigned)(curr->t_cpu->cpu_id);

   int instance = 0; /* May be safer to do a getminor(dev)!!! */
   hwprfunit_t* usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);

   uint32_t old_pil_level;

   if(usp->cswitch_stuff_enabled) {
      
      old_pil_level = grab_cswitch_lock(&usp->kerninst_cswitch_lock);
   
      // From the hash table, get the stored vector-of-timer-ptrs for current
      // thread. If none exists, then we don't need to do anything more

      timers_start = hash_table_remove(&usp->ht_vtimers, curr_thr);

      if (timers_start == NULL) { // probably the common case
         release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
         return;
      }

      timers_iter = timers_start;
      theTimer = (struct vtimer *)((kptr_t)timers_iter->addr);
      auxdata = timers_iter->auxdata;

      while(theTimer != NULL) {
         cpu_vt = getCPUTimer(theTimer, curr_cpu);
         
         // call the restart routine
         generic_vrestart_routine(cpu_vt, auxdata, theTimer->counter_type);

         timers_iter++;
         theTimer = (struct vtimer *)((kptr_t)timers_iter->addr);
         auxdata = timers_iter->auxdata;
      }

      // Return this vector of pointers to the free list
      stacklist_free(&usp->sl_heap, timers_start);

      release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
   }
}

void on_switch_out(void)
{
   int grabbed_freelistentry_yet = 0;

   void **iter, **finish;

   struct saved_timer *vec_of_timers = NULL;
   struct saved_timer *vec_of_timers_iter = NULL;

   kthread_t *curr = curthread;
   uint64_t curr_thr = (uint64_t)curr;
   unsigned curr_cpu = (unsigned)(curr->t_cpu->cpu_id);

   int instance = 0; /* May be safer to do a getminor(dev)!!! */
   hwprfunit_t* usp = (hwprfunit_t *)ddi_get_soft_state(statep, instance);

   uint32_t old_pil_level;

   unsigned add_saved_timer = 0;

   if(usp->cswitch_stuff_enabled) {
      old_pil_level = grab_cswitch_lock(&usp->kerninst_cswitch_lock);

      iter = (void**)usp->av_vtimers.vtimer_addresses;
      finish = (void**)usp->av_vtimers.finish;

      // Loop through all vtimers
      while (iter != finish) {
         struct vtimer *theTimer = (struct vtimer *)*iter++;
         struct vtimer *cpu_vt = getCPUTimer(theTimer, curr_cpu);
         // read start field to get the state
         if(cpu_vt->start_field.rcount != 0) { // timer started
            if (!grabbed_freelistentry_yet) {
               vec_of_timers = (struct saved_timer*)stacklist_get(&usp->sl_heap);
               if(vec_of_timers != NULL) {
                  vec_of_timers_iter = vec_of_timers;
		  grabbed_freelistentry_yet = 1;
	       }
	       else { // stacklist is full, can't save any more timers
		  usp->num_unsaved_timers++;
	       }
            }
            // Call stop_routine, which updates saved_timer.auxdata, 
            // vtimer.total_field, and vtimer.start_field
            add_saved_timer = generic_vstop_routine(cpu_vt, 
						    vec_of_timers_iter, 
						    theTimer->counter_type);
            
            if((vec_of_timers != NULL) && add_saved_timer) { 
	       // Add to working list:
	       (struct vtimer*)(vec_of_timers_iter->addr) = theTimer;
               vec_of_timers_iter++;
	    }
         }
      }
      if (grabbed_freelistentry_yet) { // was anything done?
         // Mark the end of the list
         (struct vtimer*)(vec_of_timers_iter->addr) = NULL;

         // Add to the hash table!
         // What to set the key to? id of current thread
         hash_table_add(&usp->ht_vtimers, 
                        vec_of_timers, // not vec_of_timers_iter!
                        curr_thr);
      }
      release_cswitch_lock(old_pil_level, &usp->kerninst_cswitch_lock);
   }
}
