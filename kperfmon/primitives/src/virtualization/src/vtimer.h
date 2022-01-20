// vtimer.h

#ifndef _VTIMER_H_
#define _VTIMER_H_

#include <inttypes.h>
#include "util/h/kdrvtypes.h"

#ifdef sparc_sun_solaris2_7
struct old_vtimer_total {
   unsigned state : 2;
   unsigned long long total : 62;
};

struct old_vtimer {
   struct old_vtimer_total total_field; // 64 bits
   uint64_t start_field;  // 64 bits
   kptr_t stop_routine;

//   void (*stop_routine)(struct old_vtimer *,
//                        void *stopped_vtimers_start_reg,
//                        void *stopped_vtimers_iter_reg);
//   Routine must append [vtimer addr, aux data] *iff* stopping it.
   kptr_t restart_routine;
//   void (*restart_routine)(struct vtimer *,
//                           uint32_t aux_data);
//   Routine will be passed aux_data, and should restart the timer.
};

struct new_vtimer_start {
   unsigned rcount : 16; // recursion count.  if 0, timer is off.
   unsigned long long start : 48;
};

struct vtimer {
   uint64_t total_field;
   struct new_vtimer_start start_field; // 64 bits
   kptr_t stop_routine;	
//   void (*stop_routine)(struct new_vtimer *,
//                        void *stopped_vtimers_start_reg,
//                        void *stopped_vtimers_iter_reg);
//   Routine must append [vtimer addr, aux data] *iff* stopping it.
   kptr_t restart_routine;
//   void (*restart_routine)(struct vtimer *,
//                           uint32_t aux_data);
//   Routine will be passed aux_data, and should restart the timer.
};

#define total_field_offset (0)
#define start_field_offset (sizeof(uint64_t))
#define stop_routine_offset (2*sizeof(uint64_t))
#define restart_routine_offset (2*sizeof(uint64_t) + sizeof(kptr_t))
#define sizeof_vtimer (2*sizeof(uint64_t) + 2*sizeof(kptr_t))

#endif // sparc_sun_solaris2_7

#endif //_VTIMER_H_
