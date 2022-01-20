// replaceFunctionCall.C

#include "util/h/mrtime.h"
#include "replaceFunctionCall.h"

replaceFunctionCall::replaceFunctionCall(const kptr_t iCallSiteAddr,
                                         const function_t &ioldfn,
                                         const moduleMgr &iModuleMgr,
                                         kernelDriver &iKernelDriver,
                                         SpringBoardHeap &iSpringBoardHeap) :
   theModuleMgr(iModuleMgr),
   theKernelDriver(iKernelDriver),
   theSpringBoardHeap(iSpringBoardHeap),
   callSiteAddr(iCallSiteAddr),
   oldfn(&ioldfn),
   // presentRequests left empty
   theLauncher(NULL) // only non-NULL when install()'d
{
   // There is no need to install our callbacks (see .h file) until we have
   // install()'d.
}

replaceFunctionCall::~replaceFunctionCall() {
   // assert everything has been uninstalled
   assert(presentRequests.size() == 0);
   assert(theLauncher == NULL);
}

// --------------------
   
void replaceFunctionCall::kerninstdIsGoingDown() {
   // kerninstd is dying, undo changes NOW!

   if (theLauncher == NULL) {
      // nothing needed
      assert(presentRequests.size() == 0);
      return;
   }
   assert(presentRequests.size() > 0);

   assert(theLauncher);
   theLauncher->unInstallNoOtherAlreadyInstalled(false, // not justTesting,
                                                 theKernelDriver,
                                                 false // no verbose timing
                                                 );

   delete theLauncher;
   theLauncher = NULL;
}

// --------------------

callLauncher*
replaceFunctionCall::makeLauncher(const function_t *newfn) const {

   // We can assert that the instruction at this call site was a call to 
   // oldfn->getEntryAddr().  (But we must peek at its "current" value,
   // from memory, not as parsed via get1OrigInsn(), because that would
   // crash if this call site were replaced then replaced again!)

#ifdef sparc_sun_solaris2_7
   const sparc_instr *callingInsnCurrInMemory = new sparc_instr(theKernelDriver.peek1Word(callSiteAddr));
      // must peek from kernel memory; mustn't use get1OrigInsn() here!
         
   assert(*callingInsnCurrInMemory == sparc_instr(sparc_instr::callandlink,
                                                  oldfn->getEntryAddr(),
                                                  callSiteAddr));
#elif defined(i386_unknown_linux2_4)
   char old_insn[5];
   unsigned nbytes = theKernelDriver.peek_block(&old_insn[0], callSiteAddr,
                                                5, false);
   if(nbytes != 5)
      assert(!"kernel read of insn bytes at callsite failed\n");

   const x86_instr *callingInsnCurrInMemory = new x86_instr(&old_insn[0]);
#elif defined(ppc64_unknown_linux2_4)
   const power_instr *callingInsnCurrInMemory = new power_instr(theKernelDriver.peek1Word(callSiteAddr));
   assert(*callingInsnCurrInMemory == power_instr(power_instr::branch,
                                                  oldfn->getEntryAddr() - callSiteAddr,
                                                  0,1));
#endif
	 
   callLauncher *theNewLauncher = new callLauncher(callSiteAddr,
                                               newfn->getEntryAddr(),
                                               callingInsnCurrInMemory
                                                  // restore back to this when
                                                  // all done
                                               );
   assert(theNewLauncher);
   return theNewLauncher;
}

void replaceFunctionCall::
installNow(callLauncher *theNewLauncher,
           bool verboseSummaryTiming,
           bool verboseIndividualTiming) {
   // Doesn't modify presentRequests[]

   if (presentRequests.size()) {
      // Overwriting a previous request, so we need to uninstrument the old and
      // instrument the new at the same time.
      instr_t *otherGuysCurrentInsn = theLauncher->createSplicingInsn();
      theNewLauncher->
         installOverwritingExisting(false, // not justtesting
                                    otherGuysCurrentInsn,
				       // other guy's currently installed insn
                                       // (useful for asserting)
                                    theLauncher->getWhenAllDoneRestoreToThisInsn(),
                                       // other guy's restore-to instruction
                                       // used for asserting (ours should be the same
                                       // or we weren't created correctly)
                                    (regset_t*)NULL,
                                    theKernelDriver,
                                    false // no verbose timing info
                                    );
      delete otherGuysCurrentInsn;
      theLauncher->unInstallAnotherAlreadyInstalled(false, // not justTesting
                                                    theKernelDriver,
                                                    false // no verbose timing info
                                                    );
      delete theLauncher;
      theLauncher = theNewLauncher;
   }
   else {
      // The easy case; we're not overwriting any previous replaceFn requests.
      assert(theLauncher == NULL);
      theLauncher = theNewLauncher;

      const mrtime_t startTime = verboseSummaryTiming ? getmrtime() : 0;

      theLauncher->
         installNotOverwritingExisting(false, // not justtesting
                                       (regset_t*)NULL,
                                       theKernelDriver,
                                       verboseIndividualTiming);
      
      if (verboseSummaryTiming) {
         const mrtime_t totalTime = getmrtime() - startTime;
         cout << "replaceFunctionCall::installNow took " << totalTime/1000
              << " usecs\n";
      }
   }
}


void replaceFunctionCall::
makeRequestAndInstallNow(client_connection *theClient,
                         unsigned reqid,
                         const function_t *newFn,
                         bool verboseSummaryInstallTiming,
                         bool verboseIndividualInstallTiming
                         ) {
   callLauncher *theNewLauncher = makeLauncher(newFn);
   
   // OK so we've created data structures for the new guy, but haven't 
   // installed them yet.  Time to install, with an eye towards uninstalling 
   // the old guy, if any, at the same time.
   installNow(theNewLauncher,
              verboseSummaryInstallTiming,
              verboseIndividualInstallTiming);
      // is smart enough to do the right thing depending on whether we're
      // overwriting an existing request.  Doesn't modify presentRequests[]

   presentRequests += make_pair(make_pair(theClient, reqid), newFn);
}

// --------------------

bool replaceFunctionCall::
removeRequestAndUninstallIfAppropriateNow(client_connection &theClient,
                                          unsigned reqid,
                                          bool verboseSummaryTiming,
                                          bool verboseIndividualTiming) {
   // uninstalls this requests, and, as appropriate, installs
   // the next one waiting in the queue.

   pdvector< pair<requester, const function_t*> >::iterator iter = presentRequests.begin();
   pdvector< pair<requester, const function_t*> >::iterator finish = presentRequests.end();
   for (; iter != finish; ++iter) {
      const pair<requester, const function_t*> &info = *iter;
      if (info.first.first == &theClient && info.first.second == reqid) {
         removeRequestAndUninstallIfAppropriateNow(iter,
                                                   verboseSummaryTiming,
                                                   verboseIndividualTiming);
         break;
      }
   }
   assert(iter != finish && "replaceFunctionCall reqid not found for removal");

   const bool result = (presentRequests.size() == 0);
   if (result) {
      assert(theLauncher == NULL);
      return true;
   }
   else {
      assert(theLauncher != NULL);
      return false;
   }
}

void replaceFunctionCall::unInstallWithNoReplacement(bool verboseSummaryTiming,
                                                 bool verboseIndividualTiming) {
   assert(presentRequests.size() == 1);

   const mrtime_t unInstallStartTime = verboseSummaryTiming ? getmrtime() : 0;

   // Remove the launcher
   theLauncher->
      unInstallNoOtherAlreadyInstalled(false, // not justTesting,
                                       theKernelDriver,
                                       verboseIndividualTiming);

   delete theLauncher;
   theLauncher = NULL; // required by dtor

   presentRequests.pop_back();
   assert(presentRequests.size() == 0);

   if (verboseSummaryTiming) {
      const mrtime_t unInstallTotalTime = getmrtime()-unInstallStartTime;
      cout << "replaceFunctionCall uninstall (no replacement) took "
           << unInstallTotalTime/1000 << " usecs.  NOTE that\n"
           << "some extra work may still be required, delayed for safety.\n";
   }
}

void replaceFunctionCall::
removeRequestAndUninstallIfAppropriateNow(
                   pdvector< pair<requester,const function_t*> >::iterator i,
                                          bool verboseSummaryTiming,
                                          bool verboseIndividualTiming) {
   const bool removingCurrentlyInstalledOne = (i==presentRequests.end()-1);
   
   if (!removingCurrentlyInstalledOne) {
      // Just slide items.  Can't swap with back and then pop_back(),
      // because we must preserve the ordering.
      while (i != presentRequests.end()-1)
         *i++ = *(i+1);
      presentRequests.pop_back(); // shrink by 1
   }
   else {
      // removing the currently installed request, so we must install a new one
      // if anything else in presentRequests[]
      assert(i == presentRequests.end() - 1);
      if (presentRequests.size() > 1) {
         installRequestReplacingExistingOneNow((i - 1)->second,
                                               verboseSummaryTiming,
                                               verboseIndividualTiming);
            // doesn't make changes to presentRequests[]
         presentRequests.pop_back();
      }
      else {
         unInstallWithNoReplacement(verboseSummaryTiming,
                                    verboseIndividualTiming);
         assert(presentRequests.size() == 0);
      }
   }
}

void replaceFunctionCall::
installRequestReplacingExistingOneNow(const function_t *newFn,
                                      bool verboseSummaryTiming,
                                      bool verboseIndividualTiming) {
   // doesn't update the vector presentRequests

   callLauncher *theNewLauncher = makeLauncher(newFn);
   
   // OK so we've created data structures for the new guy, but haven't 
   // installed them yet.  Time to install, with an eye towards 
   // uninstalling the old guy, if any, at the same time.
   installNow(theNewLauncher, verboseSummaryTiming, verboseIndividualTiming);
      // is smart enough to do the right thing depending on whether we're
      // overwriting an existing request.  Doesn't modify presentRequests[]
}
