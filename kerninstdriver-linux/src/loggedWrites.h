// loggedWrites.h

#ifndef _KERNINST_LOGGED_WRITES_
#define _KERNINST_LOGGED_WRITES_

#include "kernInstIoctls.h" // kptr_t

typedef struct oneWrite {
   kptr_t addr;
   char *origval;
   unsigned length;
   unsigned howsoon;
} oneWrite;

void undo(oneWrite *w, char *oldval_to_set_to);
void undo_blind(oneWrite *w);

typedef struct loggedWrites {
   oneWrite *writes;
   unsigned num_writes;
} loggedWrites;

loggedWrites* initialize_logged_writes(void);
void destroy_logged_writes(loggedWrites *lw);

int LW_performUndoableWrite(loggedWrites *theLoggedWrites, kptr_t addr, 
                            unsigned length, char *val, 
                            char *oldval_fillin, unsigned howSoon);

int LW_undoWrite1Insn(loggedWrites *theLoggedWrites, kptr_t addr, 
                      unsigned length, char *oldval_to_set_to);

int LW_changeUndoableWriteHowSoon(loggedWrites *theLoggedWrites, kptr_t addr, 
                                  unsigned howSoon);
#endif
