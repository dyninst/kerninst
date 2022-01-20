// cswitchout_emit.h

#ifndef _CSWITCH_OUT_EMIT_H_
#define _CSWITCH_OUT_EMIT_H_

#include "tempBufferEmitter.h"
#include "traceBuffer.h"
#include "fnRegInfo.h"

std::pair<fnRegInfo, fnRegInfo> // for how-to-stop routines, hash table add, respectively
emit_cswitchout_instrumentation(tempBufferEmitter &em,
                                bool just_in_kerninstd,
                                const fnRegInfo &theFnRegInfo,
                                   // params [none], avail regs, result reg [none]
                                traceBuffer *theTraceBuffer, // if NULL no tracing
                                unsigned traceop_begin, // undef'd if not tracing
                                unsigned traceop_finish, // undef'd if not tracing
                                kptr_t allvtimers_kernelAddr,
                                kptr_t stacklist_kernelAddr,
                                kptr_t stacklist_getfn_kernelAddr,
                                kptr_t stacklist_freefn_kernelAddr);

#endif
