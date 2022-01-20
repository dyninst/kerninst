// loop.h
// kerninstd's main loop

#ifndef _LOOP_H_
#define _LOOP_H_

#include "internalEvent.h"

void doOneTimeInternalEventInTheFuture(const internalEvent *e);
void kerninstd_mainloop();

#endif
