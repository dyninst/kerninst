// x86_directToMemoryEmitter.h

#ifndef _X86_DIRECT_TO_MEMORY_EMITTER_H_
#define _X86_DIRECT_TO_MEMORY_EMITTER_H_

#include "directToMemoryEmitter.h"

class x86_directToMemoryEmitter: public directToMemoryEmitter_t {
 public:
   x86_directToMemoryEmitter(const abi &iABI,
                             dptr_t whereToBeginEmittingToInKerninstd,
                             kptr_t whereToBeginEmittingToInKernel,
                                // if NULL then getStartEmitAddrInKernel()
                                // and getCurrentEmitAddrInKernel() will assert fail
                             unsigned itotalNumInsnBytesOKToWrite) :
      directToMemoryEmitter_t(iABI, whereToBeginEmittingToInKerninstd,
                              whereToBeginEmittingToInKernel, 
                              itotalNumInsnBytesOKToWrite) {}
 
   void complete();

   void emitSave(bool saveNeeded, const regset_t *regsToSave);
   void emitRestore(bool saveNeeeded, const regset_t *regsToRestore);
};
   

#endif
