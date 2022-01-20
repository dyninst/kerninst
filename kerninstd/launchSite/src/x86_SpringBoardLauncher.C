// x86_SpringboardLauncher.C

#include "SpringboardLauncher.h"
#include "abi.h"
#include "insnVec.h"
//#include "freeUpSpringboardInternalEvent.h"
#include "loop.h" // doOneTimeInternalEventInTheFuture()
#include "util/h/out_streams.h"
#include "util/h/rope-utils.h"
#include "util/h/mrtime.h"
#include "x86_instr.h"

//extern void flush_a_bunch(kptr_t addr, unsigned nbytes);
   // from directToMemoryEmitter.C

static uint64_t msec2nsec(uint64_t msecs) {
   return msecs * 1000000;
}
   
SpringboardLauncher::
SpringboardLauncher(kptr_t ifrom, kptr_t ito,
                    const instr_t *iwhenAllDoneRestoreToThisInsn,
                    SpringBoardHeap &iSBHeap) :
            launcher(ifrom, ito, iwhenAllDoneRestoreToThisInsn),
            theSpringBoardHeap(iSBHeap)
{
   // theItem left uninitialized
}

void SpringboardLauncher::print_info() const {
   cout << std::hex << "SpringboardLauncher.  from 0x" << from 
	<< " to 0x" << to << endl;
   cout << "sb contents unknown at this time (until install())" << endl << std::dec;
}

instr_t* SpringboardLauncher::createSplicingInsn() const {
   const kptr_t sbItemAddr = theSpringBoardHeap.item2addr(theItem);

   //assert(x86_instr::inRangeOfBicc(from, sbItemAddr));

   return makeQuickBranch(from, sbItemAddr, true); // true --> annulled
}

void SpringboardLauncher::
doInstallCommon(const regset_t *freeRegs,
                kernelDriver &theKernelDriver,
                bool verboseTimings) {
   // Given 'from' (a protected inherited vrble), we can reach a certain range of
   // potential springboard addresses.  We mark that range by lowestOKAddr and
   // highestOKAddr.

   const mrtime_t phase1StartTime = verboseTimings ? getmrtime() : 0;
   
   try {
      kptr_t min_reachable_springboard_bpcc, max_reachable_springboard_bpcc;
      assert(!"MJB FIXME: SpringboardLauncher::doInstallCommon()\n");
      //  x86_instr::getRangeOfBPcc(from,
//                                    min_reachable_springboard_bpcc,
//                                    max_reachable_springboard_bpcc);

      theItem = theSpringBoardHeap.alloc(min_reachable_springboard_bpcc,
                                         max_reachable_springboard_bpcc,
                                         getFrom(), // helps avoid cache line conflicts
                                         getTo() // helps avoid cache line conflicts
                                         );
      // NOTE: may throw SpringBoardHeap::CouldntFindOneFreeInRange()
   }
   catch(...) {
      kptr_t min_reachable_springboard_bicc, max_reachable_springboard_bicc;
      assert(!"MJB FIXME: SpringboardLauncher::doInstallCommon()\n");
      //  x86_instr::getRangeOfBicc(from,
//                                    min_reachable_springboard_bicc,
//                                    max_reachable_springboard_bicc);

      try {
         theItem = theSpringBoardHeap.alloc(min_reachable_springboard_bicc,
                                            max_reachable_springboard_bicc,
                                            getFrom(), // helps avoid $-line conflicts
                                            getTo() // helps avoid $-line conflicts
                                            );
         // NOTE: may throw SpringBoardHeap::CouldntFindOneFreeInRange()

         dout << "SpringBoardLauncher note: could not find a "
	         "springboard element within range of BPcc from " 
	      << addr2hex(from) << endl 
	      << "But did one one with range of Bicc" << endl;
      }
      catch (...) {
         dout << "SpringBoardLauncher failed to find any springboard "
		 "element within range of bpcc or bicc" << endl;
         throw SpringBoardHeap::CouldntFindOneFreeInRange();
      }
   }
   
   const kptr_t sbItemAddr = theSpringBoardHeap.item2addr(theItem);
   //assert(x86_instr::inRangeOfBicc(from, sbItemAddr));
   
   insnVec_t *springboardContents = insnVec_t::getInsnVec();
   extern abi *theKernelABI;

   assert(!"MJB FIXME: SpringboardLauncher::doInstallCommon()\n");
   // get a scratch register; throw an exception if one doesn't exist
   //  const bool no_regs_free = freeRegs->countIntRegs() == 0;
//     if (x86_instr::inRangeOfCall(from, to)) {
//        if (no_regs_free) {
//  	 // Note: in the future, we should omit the restore, and
//  	 // somehow inform the patch that it's executing in a frame 
//  	 // where a save has been done ("you're welcome, mr. patch...
//  	 // just do the restore for us, please")

//  	 instr_t *i = new x86_instr(x86_instr::save, 
//  				      -theKernelABI->
//  				      getMinFrameAlignedNumBytes());
//  	 springboardContents->appendAnInsn(i);
//  	 i = new x86_instr(x86_instr::callandlink,
//  			     to, sbItemAddr + 4);
//  	 springboardContents->appendAnInsn(i);
//  	 i = new x86_instr(x86_instr::restore);
//  	 springboardContents->appendAnInsn(i);
//        } 
//        else if (freeRegs->exists(x86_reg::o7)) {
//  	 // %o7 is free; emit call; nop; nop.  This is arguably faster than
//  	 // mov; call; mov (mostly depending on who's doing the arguing).
//  	 instr_t *i = new x86_instr(x86_instr::callandlink,
//  				      to, sbItemAddr); // toaddr,fromaddr
//  	 springboardContents->appendAnInsn(i);
//  	 i = new x86_instr(x86_instr::nop);
//  	 springboardContents->appendAnInsn(i);
//  	 i = new x86_instr(x86_instr::nop);
//  	 springboardContents->appendAnInsn(i);
//        } 
//        else {
//  	 regset_t::const_iterator iter = freeRegs->begin();
//  	 const x86_reg &tempreg = (const x86_reg&)*iter;
//  	 instr_t *i = new x86_instr(x86_instr::mov,
//  				      x86_reg::o7, tempreg);
//  	 springboardContents->appendAnInsn(i);
//  	 i = new x86_instr(x86_instr::callandlink,
//  			     to, sbItemAddr + 4);
//  	 springboardContents->appendAnInsn(i);
//  	 i = new x86_instr(x86_instr::mov,
//  			     tempreg, x86_reg::o7);
//  	 springboardContents->appendAnInsn(i);
//        }
//     } 
//     else {
//         // The code below seems to work, but we do not need it anymore --
//         // kernel code and our instrumentation patches are now in the
//         // 32-bit space. The following assert guarantees that.
//         assert(false);

//        if (no_regs_free) {
//  	 instr_t *i = new x86_instr(x86_instr::save, 
//  				      -theKernelABI->getMinFrameAlignedNumBytes());
//  	 springboardContents->appendAnInsn(i);
//  	 x86_instr::genJumpAndLink(springboardContents, to, x86_reg::l0);
//  	 i = new x86_instr(x86_instr::restore);
//  	 springboardContents->appendAnInsn(i);
//        }
//        else {
//  	 x86_reg& tempreg = (x86_reg&)*(freeRegs->begin());
//  	 x86_instr::genJumpAndLink(springboardContents, to, tempreg);
//  	 instr_t *i = new x86_instr(x86_instr::nop);
//  	 springboardContents->appendAnInsn(i);
//        }
//     }

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - phase1StartTime;
      cout << "SpringboardLauncher install phase 1 [find sb, think of code to generate]"
           << " took " << totalTime/1000 << " usecs." << endl;
   }

   // Write the springboard contents.  
   // Make it an "undoable" poke, of course.

   const mrtime_t phase2StartTime = verboseTimings ? getmrtime() : 0;

   // NOTE: I don't really like all this peek-ing, which I find to be
   // unnecessary and quite expensive.  After all, we're doing an undoable
   // poke, so who cares about the original contents; the driver does its own
   // peek anyway.  On the theory that it's nice to have the redundant
   // information for assertion checking, we can still speed things up by having
   // the driver return its poked information.  While we're on the subject, how
   // about a batched undoable poke (contiguous sequence) feature?
   assert(!"MJB FIXME: SpringboardLauncher::doInstallCommon()\n");
   //  insnVec_t::const_iterator iter;
//     unsigned offset = 0;
//     for(iter = springboardContents->begin(); 
//         iter < springboardContents->end(); iter++) {
//        orig_sb_contents += theKernelDriver.peek1Word(sbItemAddr + offset);
//        offset += 4;
//     }

   // It'd be nice to use directToMemoryEmitter, because of its built-in
   // icache flush logic.  Unfortunately, it doesn't know how to do an
   // undoable poke.
   //  offset = 0;
//     for (iter = springboardContents->begin(); 
//          iter < springboardContents->end(); ++iter) {
//        const uint32_t oldContents = orig_sb_contents[iter - springboardContents->begin()];
      
//        theKernelDriver.poke1WordUndoablePush(sbItemAddr + offset, // addr
//                                              oldContents,
//                                              (*iter)->getRaw(), // newValue
//                                              1 // 1 --> undo after launchers are undone
//                                              );
//        // true --> sure, give me a warning if undo info is already being kept
//        // at this location

//        offset += 4;
//     }
   // flush springboard into icache! (Because up to now, it's just in the D-Cache)
   
   //flush_a_bunch(sbItemAddr, springboardContents->numInsns() * 4);
   delete springboardContents;

   if (verboseTimings) {
      const mrtime_t totalTime = getmrtime() - phase2StartTime;
      cout << "SpringboardLauncher install phase 2 [write to sb in kernel w/undoable pokes] took "
           << totalTime/1000 << " usecs." << endl;
   }
}

void SpringboardLauncher::
doInstallNotOverwritingExisting(bool justTesting,
                                const regset_t *freeRegs,
                                kernelDriver &theKernelDriver,
                                bool verboseTimings) {
   doInstallCommon(freeRegs, theKernelDriver, verboseTimings);
   
   // Finally, write the quickbranch (to the springboard) to the kernel.
   // Use undoable poke.

   const mrtime_t phase3StartTime = verboseTimings ? getmrtime() : 0;

   if (justTesting) {
      const kptr_t sbItemAddr = theSpringBoardHeap.item2addr(theItem);
      //assert(x86_instr::inRangeOfBicc(from, sbItemAddr));

      cout << std::hex << "sb launcher test (not overwriting existing): from 0x" << from
	   << " to 0x" << to << endl
	   << "via a springboard that starts at 0x" << sbItemAddr << endl << std::dec;
   }
   else {
      const instr_t *toSpringboardInsn = createSplicingInsn();
      assert(!"MJB FIXME: SpringboardLauncher::doInstallNotOverwritingExisting()\n");
      //  theKernelDriver.poke1WordUndoablePush(from,
//                                              whenAllDoneRestoreToThisInsn->getRaw(),
//                                                 // expect to see this...
//                                              toSpringboardInsn->getRaw(),
//                                                 // change it to this.
//                                              0);
//           // 0 --> when/if it comes time to do an emergency undo, undo me first
      
      // flush launcher into icache!
      //flush_a_bunch(from, 4);

      if (verboseTimings) {
         const mrtime_t totalTime = getmrtime() - phase3StartTime;
         cout << "SpringboardLauncher install phase 3 [write branch, no overwrite existing so undoable poke] took " 
              << totalTime/1000 << " usecs." << endl;
      }

      delete toSpringboardInsn;
   }
}

void SpringboardLauncher::
doInstallOverwritingExisting(bool justTesting,
                             const instr_t *otherGuysCurrInstalledInsn,
                                // needed for asserting when we poke
                             const regset_t *freeRegs,
                             kernelDriver &theKernelDriver,
                             bool verboseTimings) {
   doInstallCommon(freeRegs, theKernelDriver, verboseTimings);
   
   // Finally, write the quickbranch (to the springboard) to the kernel.

   const mrtime_t phase3StartTime = verboseTimings ? getmrtime() : 0;

   if (justTesting) {
      const kptr_t sbItemAddr = theSpringBoardHeap.item2addr(theItem);
      //assert(x86_instr::inRangeOfBicc(from, sbItemAddr));

      cout << std::hex << "sb launcher test (overwriting existing): from 0x" << from
	   << " to 0x" << to << endl
	   << "via a springboard that starts at 0x" << sbItemAddr << endl << std::dec;
   }
   else {
      const instr_t *toSpringboardInsn = createSplicingInsn();
      assert(!"MJB FIXME: SpringboardLauncher::doInstallOverwritingExisting()\n");
      //  theKernelDriver.poke1WordUndoableChangeTopOfStack(from,
//                                                          otherGuysCurrInstalledInsn->getRaw(),
//                                                             // expect to see this...
//                                                          toSpringboardInsn->getRaw(),
//                                                             // change it to this.
//                                                          0);
//           // 0 --> when/if it comes time to do an emergency undo, undo me first

      // flush launcher into icache!
      //flush_a_bunch(from, 4);

      if (verboseTimings) {
         const mrtime_t totalTime = getmrtime() - phase3StartTime;
         cout << "SpringboardLauncher install phase 3 [write branch , overwriting existing launcher/no undoable poke needed] took "
              << totalTime/1000 << " usecs." << endl;
      }
      delete toSpringboardInsn;
   }
}

// ----------------------------------------------------------------------

void SpringboardLauncher::
doUninstallNoOtherAlreadyInstalled(bool justTesting,
                                   kernelDriver &theKernelDriver,
                                   bool verboseTimings) {
   const mrtime_t phase1StartTime = verboseTimings ? getmrtime() : 0;

   if (!justTesting) {
      // call kernel driver to restore the orig insn
     assert(!"MJB FIXME: SpringboardLauncher::doUninstallNoOtherAlreadyInstalled()\n");
      //  theKernelDriver.undoPoke1WordPop(from,
//                                         createSplicingInsn()->getRaw(),
//                                            // expect to see this
//                                         whenAllDoneRestoreToThisInsn->getRaw()
//                                            // change back to this
//                                         );

      // flush launcher into icache!
      //flush_a_bunch(from, 4);
   }

   if (verboseTimings) {
      const mrtime_t phase1TotalTime = getmrtime() - phase1StartTime;
      cout << "SpringboardLauncher uninstall phase 1 [splice insn] took "
           << phase1TotalTime / 1000 << " usecs." << endl;
   }

   // After a little while (10 millisecs), we'll assume that no code could possibly
   // be executing within the springboard any longer.  (It's possible after such
   // a short time, however, that some thread is still within a code patch that was
   // jumped to by this springboard, however -- but this class doesn't deal with such
   // issues.)  So, at that time, we leisurely restore the contents of the springboard,
   // and also free up that item in the springboard heap, so it may immediately be
   // reallocated & rewritten.
assert(!"MJB FIXME: SpringboardLauncher::doUninstallNoOtherAlreadyInstalled()\n");
   //  internalEvent *theInternalEvent =
//        new freeUpSpringboardInternalEvent(getmrtime() + msec2nsec(10),
//                                           theSpringBoardHeap,
//                                           theKernelDriver,
//                                           verboseTimings,
//                                           theItem, // specifies the springboard location
//                                           orig_sb_contents // what to restore to
//                                           );
//     doOneTimeInternalEventInTheFuture(theInternalEvent); // loop.C
}

void SpringboardLauncher::
doUninstallAnotherAlreadyInstalled(bool /*justTesting*/,
                                   kernelDriver &theKernelDriver,
                                   bool verboseTimings) {
   // We do not restore the splicing branch insn, because another has already
   // been installed

   // After a little while (10 millisecs), we'll assume that no code could possibly
   // be executing within the springboard any longer.  (It's possible after such
   // a short time, however, that some thread is still within a code patch that was
   // jumped to by this springboard, however -- but this class doesn't deal with such
   // issues.)  So, at that time, we leisurely restore the contents of the springboard,
   // and also free up that item in the springboard heap, so it may immediately be
   // reallocated & rewritten.
assert(!"MJB FIXME: SpringboardLauncher::doUninstallAnotherAlreadyInstalled()\n");
   //  internalEvent *theInternalEvent =
//        new freeUpSpringboardInternalEvent(getmrtime() + msec2nsec(10),
//                                           theSpringBoardHeap,
//                                           theKernelDriver,
//                                           verboseTimings,
//                                           theItem, // specifies the springboard location
//                                           orig_sb_contents // what to restore to
//                                           );
//     doOneTimeInternalEventInTheFuture(theInternalEvent); // loop.C
}
