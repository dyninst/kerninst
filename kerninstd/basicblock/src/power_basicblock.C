// power_basicblock.C - implementation of arch-specific basicblock members
//                      for POWER

#include "power_basicblock.h"

power_basicblock::~power_basicblock() {
}

kptr_t power_basicblock::getExitPoint(const fnCode_t & /* fnCode */) const {
   //On Power, no delay slots exist, hence only the last instruction
   //might be controlflow instruction.  Since post-instrumentation
   //is not available, we can always simply instrument before 
   //last instruction.  Seems a bit too simple, compare to
   //sparc getExitPoint() -Igor.

 
   const kptr_t lastInsnAddr = startAddr + numInsnBytesInBB - 4;
   return lastInsnAddr;

}
