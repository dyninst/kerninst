#ifndef _MR_TIME_H_
#define _MR_TIME_H_
#include <inttypes.h>

typedef int64_t mrtime_t;
/* 
   Return the current time in nanoseconds. MR stands for Medium-Resolution :) 
*/
extern mrtime_t getmrtime();

#endif
