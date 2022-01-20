// LRAndR.h
// launcher, relocator, and returner
// Together in this class since the form of each are decided at the same time
// (in launchSite::install())

#ifndef _LRANDR_H_
#define _LRANDR_H_

#include "util/h/kdrvtypes.h"
#include "instr.h"
#include "launcher.h"

// fwd decls, when possible, make for smaller .o files:
class function_t;
class kernelDriver;
class SpringBoardHeap;
class directToMemoryEmitter_t;

class LRAndR {
 protected:

   const instr_t *originsn;
   kptr_t launchSiteAddr; // launcher's "from"
   kptr_t patchAddr; // launcher's "to".  Unknown until registerPatchAddr()

   const regset_t *freeIntRegsBeforeInsn;
   const regset_t *freeIntRegsAfterInsn;
      // Doesn't take into account the possibility of an emitted save/restore.
      // See class LaunchSite for that.

   // Note that freeIntRegsAfterInsn isn't needed by all classes, so it's
   // probably a bad idea to put it here in the base class.
   // some classes (unwound call/restore comes to mind) will also store
   // freeIntRegsBeforeReturn.

   launcher *theLauncher; // base class.  NULL until registerPatchAddr()

   SpringBoardHeap &theSpringBoardHeap;
   
   void setLauncherToAnyAnnulledLauncher(kptr_t patchAddr,
                                         const instr_t *whenAllDoneRestoreToThisInsn) {
      theLauncher = launcher::pickAnAnnulledLauncher(launchSiteAddr, // from
						     patchAddr, // to
						     whenAllDoneRestoreToThisInsn,
						     theSpringBoardHeap);
   }

   void setPatchAddr(kptr_t iPatchAddr) {
      assert(patchAddr == 0);
      patchAddr = iPatchAddr;
   }
   
 public:
   class FailedToInstallLauncher {};
   
   LRAndR(kptr_t iLaunchSiteAddr, const instr_t *ioriginsn,
          const regset_t *iFreeRegsBefore, const regset_t *iFreeRegsAfter,
          SpringBoardHeap &iSbHeap) : originsn(ioriginsn),
                                      freeIntRegsBeforeInsn(iFreeRegsBefore),
                                      freeIntRegsAfterInsn(iFreeRegsAfter),
                                      theSpringBoardHeap(iSbHeap) {
      launchSiteAddr = iLaunchSiteAddr;
      patchAddr = 0;
      theLauncher = NULL;
   }
   
   virtual ~LRAndR() {
      assert(theLauncher == NULL);
      delete originsn;
      delete freeIntRegsBeforeInsn;
      delete freeIntRegsAfterInsn;
   }

   // Important static method (used in at least 2 distinct places in outside code).
   // Picking the right kind of LRAndR, given an instruction, is a useful task.
   static LRAndR *create_LRAndR(kptr_t addr, const function_t &fn,
                                const instr_t*, 
				const instr_t::controlFlowInfo *cfi,
                                const regset_t *intRegsAvailBeforeInsn,
                                const regset_t *intRegsAvailAfterInsn,
                                bool preReturnSnippets,
                                bool verbose
                                   // e.g. print warnings for not yet
                                   // implemented tail call unwindings.
                                );

   instr_t* getLaunchersSplicingInsn() const {
      assert(theLauncher);
      return theLauncher->createSplicingInsn();
   }
   const instr_t* getLaunchersWhenAllDoneRestoreToInsn() const {
      assert(theLauncher);
      return theLauncher->getWhenAllDoneRestoreToThisInsn();
   }

   virtual unsigned conservative_numInsnBytesForRAndR() const = 0;
      // if the RAndR is in several portions (unwound call/restore comes to mind), we
      // return the sum of all portions.

   virtual const regset_t* getInitialIntRegsForExecutingPreInsnSnippets() const {
      // we provide a reasonable default: freeIntRegsBeforeInsn.
      // If a derived class has a different answer, it must provide its own method.
      // We say 'Initial' because it doesn't take into account an emitted save, which
      // can beef up the result nicely when needed.
      return freeIntRegsBeforeInsn;
   }
   
   virtual const regset_t* getInitialIntRegsForExecutingPreReturnSnippets() const {
      // we provide a reasonable default: regset_t::emptyset
      // If a derived class has a different answer, it must provide its own method.
      // We say 'Initial' because it doesn't take into account an emitted save, which
      // can beef up the result nicely when needed.
      return regset_t::getRegSet(regset_t::empty);
   }

   // We provide reasonable defaults that work with most LRAndRs, but some of them
   // (unwound call/restore comes to mind) had better override these virtual fns:
   // UPDATE: unwound call/restore doesn't seem to be defining these methods anymore!
   // Are these methods therefore obsolete????
   virtual bool preReturnHasMeaning() const { return false; } // override if appropriate
//   virtual bool belongsPreInsn(const codeSnippet &) const { return true; }
//   virtual bool belongsPreReturn(const codeSnippet &) const { return false; }
   

   virtual void registerPatchAddrAndCreateLauncher(kptr_t patchAddr,
                                                   const instr_t *whenAllDoneRestoreToThisInsn
                                                   ) = 0;

   // we provide the reasonable nop defaults for the 1st and 3rd methods; derived
   // classes that need them (unwound call/restore comes to mind) had better implement
   // them.  In any event, the 2nd method is mandatory.
   virtual void emitStuffBeforePreInsnSnippets(directToMemoryEmitter_t &,
                                               const function_t &,
                                               const instr_t::controlFlowInfo*)
                                                  const {}
   virtual void emitStuffAfterPreInsnSnippets(directToMemoryEmitter_t &,
                                              const function_t &,
                                              const instr_t::controlFlowInfo*)
                                                 const = 0;
   virtual void emitStuffAfterPreReturnSnippets(directToMemoryEmitter_t &,
                                                const function_t &,
                                                const instr_t::controlFlowInfo*)
                                                   const {}

   void installLauncherNotOverwritingExisting(bool justTesting,
                                              kernelDriver &theKernelDriver,
                                              bool verboseTimings) {
      // instruments the kernel; can throw FailedToInstallLauncher
      try {
	 theLauncher->installNotOverwritingExisting(justTesting,
						    freeIntRegsBeforeInsn,
						    theKernelDriver,
						    verboseTimings);
         // depending on the launcher, can throw any number of exceptions.
         // To be cleaner, we should probably use derived exceptions, thus 
	 // obviating the need for the ugly catch(...), right?
      }
      catch (...) {
	 throw FailedToInstallLauncher();
      }
   }

   void installLauncherOverwritingExisting(bool justTesting,
                                           const instr_t *otherGuysCurrInstalledInsn,
                                              // needed for asserting when we poke
                                           const instr_t *otherGuysRestoreToInsn,
                                              // we assert equals that of the launcher,
                                              // else it wasn't created right
                                           kernelDriver &theKernelDriver,
                                           bool verboseTimings) {
      // instruments the kernel; can throw FailedToInstallLauncher
      try {
	 theLauncher->installOverwritingExisting(justTesting,
						 otherGuysCurrInstalledInsn,
						 otherGuysRestoreToInsn,
						 freeIntRegsBeforeInsn,
						 theKernelDriver,
						 verboseTimings);
         // depending on the launcher, can throw any number of exceptions.
	 // To be cleaner, we should probably use derived exceptions, thus 
	 // obviating the need for the ugly catch(...), right?
      }
      catch (...) {
	 throw FailedToInstallLauncher();
      }
   }

   void unInstallLauncherAndFryNoOtherAlreadyInstalled(bool justTesting,
                                                       kernelDriver &theKernelDriver,
                                                       bool verboseTimings) {
      theLauncher->unInstallNoOtherAlreadyInstalled(justTesting,
						    theKernelDriver,
						    verboseTimings);

      delete theLauncher;
      theLauncher = NULL; // dtor asserts this...
   }

   void unInstallLauncherAndFryAnotherAlreadyInstalled(bool justTesting,
                                                       kernelDriver &theKernelDriver,
                                                       bool verboseTimings) {
      theLauncher->unInstallAnotherAlreadyInstalled(justTesting,
						    theKernelDriver,
						    verboseTimings);

      delete theLauncher;
      theLauncher = NULL; // dtor asserts this...
   }
};

#endif
