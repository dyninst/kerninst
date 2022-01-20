// SpringboardLauncher.h

#ifndef _SPRINGBOARD_LAUNCHER_H_
#define _SPRINGBOARD_LAUNCHER_H_

#include "launcher.h"
#include "SpringBoardHeap.h"

// We don't derive from QuickBranchLauncher

// Allocation and deallocation of springboard heap items takes place in
// install/uninstall, rather than ctor/dtor.

class SpringboardLauncher : public launcher {
 public:
   class Need1ScratchRegisterInSpringboard {};

 private:
   pdvector<uint32_t> orig_sb_contents;

   SpringBoardHeap &theSpringBoardHeap;
      // we'd prefer not to store this, but it's too ugly to pass into do_install()

   SpringBoardHeap::item theItem; // the springboard addr can be derived from this

   void doInstallCommon(const regset_t *freeRegs,
                        kernelDriver &theKernelDriver,
                        bool verboseTimings);
   
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

 public:
   SpringboardLauncher(kptr_t ifrom, kptr_t ito,
                       const instr_t *iwhenAllDoneRestoreToThisInsn,
                       SpringBoardHeap &);

  ~SpringboardLauncher() {
      // calls base class dtor automatically
   }

   instr_t* createSplicingInsn() const;
};


#endif
