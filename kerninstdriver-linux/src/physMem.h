// physMem.h

#ifndef _KERNINST_PHYSICAL_MEM_
#define _KERNINST_PHYSICAL_MEM_

#include "loggedWrites.h"

int write1Insn(kptr_t addr, char *val, unsigned length);

int write1UndoableInsn(loggedWrites *iLoggedWrites, kptr_t addr,
                       char *val, unsigned length, unsigned howSoon,
                       char *oldval_fillin);

int undoWrite1Insn(loggedWrites *iLoggedWrites, kptr_t addr,
                   unsigned length, char *oldval_to_set_to);

int changeUndoableInsnHowSoon(loggedWrites *iLoggedWrites, kptr_t addr,
                              unsigned howSoon);

#endif
