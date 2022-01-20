// callLauncher.h
// This is only used for replaceFunction -- changing a caller's call instruction
// so that a replacement function is called.  It's not used for getting to code
// patches, because of the horrors of the delay slot instruction.

#ifndef _CALL_LAUNCHER_H_
#define _CALL_LAUNCHER_H_

#include "launcher.h"

class callLauncher : public launcher {
 private:
   typedef launcher inherited;
   callLauncher(const callLauncher &);
   callLauncher &operator=(const callLauncher &);

   void print_info() const;

   void doInstallNotOverwritingExisting(bool justTesting,
                                        const regset_t *freeRegs,
                                        kernelDriver &theKernelDriver,
                                        bool verboseTimings);
   void doInstallOverwritingExisting(bool justTesting,
                                     const instr_t *otherGuysCurrInstalledInsn,
                                        // needed for asserting when we poke
                                     const regset_t *freeRegs,
                                     kernelDriver &theKernelDriver,
                                     bool verboseTimings);
   
   void doUninstallNoOtherAlreadyInstalled(bool justTesting,
                                           kernelDriver &theKernelDriver,
                                           bool verboseTimings);

   void doUninstallAnotherAlreadyInstalled(bool justTesting,
                                           kernelDriver &,
                                           bool verboseTimings);

 public:
   callLauncher(kptr_t from,
                   // not the old callee address, but rather the insnAddr
                   // of the call instruction
                kptr_t to,
                const instr_t *iwhenAllDoneRestoreToThisInsn);
  ~callLauncher() {} // calls base dtor automatically

   instr_t* createSplicingInsn() const;
};


#endif
