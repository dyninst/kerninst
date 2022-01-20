// sparc_callLauncher.C

#include "callLauncher.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "sparc_instr.h"

extern void flush_a_bunch(kptr_t addr, unsigned nbytes);
   // from directToMemoryEmitter.C

callLauncher::callLauncher(kptr_t ifrom, kptr_t ito,
			   const instr_t *iwhenAllDoneRestoreToThisInsn) :
   inherited(ifrom, ito, iwhenAllDoneRestoreToThisInsn)
{
   assert(sparc_instr::inRangeOfCall(ifrom, ito));
}

void callLauncher::print_info() const {
   cout << "callLauncher from " << addr2hex(from) << " to " << addr2hex(to) << endl;
}

instr_t* callLauncher::createSplicingInsn() const {
   return new sparc_instr(sparc_instr::callandlink, to, from);
}

// ----------------------------------------------------------------------

void callLauncher::
doInstallNotOverwritingExisting(bool justTesting,
                                const regset_t */*freeRegs*/,
                                kernelDriver &theKernelDriver,
                                bool verboseTimings) {
   if (justTesting) {
      cout << "CallLauncher test (not overwriting existing launcher)."
           << endl
           << "   from=" << addr2hex(from) << " to="
           << addr2hex(to) << endl;
      return;
   }

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   
   const sparc_instr *splicingInsn = (sparc_instr*)createSplicingInsn();

   // Call /dev/kerninst to poke address "from" with value "splicingInsn.getRaw()"
   // Make it an "undoable" poke, so that if we die without cleanup up, /dev/kerninst
   // will automatically clean up for us (by "undo-ing the poke").

   theKernelDriver.poke1WordUndoablePush(from,
                                         whenAllDoneRestoreToThisInsn->getRaw(),
                                            // expect to see this...
                                         splicingInsn->getRaw(),
                                            // change it to this...
                                         0);
      // 0 --> undo me first (undo springboards later)
      // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, 4);

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "callLauncher [not overwriting existing so used undoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
   delete splicingInsn;
}

void callLauncher::
doInstallOverwritingExisting(bool justTesting,
                             const instr_t *otherGuysCurrInstalledInsn,
                                // needed for asserting when we poke
                             const regset_t */*freeRegs*/,
                             kernelDriver &theKernelDriver,
                             bool verboseTimings) {
   if (justTesting) {
      cout << "callLauncher test (overwriting existing launcher)."
           << endl
           << "   from=" << addr2hex(from) << " to="
           << addr2hex(to) << endl;
      return;
   }

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   
   const sparc_instr *splicingInsn = (sparc_instr*)createSplicingInsn();

   // Overwriting an existing launcher --> use changePoke1WordUndoable()

   theKernelDriver.poke1WordUndoableChangeTopOfStack(from,
                                                     otherGuysCurrInstalledInsn->getRaw(),
                                                        // current contents of memory
                                                     splicingInsn->getRaw(), 0);
      // 0 --> undo me first (undo springboards later)
      // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, 4);

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "callLauncher [overwriting existing so used changeUndoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
   delete splicingInsn;
}

// ----------------------------------------------------------------------

void callLauncher::
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
      cout << "callLauncher uninstall took "
           << totalTime / 1000 << " usecs." << endl;
   }
}
                                   
void callLauncher::
doUninstallAnotherAlreadyInstalled(bool /*justTesting*/,
                                   kernelDriver &/*theKernelDriver*/,
                                   bool /*verboseTimings*/) {
   // this is a nop routine, on purpose
}
