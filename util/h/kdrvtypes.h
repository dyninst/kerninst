#ifndef _KDRVTYPES_H_
#define _KDRVTYPES_H_

#include <inttypes.h>

/* 
   This file is included both in the driver and in the daemon

   Kernel addresses occupy 64 bits on sparcv9. The 32-bit daemon should
   handle them properly -- compile it with -D_KDRV64_ for v9.
*/
#if defined(_KDRV64_)
typedef uint64_t kptr_t;
typedef int64_t kaddrdiff_t;
#elif defined(_KDRV32_)
typedef uintptr_t kptr_t;
typedef intptr_t kaddrdiff_t;
#else
#error one of _KDRV64_ or _KDRV32_ must be defined
#endif

/* 
   Daemon addresses occupy 32 bits for now
*/
typedef uint32_t dptr_t;
typedef int32_t daddrdiff_t;

#endif
