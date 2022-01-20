// sparc_launcher.C

#include "launcher.h"
#include "QuickBranchLauncher.h"
#include "SpringboardLauncher.h"
#include "sparc_instr.h"

instr_t* launcher::makeQuickBranch(kptr_t fromaddr,
				   kptr_t toaddr,
				   bool annulled) {
   if (sparc_instr::inRangeOfBPcc(fromaddr, toaddr)) {
      return new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
			     annulled,
			     true, // predict taken
			     true, // xcc
			     (int32_t)(toaddr - fromaddr));
   }
   else if (sparc_instr::inRangeOfBicc(fromaddr, toaddr)) {
      return new sparc_instr(sparc_instr::bicc, sparc_instr::iccAlways,
			     annulled,
			     (int32_t)(toaddr - fromaddr));
   }
   else
      assert(false && "launcher::makeQuickBranch() not in range of a Bicc insn");
}

launcher* launcher::pickAnAnnulledLauncher(kptr_t from, kptr_t to,
                                           const instr_t *whenAllDoneRestoreToThisInsn,
                                           SpringBoardHeap &theSpringBoardHeap) {
   // A static method.

   // If in range, then go with QuickBranchLauncher
   if (sparc_instr::inRangeOfBicc(from, to))
      return new QuickBranchLauncher(from, to, whenAllDoneRestoreToThisInsn);
   else
      return new SpringboardLauncher(from, to, whenAllDoneRestoreToThisInsn,
                                     theSpringBoardHeap);
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
      assert(*(sparc_instr*)otherGuysRestoreToInsn == *(sparc_instr*)whenAllDoneRestoreToThisInsn);
         
      doInstallOverwritingExisting(justTesting,
				   otherGuysCurrInstalledInsn,
				   freeRegs, theKernelDriver,
				   verboseTimings);
      installed = true;
   }
}
