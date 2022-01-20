// power_CondRegClearLauncher.h
// "Fake" launcher.  Overwrites with a crclr (crxor bx,bx,bx) instruction
#ifndef _POWER_CONDREGCLEAR_LAUNCHER_H_
#define _POWER_CONDREGCLEAR_LAUNCHER_H_

#include "launcher.h"
#include "instr.h"
#include "kernelDriver.h"

class power_CondRegClearLauncher : public launcher {
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

   power_CondRegClearLauncher(const power_CondRegClearLauncher &);
   power_CondRegClearLauncher &operator=(const power_CondRegClearLauncher &);
   
 public:
   power_CondRegClearLauncher(kptr_t ifrom, kptr_t ito,
                       const instr_t *iwhenAllDoneRestoreToThisInsn);
  ~power_CondRegClearLauncher() {} // calls base class dtor automatically

   instr_t* createSplicingInsn() const;
};

#endif
