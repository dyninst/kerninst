// mm_map.h
// OS-specific routine get_mm_map()

#ifndef _MM_MAP_H_
#define _MM_MAP_H_

#include <sys/types.h> // caddr_t
extern "C" caddr_t get_mm_map();
   // implemented in an OS-specific file

#endif
