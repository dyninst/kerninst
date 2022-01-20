// x86_callLauncher.C

#include "callLauncher.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "x86_instr.h"

extern void flush_a_bunch(kptr_t addr, unsigned nbytes);
   // from directToMemoryEmitter.C

callLauncher::callLauncher(kptr_t ifrom, kptr_t ito,
			   const instr_t *iwhenAllDoneRestoreToThisInsn) :
   inherited(ifrom, ito, iwhenAllDoneRestoreToThisInsn)
{}

void callLauncher::print_info() const {
   cout << "callLauncher from " << addr2hex(from) << " to " << addr2hex(to) << endl;
}

instr_t* callLauncher::createSplicingInsn() const {
  return new x86_instr(x86_instr::call, to - (from+5));
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
   
   const x86_instr *splicingInsn = (x86_instr*)createSplicingInsn();

   // Call /dev/kerninst to poke address "from" with value "splicingInsn.getRaw()"
   // Make it an "undoable" poke, so that if we die without cleanup up, /dev/kerninst
   // will automatically clean up for us (by "undo-ing the poke").
   
   theKernelDriver.poke1LocUndoablePush(from,
                                        ((const x86_instr*)whenAllDoneRestoreToThisInsn)->getRawBytes(),
                                           // expect to see this...
                                        ((const x86_instr*)splicingInsn)->getRawBytes(),
                                           // change it to this...
                                        splicingInsn->getNumBytes(),
                                        0);
       // 0 --> undo me first (undo springboards later)
       // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, splicingInsn->getNumBytes());

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
   
   const x86_instr *splicingInsn = (x86_instr*)createSplicingInsn();

   // Overwriting an existing launcher

   theKernelDriver.poke1LocUndoableChangeTopOfStack(from,
                                                    ((const x86_instr*)otherGuysCurrInstalledInsn)->getRawBytes(),
                                                    splicingInsn->getRawBytes(),
                                                    0);
        // 0 --> undo me first (undo springboards later)
        // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, splicingInsn->getNumBytes());

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

      x86_instr *insn = (x86_instr*) createSplicingInsn();
      theKernelDriver.undoPoke1LocPop(from,
                                      insn->getRawBytes(),
                                         // expect to see this
                                      ((const x86_instr*)whenAllDoneRestoreToThisInsn)->getRawBytes()
                                         // change back to this
                                      );
   
      // flush original instruction back into the icache!
      flush_a_bunch(from, whenAllDoneRestoreToThisInsn->getNumBytes());
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
