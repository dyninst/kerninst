// power_CondRegClearLauncher.C

#include "power_CondRegClearLauncher.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "power_instr.h"

extern void flush_a_bunch(kptr_t, unsigned nbytes);
   // from directToMemoryEmitter.C

power_CondRegClearLauncher::
power_CondRegClearLauncher(kptr_t ifrom, kptr_t ito,
                    const instr_t *iwhenAllDoneRestoreToThisInsn) :
      inherited(ifrom, ito, iwhenAllDoneRestoreToThisInsn)
{}

void power_CondRegClearLauncher::print_info() const {
   cout << "crclr  from " << addr2hex(from) << " to " << addr2hex(to) << endl;
}

instr_t* power_CondRegClearLauncher::createSplicingInsn() const {
   return new power_instr(power_instr::condreg, power_instr::CREqv, 
                          2, 2, 2); //bit 2 is the zero bit 
}

void power_CondRegClearLauncher::
doInstallNotOverwritingExisting(bool justTesting,
                                const regset_t */*freeRegs*/,
                                kernelDriver &theKernelDriver,
                                bool verboseTimings) {
   if (justTesting) {
      cout << "power_CondRegClearLauncher test (not overwriting existing launcher)."
           << endl
           << "   from=" << addr2hex(from) << " to="
           << addr2hex(to) << endl;
      return;
   }

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   
   const power_instr *branch = (power_instr*)createSplicingInsn();
   unsigned length = whenAllDoneRestoreToThisInsn->getNumBytes();
   char *newVal = (char*) malloc(length);
   uint32_t branchRaw = branch->getRaw();
   memcpy(newVal, &branchRaw, branch->getNumBytes());
   char *whenAllDoneRestoreToThisInsnRawBytes = (char *)malloc(4);
   uint32_t whenAllDoneRestoreToThisInsnRaw = whenAllDoneRestoreToThisInsn->getRaw();
   memcpy(whenAllDoneRestoreToThisInsnRawBytes, &whenAllDoneRestoreToThisInsnRaw, 4);   

   // Call /dev/kerninst to poke address "from" with value "branch.getRaw()"
   // Make it an "undoable" poke, so that if we die without cleanup up, 
   // /dev/kerninst will automatically clean up for us.

   theKernelDriver.poke1LocUndoablePush(from,
                                        whenAllDoneRestoreToThisInsnRawBytes,
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
      
      cout << "power_CondRegClearLauncher [not overwriting existing so used undoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
}

void power_CondRegClearLauncher::
doInstallOverwritingExisting(bool justTesting,
                             const instr_t *otherGuysCurrInstalledInsn,
                                // needed for asserting when we poke
                             const regset_t */*freeRegs*/,
                             kernelDriver &theKernelDriver,
                             bool verboseTimings) {
   if (justTesting) {
      cout << "power_CondRegClearLauncher test (overwriting existing launcher)."
           << endl
           << "   from=" << addr2hex(from) << " to="
           << addr2hex(to) << endl;
      return;
   }

   const power_instr *branch = (power_instr*)createSplicingInsn();
   unsigned length = whenAllDoneRestoreToThisInsn->getNumBytes();
   char *newVal = (char*) malloc(length);
   uint32_t branchRaw = branch->getRaw();
   memcpy(newVal, &branchRaw, branch->getNumBytes());
   char *whenAllDoneRestoreToThisInsnRawBytes = (char *)malloc(4);
   uint32_t whenAllDoneRestoreToThisInsnRaw = whenAllDoneRestoreToThisInsn->getRaw();
   memcpy(whenAllDoneRestoreToThisInsnRawBytes, &whenAllDoneRestoreToThisInsnRaw, 4);   
   char *oldVal = (char*) malloc(length);
   uint32_t otherGuysCurrInstalledInsnRaw = otherGuysCurrInstalledInsn->getRaw();
   memcpy(oldVal, 
          &otherGuysCurrInstalledInsnRaw,
          otherGuysCurrInstalledInsn->getNumBytes());

//   Overwriting an existing launcher

   theKernelDriver.poke1LocUndoableChangeTopOfStack(from,
                                                    oldVal,
                                                    newVal,
                                                    0);
       // 0 --> undo me first (undo springboards later)
       // This is the necessary unwinding order of things.

   // flush launcher into icache!
   flush_a_bunch(from, length);

   delete branch;
   free(newVal);
   free(oldVal);

   const mrtime_t startTime = verboseTimings ? getmrtime() : 0;
   

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      
      cout << "power_CondRegClearLauncher [overwriting existing so used changeUndoable]";
      cout << " took " << totalTime/1000 << " usecs." << endl;
   }
}

// ----------------------------------------------------------------------

void power_CondRegClearLauncher::
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

      power_instr *branch = (power_instr*)createSplicingInsn();
      unsigned length = whenAllDoneRestoreToThisInsn->getNumBytes();
      char *newVal = (char*) malloc(length);
      uint32_t branchRaw = branch->getRaw();
      memcpy(newVal, &branchRaw, branch->getNumBytes());
      char *whenAllDoneRestoreToThisInsnRawBytes = (char *)malloc(4);
      uint32_t whenAllDoneRestoreToThisInsnRaw = whenAllDoneRestoreToThisInsn->getRaw();
      memcpy(whenAllDoneRestoreToThisInsnRawBytes, &whenAllDoneRestoreToThisInsnRaw, 4);   
      
      theKernelDriver.undoPoke1LocPop(from,
                                      newVal, // expect to see this
                                      whenAllDoneRestoreToThisInsnRawBytes
                                         // change back to this
                                      );
   
      // flush original instruction back into the icache!
      flush_a_bunch(from, length);

      delete branch;
      free(newVal);
   }

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - startTime;
      cout << "power_CondRegClearBranchLauncher uninstall took "
           << totalTime / 1000 << " usecs." << endl;
   }
}
                                   
void power_CondRegClearLauncher::
doUninstallAnotherAlreadyInstalled(bool /*justTesting*/,
                                   kernelDriver &/*theKernelDriver*/,
                                   bool /*verboseTimings*/) {
   // this is a nop routine, on purpose
}
