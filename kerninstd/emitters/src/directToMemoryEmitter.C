// directToMemoryEmitter.C

#ifdef sparc_sun_solaris2_7
#include "sparc_directToMemoryEmitter.h"
#include "sparc_reg_set.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_directToMemoryEmitter.h"
#include "x86_reg_set.h"
#elif defined(rs6000_ibm_aix5_1)
#include "power_directToMemoryEmitter.h"
#include "power_reg_set.h"
#endif

directToMemoryEmitter_t* directToMemoryEmitter_t::
getDirectToMemoryEmitter(const abi& iABI,
                         dptr_t whereToBeginEmittingToInKerninstd,
                         kptr_t whereToBeginEmittingToInKernel,
                         unsigned itotalNumInsnBytesOKToWrite) {
#ifdef sparc_sun_solaris2_7
   return (directToMemoryEmitter_t*)
      (new sparc_directToMemoryEmitter(iABI,
				       whereToBeginEmittingToInKerninstd,
				       whereToBeginEmittingToInKernel,
				       itotalNumInsnBytesOKToWrite));
#elif defined(i386_unknown_linux2_4)
   return (directToMemoryEmitter_t*)
      (new x86_directToMemoryEmitter(iABI,
                                     whereToBeginEmittingToInKerninstd,
                                     whereToBeginEmittingToInKernel,
                                     itotalNumInsnBytesOKToWrite));
#elif defined(ppc64_unknown_linux2_4)
   return (directToMemoryEmitter_t*)
      (new power_directToMemoryEmitter(iABI,
                                       whereToBeginEmittingToInKerninstd,
                                       whereToBeginEmittingToInKernel,
                                       itotalNumInsnBytesOKToWrite));
#endif
}

unsigned directToMemoryEmitter_t::
getNumBytesForEmitSaveRestore(const unsigned numInt32Regs, 
                              const unsigned numInt64Regs) {
#ifdef sparc_sun_solaris2_7
   return 8; //save/restore is 2 instructions
#elif defined(i386_unknown_linux2_4)
   return 4; //pusha,pushf + popf,popa 
#elif defined(ppc64_unknown_linux2_4)
   return 2*4*(numInt32Regs + numInt64Regs); 
   //one instruction per reg  for each of save/restore (hence the factor of 2)
#endif
}

regset_t *directToMemoryEmitter_t::
getSaveRegSet(const regset_t *preSaveRegSet, regset_t **regsToSave, 
              unsigned numRegsNeeded) {
#ifdef sparc_sun_solaris2_7
   return new sparc_reg_set(sparc_reg_set::save,
                            *(const sparc_reg_set*)preSaveRegSet,
                            true);
#elif defined(i386_unknown_linux2_4)
   return new x86_reg_set(x86_reg_set::save); 
#elif defined(ppc64_unknown_linux2_4)
   assert(numRegsNeeded > 0); 
   unsigned numRegsToSave = numRegsNeeded - preSaveRegSet->count();
   unsigned i,numSaved;
   *regsToSave = new power_reg_set(power_reg_set::empty); 
   regset_t *availRegs = new power_reg_set(*preSaveRegSet);

   //19 is the number of nonvolatile registers, i.e. max we can save.
   //We save from r31 down to r14, if necessary. 
   for (i = 0, numSaved=0; (numSaved < numRegsToSave) && (i < 19)  ; i++) {
      power_reg regToSave =  power_reg(power_reg::rawIntReg, 31-i);
      //Note: no need to save if this register is dead already.
      if (!preSaveRegSet->exists(regToSave)) {
         **regsToSave +=  power_reg(power_reg::rawIntReg, 31-i); 
         *availRegs +=  power_reg(power_reg::rawIntReg, 31-i); 
         numSaved++; 
      }
   }
   assert((numSaved == numRegsToSave) && "Could not save all registers");
   return availRegs;
#endif
}
