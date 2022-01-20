#ifndef _GLOBAL_LOCK_H_
#define _GLOBAL_LOCK_H_

#include "sparc_reg.h"
#include "tempBufferEmitter.h"

extern void create_global_lock();
extern void delete_global_lock();
extern void emit_global_lock(tempBufferEmitter &em, sparc_reg vreg);
extern void emit_global_unlock(tempBufferEmitter &em, sparc_reg vreg);

#endif
