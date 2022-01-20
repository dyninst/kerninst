// x86_TrapLauncher.C

#include "TrapLauncher.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "x86_instr.h"

extern void flush_a_bunch(kptr_t, unsigned nbytes);
   // from directToMemoryEmitter.C

TrapLauncher::
TrapLauncher(kptr_t ifrom, kptr_t ito,
             const instr_t *iwhenAllDoneRestoreToThisInsn) :
      inherited(ifrom, ito, iwhenAllDoneRestoreToThisInsn)
{}

void TrapLauncher::print_info() const {
   cout << "Trap launcher: from " << addr2hex(from) << " to " << addr2hex(to) << endl;
}

instr_t* TrapLauncher::createSplicingInsn() const {
   return new x86_instr(x86_instr::int3);
}

void TrapLauncher::
doInstallNotOverwritingExisting(bool justTesting,
                                const regset_t */*freeRegs*/,
                                kernelDriver &theKernelDriver,
                                bool verboseTimings) {
   if (justTesting) {
      cout << "TrapLauncher test (not overwriting existing launcher)."
           << endl
           << "   from=" << addr2hex(from) << " to="
           << addr2hex(to) << endl;
      return;
   }

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   
   const x86_instr *trap = (x86_instr*)createSplicingInsn();
   unsigned length = whenAllDoneRestoreToThisInsn->getNumBytes();
   char *newVal = (char*) malloc(length);
   memcpy(newVal, trap->getRawBytes(), trap->getNumBytes());
   if(trap->getNumBytes() < length) {
      for(unsigned i=trap->getNumBytes(); i<length; i++)
         newVal[i] = (char)0x90; //nop padding
   }

   // Call /dev/kerninst to record patchAddr (to) associated with this trap
   // (from)

   theKernelDriver.insertTrapToPatchAddrMapping(from, to);

   // Call /dev/kerninst to poke address "from" with value "trap.getRaw()"
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

   delete trap;
   free(newVal);

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "TrapLauncher [not overwriting existing so used undoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
}

void TrapLauncher::
doInstallOverwritingExisting(bool justTesting,
                             const instr_t *otherGuysCurrInstalledInsn,
                                // needed for asserting when we poke
                             const regset_t */*freeRegs*/,
                             kernelDriver &theKernelDriver,
                             bool verboseTimings) {
   if (justTesting) {
      cout << "TrapLauncher test (overwriting existing launcher)."
           << endl
           << "   from=" << addr2hex(from) << " to="
           << addr2hex(to) << endl;
      return;
   }

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;

   // Call /dev/kerninst to update patchAddr (to) associated with this trap
   // (from)

   theKernelDriver.updateTrapToPatchAddrMapping(from, to);

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "TrapLauncher [overwriting existing so used changeUndoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
}

// ----------------------------------------------------------------------

void TrapLauncher::
doUninstallNoOtherAlreadyInstalled(bool justTesting,
                                   kernelDriver &theKernelDriver,
                                   bool verboseTimings) {
   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   
   if (!justTesting) {
      // call driver to poke address [from] with the instruction
      // "whenAllDoneRestoreToThisInsn"

      x86_instr *trap = (x86_instr*)createSplicingInsn();
      unsigned length = whenAllDoneRestoreToThisInsn->getNumBytes();
      char *newVal = (char*) malloc(length);
      memcpy(newVal, trap->getRawBytes(), trap->getNumBytes());
      if(trap->getNumBytes() < length) {
         for(unsigned i=trap->getNumBytes(); i<length; i++)
            newVal[i] = (char)0x90; //nop padding
      }
      
      theKernelDriver.undoPoke1LocPop(from,
                                      newVal, // expect to see this
                                      ((const x86_instr*)whenAllDoneRestoreToThisInsn)->getRawBytes()
                                         // change back to this
                                      );

      // Call /dev/kerninst to remove patchAddr (to) association with this
      // trap (from)
      theKernelDriver.removeTrapToPatchAddrMapping(from);

      // flush original instruction back into the icache!
      flush_a_bunch(from, length);

      delete trap;
      free(newVal);
   }

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      cout << "TrapLauncher uninstall took "
           << totalTime / 1000 << " usecs." << endl;
   }
}
                                   
void TrapLauncher::
doUninstallAnotherAlreadyInstalled(bool /*justTesting*/,
                                   kernelDriver &/*theKernelDriver*/,
                                   bool /*verboseTimings*/) {
   // this is a nop routine, on purpose
}
