/*
 * kernInstBits.h - defines used to manipulate debug and enable bitfields
 */

#ifndef _KERNINST_LINUX_BITS_
#define _KERNINST_LINUX_BITS_

#define KERNINST_DEBUG_ALL 0x0000ffff
#define KERNINST_DEBUG_MEMORY  1<<0  /* debug memory (de)allocations */
#define KERNINST_DEBUG_SYMTAB  1<<1  /* debug symbol table */
#define KERNINST_DEBUG_CALLEES 1<<2  /* debug callee watching */
#define KERNINST_DEBUG_TICK    1<<3  /* debug tick */
#define KERNINST_DEBUG_VTIMERS 1<<4  /* debug virtual timers */
#define KERNINST_DEBUG_IOCTLS  1<<15 /* debug calls to kerninst_ioctl */
/* NOTE: don't define cross-platform values greater than 1<<15, since the top
   16 bits will be used for platform-dependent bits */

#define KERNINST_ENABLE_ALL 0x0000ffff
#define KERNINST_ENABLE_CONTEXTSWITCH 1<<0 /* enable context switch handling */
#define KERNINST_ENABLE_UNDOMEMORY    1<<1 /* enable undoable memory ops */
/* NOTE: don't define cross-platform values greater than 1<<15, since the top
   16 bits will be used for platform-dependent bits */

#ifdef i386_unknown_linux2_4
#define KERNINST_DEBUG_P4COUNTERS  1<<16
#define KERNINST_ENABLE_P4COUNTERS 1<<16 /* Don't let users manipulate this, 
                                            it's really an internal flag */
#define KERNINST_DEBUG_TRAPS  1<<17
/* no KERNINST_ENABLE_TRAPS since not yet necessary */
#endif
#ifdef ppc64_unknown_linux2_4
#define KERNINST_ENABLE_POWER4COUNTERS 1<<16
#endif
#endif // _KERNINST_LINUX_BITS_
