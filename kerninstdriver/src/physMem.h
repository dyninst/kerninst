// physMem.h

#ifndef _PHYS_MEM_H_
#define _PHYS_MEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include "util/h/kdrvtypes.h" // kptr_t

int write1AlignedWordToNucleus(kptr_t addr, uint32_t val);
// ret val: 0 if OK

int write1AlignedWordToNonNucleus(kptr_t addr, uint32_t val);
// ret val: 0 if OK

int write1UndoableAlignedWord(void *iLoggedWrites,
                              kptr_t addr, int maybeNucleus,
                              uint32_t val, unsigned howSoon,
                              uint32_t *oldval_fillin);
   
// explanation of 'howSoon':  When it comes time to undo, we can't undo everything
// at once.  There needs to be an order, when the kernel has been instrumented.
// We want to undo launchers first, then springboards, and lastly, code patches.
// Launchers get a 'howSoon' of zero.  springboards get a 'howSoon' of 1.
// Other things get a 'howSoon' of 2.

int undoWriteWordNucleus(void *iLoggedWrites,
                         kptr_t addr,
                         uint32_t oldval_to_set_to);
   // should cancel the effect of a write1UndoableAlignedWordMaybeNucleus

int undoWriteWordNonNucleus(void *iLoggedWrites,
                            kptr_t addr,
                            uint32_t oldval_to_set_to);
   // should cancel the effect of a write1UndoableAlignedWordNonNucleus

void changeUndoWriteWordHowSoon(void *iLoggedWrites,
                                kptr_t kernelAddr,
                                unsigned newHowSoon);

#ifdef __cplusplus
}
#endif

#endif
