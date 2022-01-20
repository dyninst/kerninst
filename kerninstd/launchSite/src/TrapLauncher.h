// TrapLauncher.h
// Launching to a patch via an OS trap.

#ifndef _TRAP_LAUNCHER_H_
#define _TRAP_LAUNCHER_H_

#include "launcher.h"
#include "instr.h"
#include "kernelDriver.h"

class TrapLauncher : public launcher {
 private:
   typedef launcher inherited;

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
   
   void print_info() const;

   TrapLauncher(const TrapLauncher &);
   TrapLauncher &operator=(const TrapLauncher &);
   
 public:
   TrapLauncher(kptr_t ifrom, kptr_t ito,
                const instr_t *iwhenAllDoneRestoreToThisInsn);
  ~TrapLauncher() {} // calls base class dtor automatically

   instr_t* createSplicingInsn() const;
};

#endif
