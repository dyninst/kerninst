// vtimerSupport.h
// some fns needed by both vtimerMgr.C and vtimerTest.C

#include "util/h/kdrvtypes.h"
#include "tempBufferEmitter.h"
#include "sparc_reg_set.h"

void emit_stop_for_pic0VTime(tempBufferEmitter &stopEm,
                             sparc_reg vtimer_addr32_reg, // an input param
                             sparc_reg vtimerStartReg, // for asserting overflow
                             sparc_reg vtimerIterReg, // you'll write here if stopping
                             const sparc_reg_set &availRegs);

void emit_restart_for_pic0VTime(tempBufferEmitter &restartEm,
                                sparc_reg vtimer_addr32_reg, // an input param
                                sparc_reg vtimer_auxdata_reg, // an input param
                                const sparc_reg_set &availRegs);

// --------------------------------------------------------------------------------

void emit_addNewEntryToAllVTimers(tempBufferEmitter &em,
                                  kptr_t vtimerAddrInKernel,
                                  dptr_t all_vtimers_kerninstdAddr,
                                  dptr_t all_vtimers_add_kerninstdAddr);
