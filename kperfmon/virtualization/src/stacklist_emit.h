// stacklist_emit.h

#ifndef _STACKLIST_EMIT_H_
#define _STACKLIST_EMIT_H_

#include "tempBufferEmitter.h"

void emit_stacklist_initialize(tempBufferEmitter &,
                               kptr_t stacklist_kernelAddr,
                               dptr_t stacklist_kerninstdAddr);
   // both kernel and kerninstd addresses are needed; the latter because
   // it's going to be kerninstd who does the initialization and it needs
   // to know where to write...the former so it knows what values to write.
   // A subtle distinction: kerninstd addresses are written, but kernel
   // addresses are the "source value" for such writes.

void emit_stacklist_get(tempBufferEmitter &get_em);
   // NOTE: Writes to condition codes without saving them.  This should be OK in
   // practice.

void emit_stacklist_free(tempBufferEmitter &free_em);
   // NOTE: Writes to condition codes without saving them.  This should be OK in
   // practice.

#endif
