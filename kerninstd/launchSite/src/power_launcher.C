// power_launcher.C

#include "launcher.h"
#include "QuickBranchLauncher.h"
#include "SpringboardLauncher.h"
#include "power_instr.h"

instr_t* launcher::makeQuickBranch(kptr_t fromaddr,
				   kptr_t toaddr,
				   bool /* annulled */) {
  assert(power_instr::inRangeOfBranch(fromaddr, toaddr));
   kaddrdiff_t jump_distance = toaddr - fromaddr;
   return new power_instr(power_instr::branch, jump_distance, 0 /*not absolute */, 0 /* no link */ );
}

launcher* launcher::pickAnAnnulledLauncher(kptr_t from, kptr_t to,
                                           const instr_t *whenAllDoneRestoreToThisInsn,
                                           SpringBoardHeap &theSpringBoardHeap) {
   // A static method.

  assert(power_instr::inRangeOfBranch(from, to));

  //Always go with QuickBranchLauncher for now.  Springboard will not be needed, I hope.
  return new QuickBranchLauncher(from, to, whenAllDoneRestoreToThisInsn);
  
}

void launcher::
installOverwritingExisting(bool justTesting,
			   const instr_t *otherGuysCurrInstalledInsn,
			      // needed for asserting when we poke
			   const instr_t *otherGuysRestoreToInsn,
			      // we assert equals our whenAllDoneRestoreToInsn,
			      // or we weren't created right.
			   const regset_t *freeRegs,
			   kernelDriver &theKernelDriver,
			   bool verboseTimings) {
   if (!installed) {
      assert(*(const power_instr*)otherGuysRestoreToInsn == *(const power_instr*)whenAllDoneRestoreToThisInsn);
         
      doInstallOverwritingExisting(justTesting,
				   otherGuysCurrInstalledInsn,
				   freeRegs, theKernelDriver,
				   verboseTimings);
      installed = true;
   }
}
