// x86_launcher.C

#include "launcher.h"
#include "callLauncher.h"
#include "QuickBranchLauncher.h"
#include "TrapLauncher.h"
#include "x86_instr.h"

instr_t* launcher::makeQuickBranch(kptr_t fromaddr,
				   kptr_t toaddr,
				   bool /*annulled*/) {
   return new x86_instr(x86_instr::jmp, (int32_t)(toaddr - (fromaddr+5)));
}

launcher* launcher::
pickAnAnnulledLauncher(kptr_t from, kptr_t to,
                       const instr_t *whenAllDoneRestoreToThisInsn,
                       SpringBoardHeap &/*theSpringBoardHeap*/) {
   // A static method.
   if(whenAllDoneRestoreToThisInsn->getNumBytes() >= 5)
      return new QuickBranchLauncher(from, to, whenAllDoneRestoreToThisInsn);
   else
      return new TrapLauncher(from, to, whenAllDoneRestoreToThisInsn);
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
      assert(*(const x86_instr*)otherGuysRestoreToInsn == *(const x86_instr*)whenAllDoneRestoreToThisInsn);
         
      doInstallOverwritingExisting(justTesting,
				   otherGuysCurrInstalledInsn,
				   freeRegs, theKernelDriver,
				   verboseTimings);
      installed = true;
   }
}
