// cswitchin_emit.h

#ifndef _CSWITCH_IN_EMIT_H_
#define _CSWITCH_IN_EMIT_H_

#include "util/h/kdrvtypes.h"
#include "tempBufferEmitter.h"
#include "traceBuffer.h"
#include "fnRegInfo.h"

std::pair<fnRegInfo, fnRegInfo> // for hash table remove, how-to-restart, respectively
emit_cswitchin_instrumentation(tempBufferEmitter &em,
                               bool just_kerninstd,
                               const fnRegInfo &theFnRegInfo,
                                   // params [none], avail regs, result reg [none]
                               traceBuffer *theTraceBuffer, // can be NULL
                               unsigned traceop_begin, // undef'd if not tracing
                               unsigned traceop_end, // undef'd if not tracing
                               kptr_t stacklist_kernelAddr,
                               kptr_t stacklist_freefn_kernelAddr);

#endif
