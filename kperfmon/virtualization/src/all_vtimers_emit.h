// all_vtimers_emit.h

#ifndef _ALL_VTIMERS_EMIT_H_
#define _ALL_VTIMERS_EMIT_H_

#include "all_vtimers.h"
#include "tempBufferEmitter.h"

void emit_all_vtimers_initialize(tempBufferEmitter &em,
                                 dptr_t all_vtimers_kerninstdAddr);
void emit_all_vtimers_destroy(tempBufferEmitter &em);
void emit_all_vtimers_add(tempBufferEmitter &em);
void emit_all_vtimers_remove(tempBufferEmitter &em);

#endif
