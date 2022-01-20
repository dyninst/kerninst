// sparc_QuickBranchLauncher.C

#include "QuickBranchLauncher.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "sparc_instr.h"

extern void flush_a_bunch(kptr_t, unsigned nbytes);
   // from directToMemoryEmitter.C

QuickBranchLauncher::
QuickBranchLauncher(kptr_t ifrom, kptr_t ito,
                    const instr_t *iwhenAllDoneRestoreToThisInsn) :
      inherited(ifrom, ito, iwhenAllDoneRestoreToThisInsn)
{
   assert(sparc_instr::inRangeOfBicc(from, to));
}

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
   
   const instr_t *branch = createSplicingInsn();

   // Call /dev/kerninst to poke address "from" with value "branch.getRaw()"
   // Make it an "undoable" poke, so that if we die without cleanup up, /dev/kerninst
   // will automatically clean up for us (by "undo-ing the poke").

   theKernelDriver.poke1WordUndoablePush(from,
                                         whenAllDoneRestoreToThisInsn->getRaw(),
                                            // expect to see this...
                                         branch->getRaw(),
                                            // change it to this...
                                         0);
      // 0 --> undo me first (undo springboards later)
      // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, 4);

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "QuickBranchLauncher [not overwriting existing so used undoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
   delete branch;
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
   
   const instr_t* branch = createSplicingInsn();

   // Overwriting an existing launcher --> use changePoke1WordUndoable()

   theKernelDriver.poke1WordUndoableChangeTopOfStack(from,
                                                     otherGuysCurrInstalledInsn->getRaw(),
                                                        // current contents of memory
                                                     branch->getRaw(), 0);
      // 0 --> undo me first (undo springboards later)
      // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, 4);

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "QuickBranchLauncher [overwriting existing so used changeUndoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
   delete branch;
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

      theKernelDriver.undoPoke1WordPop(from,
                                       createSplicingInsn()->getRaw(),
                                          // expect to see this
                                       whenAllDoneRestoreToThisInsn->getRaw()
                                          // change back to this
                                       );
   
      // flush original instruction back into the icache!
      flush_a_bunch(from, 4);
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
