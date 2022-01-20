// QuickBranchLauncher.h
// Launching to a patch via a ba,a or ba,a,pt instruction.

#ifndef _QUICK_BRANCH_LAUNCHER_H_
#define _QUICK_BRANCH_LAUNCHER_H_

#include "launcher.h"
#include "instr.h"
#include "kernelDriver.h"

class QuickBranchLauncher : public launcher {
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

   QuickBranchLauncher(const QuickBranchLauncher &);
   QuickBranchLauncher &operator=(const QuickBranchLauncher &);
   
 public:
   QuickBranchLauncher(kptr_t ifrom, kptr_t ito,
                       const instr_t *iwhenAllDoneRestoreToThisInsn);
  ~QuickBranchLauncher() {} // calls base class dtor automatically

   instr_t* createSplicingInsn() const;
};

#endif
