#include <asm/processor.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/smp.h> // smp_processor_id()
#include <linux/types.h>
#include "vtimer.h"
#include "kernInstIoctls.h"

extern uint32_t kerninst_enable_bits;

#define SPRN_SIAR       780
#define SPRN_SDAR       781
#define SPRN_MMCRA      786
#define SPRN_PMC1       787
#define SPRN_PMC2       788
#define SPRN_PMC3       789
#define SPRN_PMC4       790
#define SPRN_PMC5       791
#define SPRN_PMC6       792
#define SPRN_PMC7       793
#define SPRN_PMC8       794
#define SPRN_MMCR0      795
#define SPRN_MMCR1      798

void set_power_perfctr_state(struct kernInstPowerPerfCtrState *state) {   
   mtspr(SPRN_MMCR0, state->mmcr0); //mmcr0
   mtspr(SPRN_MMCR1, state->mmcr1); //mmcr1
   mtspr(SPRN_MMCRA, state->mmcra); //mmcra
}

void get_power_perfctr_state(struct kernInstPowerPerfCtrState *curr_state) {
   curr_state->mmcr0 = mfspr(SPRN_MMCR0);
   curr_state->mmcr1 = mfspr(SPRN_MMCR1);
   curr_state->mmcra = mfspr(SPRN_MMCRA);
}

inline uint64_t  read_pmc (unsigned int n) {
   switch(n) {
      case 1:
              return mfspr(SPRN_PMC1);
      case 2:
              return mfspr(SPRN_PMC2);
      case 3:
              return mfspr(SPRN_PMC3);
      case 4:
              return mfspr(SPRN_PMC4);
      case 5:
              return mfspr(SPRN_PMC5);
      case 6:
              return mfspr(SPRN_PMC6);
      case 7:
              return mfspr(SPRN_PMC7);
      case 8:
              return mfspr(SPRN_PMC8);
      default:
              return 0;

   }
}

void read_hwc_all_cpus(void *args)
{
   struct readHwcArgs *real_args = (struct readHwcArgs *)args;
   uint64_t counter = 0;

   unsigned cpu_ndx = (unsigned)smp_processor_id();

   unsigned kind = KIND_OF(real_args->type);
   if(kind == 1) /* HWC_TICKS */ {
      counter = mftb(); 
   }  
   else if((kind != 0) &&
           (kerninst_enable_bits & KERNINST_ENABLE_POWER4COUNTERS)) {
      counter = read_pmc(CTR_OF(real_args->type));
   }

   real_args->cpu_vals[cpu_ndx] = counter;   
}
