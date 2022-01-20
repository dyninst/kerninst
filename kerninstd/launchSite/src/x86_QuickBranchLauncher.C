// x86_QuickBranchLauncher.C

#include "QuickBranchLauncher.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "x86_instr.h"

extern void flush_a_bunch(kptr_t, unsigned nbytes);
   // from directToMemoryEmitter.C

QuickBranchLauncher::
QuickBranchLauncher(kptr_t ifrom, kptr_t ito,
                    const instr_t *iwhenAllDoneRestoreToThisInsn) :
      inherited(ifrom, ito, iwhenAllDoneRestoreToThisInsn)
{}

void QuickBranchLauncher::print_info() const {
   cout << "QuickBranch.  from " << addr2hex(from) << " to " << addr2hex(to) << endl;
}

instr_t* QuickBranchLauncher::createSplicingInsn() const {
   return makeQuickBranch(from, to, true); // true --> annulled
}

void QuickBranchLauncher::
doInstallNotOverwritingExisting(bool justTesting,
                                const regset_t */*freeRegs*/,
                                kernelDriver &theKernelDriver,
                                bool verboseTimings) {
   if (justTesting) {
      cout << "QuickBranchLauncher test (not overwriting existing launcher)."
           << endl
           << "   from=" << addr2hex(from) << " to="
           << addr2hex(to) << endl;
      return;
   }

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   
   const x86_instr *branch = (x86_instr*)createSplicingInsn();
   unsigned length = whenAllDoneRestoreToThisInsn->getNumBytes();
   char *newVal = (char*) malloc(length);
   memcpy(newVal, branch->getRawBytes(), branch->getNumBytes());
   if(branch->getNumBytes() < length) {
      for(unsigned i=branch->getNumBytes(); i<length; i++)
         newVal[i] = (char)0x90; //nop padding
   }

   // Call /dev/kerninst to poke address "from" with value "branch.getRaw()"
   // Make it an "undoable" poke, so that if we die without cleanup up, 
   // /dev/kerninst will automatically clean up for us.

   theKernelDriver.poke1LocUndoablePush(from,
                                        ((const x86_instr*)whenAllDoneRestoreToThisInsn)->getRawBytes(),
                                            // expect to see this...
                                         newVal,
                                            // change it to this...
                                         length,
                                         0);
      // 0 --> undo me first (undo springboards later)
      // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, length);

   delete branch;
   free(newVal);

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "QuickBranchLauncher [not overwriting existing so used undoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
}

void QuickBranchLauncher::
doInstallOverwritingExisting(bool justTesting,
                             const instr_t *otherGuysCurrInstalledInsn,
                                // needed for asserting when we poke
                             const regset_t */*freeRegs*/,
                             kernelDriver &theKernelDriver,
                             bool verboseTimings) {
   if (justTesting) {
      cout << "QuickBranchLauncher test (overwriting existing launcher)."
           << endl
           << "   from=" << addr2hex(from) << " to="
           << addr2hex(to) << endl;
      return;
   }

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   
   // Overwriting an existing launcher
   const x86_instr *branch = (x86_instr*)createSplicingInsn();
   unsigned length = whenAllDoneRestoreToThisInsn->getNumBytes();
   char *newVal = (char*) malloc(length);
   memcpy(newVal, branch->getRawBytes(), branch->getNumBytes());
   if(branch->getNumBytes() < length) {
      for(unsigned i=branch->getNumBytes(); i<length; i++)
         newVal[i] = (char)0x90; //nop padding
   }
   char *oldVal = (char*) malloc(length);
   memcpy(oldVal, 
          ((const x86_instr*)otherGuysCurrInstalledInsn)->getRawBytes(), 
          otherGuysCurrInstalledInsn->getNumBytes());
   if(otherGuysCurrInstalledInsn->getNumBytes() < length) {
      for(unsigned i=otherGuysCurrInstalledInsn->getNumBytes(); i<length; i++)
         oldVal[i] = (char)0x90; //nop padding
   }

   theKernelDriver.poke1LocUndoableChangeTopOfStack(from,
                                                    oldVal,
                                                    newVal,
                                                    0);
       // 0 --> undo me first (undo springboards later)
       // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, length);

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "QuickBranchLauncher [overwriting existing so used changeUndoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
}

// ----------------------------------------------------------------------

void QuickBranchLauncher::
doUninstallNoOtherAlreadyInstalled(bool justTesting,
                                   kernelDriver &theKernelDriver,
                                   bool verboseTimings) {
   // NOTE: can't do any justTesting check here, since the flag can be changed
   // by the user at runtime...we MUST, to be safe, ALWAYS do a true uninstall.
   // [Update on the above comment: well now that we have undoable kernel pokes,
   // we can take the risk that justTesting hasn't changed recently]

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   
   if (!justTesting) {
      // call driver to poke address [from] with the instruction
      // "whenAllDoneRestoreToThisInsn"

      x86_instr *branch = (x86_instr*)createSplicingInsn();
      unsigned length = whenAllDoneRestoreToThisInsn->getNumBytes();
      char *newVal = (char*) malloc(length);
      memcpy(newVal, branch->getRawBytes(), branch->getNumBytes());
      if(branch->getNumBytes() < length) {
         for(unsigned i=branch->getNumBytes(); i<length; i++)
            newVal[i] = (char)0x90; //nop padding
      }
      
      theKernelDriver.undoPoke1LocPop(from,
                                      newVal, // expect to see this
                                      ((const x86_instr*)whenAllDoneRestoreToThisInsn)->getRawBytes()
                                         // change back to this
                                      );
   
      // flush original instruction back into the icache!
      flush_a_bunch(from, length);

      delete branch;
      free(newVal);
   }

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      cout << "QuickBranchLauncher uninstall took "
           << totalTime / 1000 << " usecs." << endl;
   }
}
                                   
void QuickBranchLauncher::
doUninstallAnotherAlreadyInstalled(bool /*justTesting*/,
                                   kernelDriver &/*theKernelDriver*/,
                                   bool /*verboseTimings*/) {
   // this is a nop routine, on purpose
}
