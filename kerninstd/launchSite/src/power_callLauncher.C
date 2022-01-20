// power_callLauncher.C

#include "callLauncher.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "power_instr.h"

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
  return new power_instr(power_instr::branch, to - from, 0, /* not absolute */
                          1 /* link */  );
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
   
   const power_instr *splicingInsn = (power_instr*)createSplicingInsn();

   // Call /dev/kerninst to poke address "from" with value "splicingInsn.getRaw()"
   // Make it an "undoable" poke, so that if we die without cleanup up, /dev/kerninst
   // will automatically clean up for us (by "undo-ing the poke").
   
   //this is temporary until we change the driver to handle fixed-size instrs 
   char *whenAllDoneRestoreToThisInsnRawBytes = (char *) malloc(4);
   char *splicingInsnRawBytes = (char *) malloc(4);
   uint32_t whenAllDoneRestoreToThisInsnRaw = whenAllDoneRestoreToThisInsn->getRaw();
   uint32_t splicingInsnRaw = splicingInsn->getRaw();
   memcpy(whenAllDoneRestoreToThisInsnRawBytes, &whenAllDoneRestoreToThisInsnRaw, 4); 
   memcpy(splicingInsnRawBytes, &splicingInsnRaw, 4); 
   theKernelDriver.poke1LocUndoablePush(from,
                                        whenAllDoneRestoreToThisInsnRawBytes,
                                           // expect to see this...
                                        splicingInsnRawBytes,
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
   
   const power_instr *splicingInsn = (power_instr*)createSplicingInsn();

   // Overwriting an existing launcher

   //this is temporary until we change the driver to handle fixed-size instrs 
   char *otherGuysCurrInstalledInsnRawBytes = (char *) malloc(4);
   char *splicingInsnRawBytes = (char *) malloc(4);
   uint32_t otherGuysCurrInstalledInsnRaw = otherGuysCurrInstalledInsn->getRaw();
   uint32_t splicingInsnRaw = splicingInsn->getRaw();
   memcpy(otherGuysCurrInstalledInsnRawBytes, &otherGuysCurrInstalledInsnRaw, 4); 
   memcpy(splicingInsnRawBytes, &splicingInsnRaw, 4); 
   theKernelDriver.poke1LocUndoableChangeTopOfStack(from,
                                                    otherGuysCurrInstalledInsnRawBytes,
                                                    splicingInsnRawBytes,
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

      power_instr *insn = (power_instr*) createSplicingInsn();
   
     //this is temporary until we change the driver to handle fixed-size instrs 
     char *whenAllDoneRestoreToThisInsnRawBytes = (char *) malloc(4);
     uint32_t whenAllDoneRestoreToThisInsnRaw = whenAllDoneRestoreToThisInsn->getRaw();
     uint32_t insnRaw = insn->getRaw();
     memcpy(whenAllDoneRestoreToThisInsnRawBytes, &whenAllDoneRestoreToThisInsnRaw, 4); 
     char *insnRawBytes = (char *) malloc(4);
     memcpy(insnRawBytes, &insnRaw, 4); 
      theKernelDriver.undoPoke1LocPop(from,
                                      insnRawBytes,
                                         // expect to see this
                                      whenAllDoneRestoreToThisInsnRawBytes
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
