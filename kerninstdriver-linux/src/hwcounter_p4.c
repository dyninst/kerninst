// hwcounter_p4.c - definitions to represent Pentium4/Xeon hardware counters

/* See "IA-32 Intel® Architecture Software Developer's Manual Volume 3: 
   System Programming Guide (Order #253668)" for more information */

#include "hwcounter_p4.h"
#include "vtimer.h"
#include "kernInstIoctls.h"
#include "kernInstBits.h"
#include <asm/current.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>

#ifndef i386_unknown_linux2_4
#error file is x86/linux specific
#endif

extern uint32_t kerninst_enable_bits;

//--------- Counters & Counter Configuration Control Register (CCCRs) ---------

const char* counter_names[NUM_COUNTERS] = {
   "BPU_COUNTER0",
   "BPU_COUNTER1",
   "BPU_COUNTER2",
   "BPU_COUNTER3",
   "MS_COUNTER0",
   "MS_COUNTER1",
   "MS_COUNTER2",
   "MS_COUNTER3",
   "FLAME_COUNTER0",
   "FLAME_COUNTER1",
   "FLAME_COUNTER2",
   "FLAME_COUNTER3",
   "IQ_COUNTER0",
   "IQ_COUNTER1",
   "IQ_COUNTER2",
   "IQ_COUNTER3",
   "IQ_COUNTER4",
   "IQ_COUNTER5"
};

const int counter_escrs[NUM_COUNTERS][8] = {
   {BPU_ESCR0, IS_ESCR0, MOB_ESCR0, ITLB_ESCR0, PMH_ESCR0, IX_ESCR0, FSB_ESCR0, BSU_ESCR0},
   {BPU_ESCR0, IS_ESCR0, MOB_ESCR0, ITLB_ESCR0, PMH_ESCR0, IX_ESCR0, FSB_ESCR0, BSU_ESCR0},
   {BPU_ESCR1, IS_ESCR1, MOB_ESCR1, ITLB_ESCR1, PMH_ESCR1, IX_ESCR1, FSB_ESCR1, BSU_ESCR1},
   {BPU_ESCR1, IS_ESCR1, MOB_ESCR1, ITLB_ESCR1, PMH_ESCR1, IX_ESCR1, FSB_ESCR1, BSU_ESCR1},
   {MS_ESCR0, TC_ESCR0, TBPU_ESCR0, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR},
   {MS_ESCR0, TC_ESCR0, TBPU_ESCR0, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR},
   {MS_ESCR1, TC_ESCR1, TBPU_ESCR1, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR},
   {MS_ESCR1, TC_ESCR1, TBPU_ESCR1, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR, NO_ESCR},
   {FLAME_ESCR0, FIRM_ESCR0, SAAT_ESCR0, U2L_ESCR0, NO_ESCR, DAC_ESCR0, NO_ESCR, NO_ESCR},
   {FLAME_ESCR0, FIRM_ESCR0, SAAT_ESCR0, U2L_ESCR0, NO_ESCR, DAC_ESCR0, NO_ESCR, NO_ESCR},
   {FLAME_ESCR1, FIRM_ESCR1, SAAT_ESCR1, U2L_ESCR1, NO_ESCR, DAC_ESCR1, NO_ESCR, NO_ESCR},
   {FLAME_ESCR1, FIRM_ESCR1, SAAT_ESCR1, U2L_ESCR1, NO_ESCR, DAC_ESCR1, NO_ESCR, NO_ESCR},
   {IQ_ESCR0, ALF_ESCR0, RAT_ESCR0, SSU_ESCR0, CRU_ESCR0, CRU_ESCR2, CRU_ESCR4, NO_ESCR},
   {IQ_ESCR0, ALF_ESCR0, RAT_ESCR0, SSU_ESCR0, CRU_ESCR0, CRU_ESCR2, CRU_ESCR4, NO_ESCR},
   {IQ_ESCR1, ALF_ESCR1, RAT_ESCR1, NO_ESCR, CRU_ESCR1, CRU_ESCR3, CRU_ESCR5, NO_ESCR},
   {IQ_ESCR1, ALF_ESCR1, RAT_ESCR1, NO_ESCR, CRU_ESCR1, CRU_ESCR3, CRU_ESCR5, NO_ESCR},
   {IQ_ESCR0, ALF_ESCR0, RAT_ESCR0, SSU_ESCR0, CRU_ESCR0, CRU_ESCR2, CRU_ESCR4, NO_ESCR},
   {IQ_ESCR1, ALF_ESCR1, RAT_ESCR1, NO_ESCR, CRU_ESCR1, CRU_ESCR3, CRU_ESCR5, NO_ESCR}
};

//------------------ Current Performance Counters State ------------------

extern uint32_t kerninst_debug_bits;

/* retrive the performance counter state on the current cpu, should be the
   same across all cpus due to our management */
void retrieve_machine_state(perfctr_state_t *ps)
{
  int i;
  unsigned msr_address;
  unsigned cccr_low = 0;
  for(i=0; i<NUM_COUNTERS; i++) {
    msr_address = CCCR_ADDR(i);
    __asm__ __volatile__("rdmsr"
			 : "=a" (cccr_low)
			 : "c" (msr_address));
    if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS)
       printk(KERN_INFO "        : - %s_CCCR = 0x%08x\n",
              counter_names[i], cccr_low);
    ps->cccr_val[i] = cccr_low;
    if(CCCR_IS_ENABLED(cccr_low)) {
       msr_address = ESCR_ADDR(counter_escrs[i][ESCR_OF_CCCR(cccr_low)]);
       __asm__ __volatile__("rdmsr"
                            : "=a" (ps->escr_val[i])
                            : "c" (msr_address));
    }
    else
       ps->escr_val[i] = 0;
  }
  msr_address = IA32_PEBS_ENABLE;
  __asm__ __volatile__("rdmsr"
                       : "=a" (ps->pebs_msr[0])
                       : "c" (msr_address));
  msr_address = PEBS_MATRIX_VERT;
  __asm__ __volatile__("rdmsr"
                       : "=a" (ps->pebs_msr[1])
                       : "c" (msr_address));
}

void set_machine_state(perfctr_state_t *ps)
{
   int i;
   unsigned msr_address, msr_low;

   for(i=0; i<NUM_COUNTERS; i++) {
      msr_low = ps->cccr_val[i];
      msr_address = CCCR_ADDR(i);
      if(msr_low != (unsigned)-1) {
         __asm__ __volatile__("wrmsr"
                              : /* no outputs */
                              : "a" (msr_low), 
                                "c" (msr_address));
         if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS) {
            printk(KERN_INFO "        : - set %s_CCCR = 0x%08x\n",
                   counter_names[i], ps->cccr_val[i]);
         }
         if(CCCR_IS_ENABLED(ps->cccr_val[i])) {
            msr_address = ESCR_ADDR(counter_escrs[i][ESCR_OF_CCCR(ps->cccr_val[i])]);
            msr_low = ps->escr_val[i];
            __asm__ __volatile__("wrmsr"
                                 : /* no outputs */
                                 : "a" (msr_low), 
                                 "c" (msr_address));
            if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS) {
               printk(KERN_INFO "        : - set ESCR_0x%04x = 0x%08x\n",
                      msr_address, ps->escr_val[i]);
            }
         }
      }
   }
   if(ps->pebs_msr[0] != (unsigned)-1) {
      msr_address = IA32_PEBS_ENABLE;
      msr_low = ps->pebs_msr[0];
      __asm__ __volatile__("wrmsr"
                           : /* no outputs */
                           : "a" (msr_low), "c" (msr_address));
      if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS) {
         printk(KERN_INFO "        : - set IA32_PEBS_ENABLE = 0x%08x\n",
                msr_low);
      }
      msr_address = PEBS_MATRIX_VERT;
      msr_low = ps->pebs_msr[1];
      __asm__ __volatile__("wrmsr"
                           : /* no outputs */
                           : "a" (msr_low), "c" (msr_address));
      if(kerninst_debug_bits & KERNINST_DEBUG_P4COUNTERS) {
         printk(KERN_INFO "        : - set PEBS_MATRIX_VERT = 0x%08x\n",
                msr_low);
      }
   }
}

void get_perfctr_value(perfctr_val_t *val)
{
   unsigned tmp_low = 0;
   unsigned tmp_hi = 0;
   __asm__ __volatile__("rdmsr"
                        : "=a" (tmp_low), "=d" (tmp_hi)
                        : "c" (val->ctr));
   val->low = tmp_low;
   val->high = tmp_hi;
}

void set_perfctr_value(perfctr_val_t *val)
{
   unsigned tmp_low = val->low;
   unsigned tmp_hi = val->high;
   __asm__ __volatile__("wrmsr"
                        : /* no outputs */
                        : "a" (tmp_low), "d" (tmp_hi), "c" (val->ctr));
}

void read_hwc_all_cpus(void *args)
{
   struct readHwcArgs *real_args = (struct readHwcArgs *)args;
   unsigned pmc_low = 0, pmc_high = 0;
   uint64_t val = 0;
   unsigned ctr_addr = 0;

   unsigned cpu_ndx = (unsigned)smp_processor_id();

   unsigned kind = KIND_OF(real_args->type);
   if(kind == 1) { /* HWC_TICKS */
      rdtsc(pmc_low, pmc_high);
   }
   else if((kind != 0) &&
           (kerninst_enable_bits & KERNINST_ENABLE_P4COUNTERS)) {
      ctr_addr = COUNTER_ADDR(CTR_OF(real_args->type));
      __asm__ __volatile__ ("rdmsr"
                            : "=a" (pmc_low), "=d" (pmc_high)
                            : "c" (ctr_addr));
      pmc_high &= 0x000000FFUL; // 40-bit counters, only 8 bits in high
   }
   val = pmc_high;
   val <<= 32;
   val += pmc_low;
   real_args->cpu_vals[cpu_ndx] = val;
}
