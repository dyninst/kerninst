// vtimer.h

#ifndef _VTIMER_H_
#define _VTIMER_H_

#if (defined _KERNEL_) || (defined __KERNEL__)
#include <linux/types.h>
#else
#include <inttypes.h>
#endif

#ifdef ppc64_unknown_linux2_4
struct vtimer_start {
   unsigned rcount : 16; // recursion count.  if 0, timer is off.
   unsigned long long start : 48;
};
#else
struct vtimer_start {
   unsigned long long start : 48;
   unsigned rcount : 16; // recursion count.  if 0, timer is off.
};
#endif
struct vtimer {
   // Current timer total
   uint64_t total_field;

   // Current timer start and recursion count
   struct vtimer_start start_field; // 64 bits

   // vstop & vrestart routines (not currently used)
   void *stop_routine;
#if defined(ppc64_unknown_linux2_4) && !defined(__KERNEL__)
//need to make sure kernel and userspace see the same size
   uint32_t pad1;
#endif

   void *restart_routine;
#if defined(ppc64_unknown_linux2_4) && !defined(__KERNEL__)
//need to make sure kernel and userspace see the same size
   uint32_t pad2;
#endif

   // Type: ((PERFCTR_NUM << 16) | [WALL_TIMER] | kapi_hwcounter_kind)
   unsigned counter_type;
};

// offsets from start of vtimer
#define total_field_offset ((unsigned)&((struct vtimer *)0)->total_field)
#define start_field_offset ((unsigned)&((struct vtimer *)0)->start_field)
#define stop_routine_offset ((unsigned)&((struct vtimer *)0)->stop_routine)
#define restart_routine_offset ((unsigned)&((struct vtimer *)0)->restart_routine)
#define counter_type_offset ((unsigned)&((struct vtimer *)0)->counter_type)
#define sizeof_vtimer (sizeof(struct vtimer))

// macros for extracting various parts of counter_type
#define WALL_TIMER (1 << 15)
#define IS_WALL_TIMER(t) (t & WALL_TIMER)
#define CTR_OF(t) (t >> 16)
#define KIND_OF(t) (t & 0x00007fff)

#endif
