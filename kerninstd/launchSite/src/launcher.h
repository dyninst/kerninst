// launcher.h
// class launcher: Arguably the lowest-level class in kerninstd.
// Manages replacing a single instruction (the "launch site") with a branch
// to somewhere else.  We know nor care nothing about what that "someone else" is.
// This class has never heard the word 'springboard', for example, though clearly
// it's used to get to a springboard, or code patch, or to implement replaceFunction()
// functionality at a caller.  It's general enough for all that.
//
// This class could be made simpler if the ctor truly did an install() and
// the dtor did an uninstall().  Is this practical?

#ifndef _LAUNCHER_H_
#define _LAUNCHER_H_

#include "common/h/Vector.h"
#include "instr.h"
#include "kernelDriver.h"

class SpringBoardHeap;

class launcher {
 private:
   launcher(const launcher &);
   launcher& operator=(const launcher &);

   bool installed;
   
 protected:
   kptr_t from;
   kptr_t to;
   const instr_t *whenAllDoneRestoreToThisInsn;
      // not necessarily the same as a copy of memory when we created this
      // launcher, because consider the case where this launcher was created
      // while another launcher was installed, and we are gonna overwrite that
      // guy.  In that case, we want to set our "whenAllDoneRestoreToThisInsn"
      // to a copy of the other guy's "whenAllDoneRestoreToThisInsn".
      // The other guy, if it was not installed on top of an existing launcher,
      // should set its "whenAllDoneRestoreToThisInsn" to a copy of memory
      // (or perhaps even get1OrigInsn() will do, though that routine always
      // makes me nervous.)

   // The following routines actually change the kernel!
   virtual void doInstallNotOverwritingExisting(bool justTesting,
                                                const regset_t *freeRegs,
                                                kernelDriver &,
                                                bool verboseTimings) = 0;
   virtual void doInstallOverwritingExisting(bool justTesting,
                                             const instr_t *otherGuysCurrInstalledInsn,
                                                // needed for asserting when we poke
                                             const regset_t *freeRegs,
                                             kernelDriver &,
                                             bool verboseTimings) = 0;

   virtual void doUninstallNoOtherAlreadyInstalled(bool justTesting,
                                                   kernelDriver &,
                                                   bool verboseTimings) = 0;
      // put back the original instruction (whenAllDoneRestoreToThisInsn)

   virtual void doUninstallAnotherAlreadyInstalled(bool justTesting,
                                                   kernelDriver &,
                                                   bool verboseTimings) = 0;
      // usually does a nop in this case, though springboard launcher
      // will deallocate stuff

   static instr_t* makeQuickBranch(kptr_t fromaddr, kptr_t toaddr,
				   bool annulled);

 public:
   launcher(kptr_t ifrom, kptr_t ito,
            const instr_t *iwhenAllDoneRestoreToThisInsn) :
      installed(false),
      from(ifrom), to(ito),
      whenAllDoneRestoreToThisInsn(iwhenAllDoneRestoreToThisInsn)
   {}

   virtual ~launcher() {
      assert(!installed);
         // because if installed, I'm not sure whether to pass true or false
         // to unInstall()
      delete whenAllDoneRestoreToThisInsn;      
   }

   virtual void print_info() const = 0;

   kptr_t getFrom() const { return from; }
   kptr_t getTo() const { return to; }
   const instr_t* getWhenAllDoneRestoreToThisInsn() const {
      return whenAllDoneRestoreToThisInsn;
   }

   virtual instr_t* createSplicingInsn() const = 0; // helpful for asserts

   void installNotOverwritingExisting(bool justTesting,
                                      const regset_t *freeRegs,
                                      kernelDriver &theKernelDriver,
                                      bool verboseTimings) {
      if (!installed) {
         doInstallNotOverwritingExisting(justTesting, freeRegs, theKernelDriver,
                                         verboseTimings);
         installed = true;
      }
   }
   
   void installOverwritingExisting(bool justTesting,
                                   const instr_t *otherGuysCurrInstalledInsn,
                                      // needed for asserting when we poke
                                   const instr_t *otherGuysRestoreToInsn,
                                      // we assert equals our
                                      // whenAllDoneRestoreToInsn, or we weren't
                                      // created right.
                                   const regset_t *freeRegs,
                                   kernelDriver &theKernelDriver,
                                   bool verboseTimings);

   void unInstallNoOtherAlreadyInstalled(bool justTesting,
                                         kernelDriver &theKernelDriver,
                                         bool verboseTimings) {
      if (installed) {
         doUninstallNoOtherAlreadyInstalled(justTesting,
                                            theKernelDriver,
                                            verboseTimings);
         installed = false;
      }
   }

   void unInstallAnotherAlreadyInstalled(bool justTesting,
                                         kernelDriver &theKernelDriver,
                                         bool verboseTimings) {
      // usually a nop, though springboard launcher will deallocate some stuff

      if (installed) {
         doUninstallAnotherAlreadyInstalled(justTesting, theKernelDriver,
                                            verboseTimings);
         installed = false;
      }
   }

   static launcher *pickAnAnnulledLauncher(kptr_t from, kptr_t to,
                                           const instr_t *whenAllDoneRestoreToThisInsn,
                                           SpringBoardHeap &theSpringBoardHeap);
};

#endif
