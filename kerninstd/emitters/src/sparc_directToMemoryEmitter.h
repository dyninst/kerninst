// sparc_directToMemoryEmitter.h

#ifndef _SPARC_DIRECT_TO_MEMORY_EMITTER_H_
#define _SPARC_DIRECT_TO_MEMORY_EMITTER_H_

#include "directToMemoryEmitter.h"

class sparc_directToMemoryEmitter: public directToMemoryEmitter_t {
 public:
   sparc_directToMemoryEmitter(const abi &iABI,
                               dptr_t whereToBeginEmittingToInKerninstd,
                               kptr_t whereToBeginEmittingToInKernel,
                               // if NULL then getStartEmitAddrInKernel()
                               // and getCurrentEmitAddrInKernel() will assert fail
                               unsigned itotalNumInsnBytesOKToWrite) :
      directToMemoryEmitter_t (iABI, whereToBeginEmittingToInKerninstd,
                               whereToBeginEmittingToInKernel, itotalNumInsnBytesOKToWrite) { }
 
   void complete ();
   void emitSave(bool saveNeeded, const regset_t *regsToSave);
   void emitRestore(bool SaveNeeded, const regset_t *regsToRestore);
};
   

#endif
