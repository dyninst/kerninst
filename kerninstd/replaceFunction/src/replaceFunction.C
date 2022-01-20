// replaceFunction.C

#include "util/h/mrtime.h"
#include "replaceFunction.h"

replaceFunction::replaceFunction(const function_t &ioldfn,
                                 const moduleMgr &iModuleMgr,
                                 kernelDriver &iKernelDriver,
                                 SpringBoardHeap &iSpringBoardHeap) :
   theModuleMgr(iModuleMgr),
   theKernelDriver(iKernelDriver),
   theSpringBoardHeap(iSpringBoardHeap),
   oldfn(&ioldfn),
   // presentRequests left empty
   theLauncher(NULL) // only non-NULL when install()'d
   // patchedUpCallSites remains an empty vector until install()'d
{
    //avoid a minor memory leak..
    deadRegsAtStartOfOldFn = regset_t::getRegSet(regset_t::empty);
    *deadRegsAtStartOfOldFn = *theModuleMgr.getDeadRegsBeforeInsnAddr(*oldfn,
                                  oldfn->getEntryAddr(), false);  // no verbose
   
   // There is no need to install our callbacks (see .h file) until we have
   // install()'d.  We also do not set origInsnAtLauncher until installing!
}

replaceFunction::~replaceFunction() {
   // assert everything has been uninstalled

   assert(presentRequests.size() == 0);
   assert(theLauncher == NULL);
   assert(patchedUpCallSites.size() == 0);
   delete deadRegsAtStartOfOldFn;
}

// --------------------
   
void replaceFunction::kerninstdIsGoingDown() {
   // kerninstd is dying, undo changes NOW!

   if (theLauncher == NULL) {
      // nothing needed
      assert(patchedUpCallSites.size() == 0);
      assert(presentRequests.size() == 0);
      return;
   }

   assert(presentRequests.size() > 0);
   const pair<requester, prefs> &theRequest = presentRequests.back();

   assert(theLauncher);
   theLauncher->unInstallNoOtherAlreadyInstalled(false, // not justTesting,
                                                 theKernelDriver,
                                                 false // no verbose timing info
                                                 );

   delete theLauncher;
   theLauncher = NULL;

   // --------------------

   const prefs &thePrefs = theRequest.second;
   const function_t *newfn = thePrefs.first;
   const bool patchUpCallSitesToo = thePrefs.second;

   if (patchUpCallSitesToo)
      ; // can't assert patchedUpCallSites.size() > 0 cuz there in theory could be none
   else
      assert(patchedUpCallSites.size() == 0);

   pdvector<callLauncher*>::const_iterator iter = patchedUpCallSites.begin();
   pdvector<callLauncher*>::const_iterator finish = patchedUpCallSites.end();
   for (; iter != finish; ++iter) {
      callLauncher *theCallSiteLauncher = *iter;

      // call site should presently be calling the newfn's start addr:
      assert(theCallSiteLauncher->getTo() == newfn->getEntryAddr());

      theCallSiteLauncher->
         unInstallNoOtherAlreadyInstalled(false, // not justTesting
                                          theKernelDriver,
                                          false // no verbose timing info
                                          );

      delete theCallSiteLauncher;
      theCallSiteLauncher = NULL;
   }
   patchedUpCallSites.clear();
}

// --------------------

pair< launcher*, pdvector<callLauncher*> >
replaceFunction::makeLaunchers(const function_t *newfn,
                               bool replaceCallSitesToo) const {
#ifdef sparc_sun_solaris2_7
   const instr_t *insn = new sparc_instr(theKernelDriver.peek1Word(oldfn->getEntryAddr()));
#elif defined(i386_unknown_linux2_4)
   const instr_t *insn = new x86_instr(*(const x86_instr*)(oldfn->get1OrigInsn(oldfn->getEntryAddr())));
#elif defined(ppc64_unknown_linux2_4)
   const instr_t *insn = new power_instr(theKernelDriver.peek1Word(oldfn->getEntryAddr()));
#endif

   launcher *theNewLauncher = launcher::pickAnAnnulledLauncher(oldfn->getEntryAddr(),
                                                               newfn->getEntryAddr(),
                                                               insn,
                                                               theSpringBoardHeap);
   assert(theNewLauncher);

   pdvector<callLauncher*> theNewPatchedUpCallSites;
      // presently empty, and if preferences dictate so, may stay that way

   if (replaceCallSitesToo) {
      bool ignoreIsResultSorted;
      const pdvector<kptr_t> &theCallSitesToOldFn =
         theModuleMgr.getCallSitesTo(oldfn->getEntryAddr(),
                                     ignoreIsResultSorted);

      // Let us assert that all call sites to this function are being made
      // from correctly-parsed functions.  Perhaps this is too strong an
      // assertion to make, but for now, code in the loop below makes the same
      // checks (by calling findFn(), which asserts found).
      pdvector<kptr_t>::const_iterator iter = theCallSitesToOldFn.begin();
      pdvector<kptr_t>::const_iterator finish = theCallSitesToOldFn.end();
      for (; iter != finish; ++iter) {
         const kptr_t callSiteAddr = *iter;
         (void)theModuleMgr.findFn(callSiteAddr, false);
      }

      iter = theCallSitesToOldFn.begin(); // reset the search
      for (; iter != finish; ++iter) {
         const kptr_t theCallSite = *iter; // from ('to' is newfn->getEntryAddr())
         
         // We can assert that the instruction at this call site was a call to 
         // oldfn->getEntryAddr().  (But we must peek at its "current" value,
         // from memory, not as parsed via get1OrigInsn(), because that would
         // crash if this call site were replaced then replaced again!)

#ifdef sparc_sun_solaris2_7
         const sparc_instr *callingInsnCurrInMemory = new sparc_instr(theKernelDriver.peek1Word(theCallSite));
            // must peek from kernel memory; mustn't use get1OrigInsn() here!
         
         assert(*callingInsnCurrInMemory == sparc_instr(sparc_instr::callandlink,
							oldfn->getEntryAddr(),
							theCallSite));
#elif defined(i386_unknown_linux2_4)
         char old_insn[5];
         unsigned nbytes = theKernelDriver.peek_block(&old_insn[0], theCallSite,
						      5, false);
	 if(nbytes != 5)
            assert(!"kernel read of insn bytes at callsite failed\n");

	 const x86_instr *callingInsnCurrInMemory = new x86_instr(&old_insn[0]);
#elif defined(ppc64_unknown_linux2_4)
         const power_instr *callingInsnCurrInMemory = new power_instr(theKernelDriver.peek1Word(theCallSite));
#endif
	 
         theNewPatchedUpCallSites += new callLauncher(theCallSite,
                                                      newfn->getEntryAddr(),
                                                      callingInsnCurrInMemory
                                                         // restore back to this when
                                                         // all done
                                                      );

//         cout << "Created callLauncher from " << addr2hex(theCallSite)
//              << " to " << addr2hex(newfn->getEntryAddr()) << endl;
      }
   }

   return make_pair(theNewLauncher, theNewPatchedUpCallSites);
}

void replaceFunction::
installNow(launcher *theNewLauncher,
           const pdvector<callLauncher*> &theNewPatchedUpCallSites,
           bool verboseSummaryTiming,
           bool verboseIndividualTiming) {
   // Doesn't modify presentRequests[]

   if (presentRequests.size()) {
      // Overwriting a previous request, so we need to uninstrument the old and
      // instrument the new at the same time.
      const prefs &oldPrefs = presentRequests.back().second;

      if (patchedUpCallSites.size() > 0)
         assert(oldPrefs.second);

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
                                    deadRegsAtStartOfOldFn,
                                    theKernelDriver,
                                    false // no verbose timing info
                                    );
      delete otherGuysCurrentInsn;
      theLauncher->unInstallAnotherAlreadyInstalled(false, // not justTesting
                                                    theKernelDriver,
                                                    false // no verbose timing info
                                                    );
         // does a nop, though springboard launcher will de-allocate the springboard,
         // but that's about it.

      delete theLauncher;
      theLauncher = theNewLauncher;

      // We have replaced "theLauncher" (from start of old fn)
      // Now it is time to undo call site stuff, if any, from the old request
      // That is, calls to "oldfn" had been replaced with calls to "newfn".
      // Now it is time to replace calls from "oldfn" with calls to "newfn2".
      // Thus, the call sites are probably unchanged, if they were set at all

      if (patchedUpCallSites.size() > 0 && theNewPatchedUpCallSites.size() > 0)
         assert(theNewPatchedUpCallSites.size() == patchedUpCallSites.size());

      pdvector<callLauncher*>::iterator olditer = patchedUpCallSites.begin();
      pdvector<callLauncher*>::iterator oldfinish = patchedUpCallSites.end();
      for (; olditer != oldfinish; ++olditer) {
         callLauncher *oldCallLauncher = *olditer;

         const unsigned oldCallLauncherNdx = olditer - patchedUpCallSites.begin();

         callLauncher *newCallLauncher = theNewPatchedUpCallSites[oldCallLauncherNdx];
         assert(newCallLauncher);
         
         // assert that they match up!
         assert(newCallLauncher->getFrom() == oldCallLauncher->getFrom());

         instr_t *otherGuysCurrentInsn = oldCallLauncher->createSplicingInsn();
         newCallLauncher->
            installOverwritingExisting(false, // not justtesting
                                           otherGuysCurrentInsn,
                                          // other guy's currently installed insn
                                          // (useful for asserting)
                                       oldCallLauncher->getWhenAllDoneRestoreToThisInsn(),
                                          // used for asserting; ours should be the
                                          // same, or we weren't created correctly
                                       (regset_t*)NULL,
                                          // I happen to know that this is ignored
                                          // for a callLauncher
                                       theKernelDriver,
                                       false // no verbose timing info
                                       );
	 delete otherGuysCurrentInsn;
         
         oldCallLauncher->
            unInstallAnotherAlreadyInstalled(false, // not justtesting
                                             theKernelDriver,
                                             false // no verbose timing info
                                             );
      
         delete oldCallLauncher;
         oldCallLauncher=NULL;
      }
      patchedUpCallSites = theNewPatchedUpCallSites; // swap() would be quicker
         // may be a zero-element vector
   }
   else {
      // The easy case; we're not overwriting any previous replaceFn requests.
      assert(theLauncher == NULL);
      theLauncher = theNewLauncher;

      const mrtime_t startTime = verboseSummaryTiming ? getmrtime() : 0;

      theLauncher->
         installNotOverwritingExisting(false, // not justtesting
                                       deadRegsAtStartOfOldFn,
                                       theKernelDriver,
                                       verboseIndividualTiming);
      
      assert(patchedUpCallSites.size() == 0);
      patchedUpCallSites = theNewPatchedUpCallSites; // swap() would be quicker
      pdvector<callLauncher*>::const_iterator iter = patchedUpCallSites.begin();
      pdvector<callLauncher*>::const_iterator finish = patchedUpCallSites.end();
      for (; iter != finish; ++iter) {
         callLauncher *theCallSiteLauncher = *iter;
         assert(theCallSiteLauncher);

         theCallSiteLauncher->
            installNotOverwritingExisting(false, // not justtesting
                                          (regset_t*)NULL, 
					     // ignored for a callLauncher
                                          theKernelDriver,
                                          verboseIndividualTiming);
      }

      if (verboseSummaryTiming) {
         const mrtime_t totalTime = getmrtime() - startTime;
         cout << "replaceFunction::installNow took " << totalTime/1000
              << " usecs, with " << patchedUpCallSites.size()
              << " patched up call sites." << endl;
      }
   }
}


void replaceFunction::
makeRequestAndInstallNow(client_connection *theClient,
                         unsigned reqid,
                         const prefs &thePrefs, // new fn & replaceCallSitesToo flag
                         bool verboseSummaryInstallTiming,
                         bool verboseIndividualInstallTiming
                         ) {
   pair< launcher*, pdvector<callLauncher*> > theNewLaunchers =
      makeLaunchers(thePrefs.first, // new fn
                    thePrefs.second // replace call sites too?
                    );
   
   // OK so we've created data structures for the new guy, but haven't installed
   // them yet.  Time to install, with an eye towards uninstalling the old guy,
   // if any, at the same time.
   installNow(theNewLaunchers.first, theNewLaunchers.second,
              verboseSummaryInstallTiming,
              verboseIndividualInstallTiming);
      // is smart enough to do the right thing depending on whether we're
      // overwriting an existing request.  Doesn't modify presentRequests[]

   presentRequests += make_pair(make_pair(theClient, reqid), thePrefs);
}

// --------------------

bool replaceFunction::
removeRequestAndUninstallIfAppropriateNow(client_connection &theClient,
                                          unsigned reqid,
                                          bool verboseSummaryTiming,
                                          bool verboseIndividualTiming) {
   // uninstalls this requests, and, as appropriate, installs
   // the next one waiting in the queue.

   pdvector< pair<requester, prefs> >::iterator iter = presentRequests.begin();
   pdvector< pair<requester, prefs> >::iterator finish = presentRequests.end();
   for (; iter != finish; ++iter) {
      const pair<requester, prefs> &info = *iter;
      if (info.first.first == &theClient && info.first.second == reqid) {
         removeRequestAndUninstallIfAppropriateNow(iter,
                                                   verboseSummaryTiming,
                                                   verboseIndividualTiming);
         break;
      }
   }
   assert(iter != finish && "not found");

   const bool result = presentRequests.size() == 0;
   if (result) {
      assert(theLauncher == NULL);
      assert(patchedUpCallSites.size() == 0);
      return true;
   }
   else {
      assert(theLauncher != NULL);
      return false;
   }
}

void replaceFunction::unInstallWithNoReplacement(bool verboseSummaryTiming,
                                                 bool verboseIndividualTiming) {
   assert(presentRequests.size() == 1);

   const mrtime_t unInstallStartTime = verboseSummaryTiming ? getmrtime() : 0;

   pdvector<callLauncher*>::iterator iter = patchedUpCallSites.begin();
   pdvector<callLauncher*>::iterator finish = patchedUpCallSites.end();
   for (; iter != finish; ++iter) {
      callLauncher *theCallLauncher = *iter;

      theCallLauncher->
         unInstallNoOtherAlreadyInstalled(false, // not justTesting,
                                          theKernelDriver,
                                          verboseIndividualTiming);
      
      delete theCallLauncher;
      theCallLauncher = NULL; // help purify find mem leaks
   }
   patchedUpCallSites.clear(); // required by dtor

   // And finally we remove the "main" launcher (removing in the reverse order that
   // things were installed; perhaps not necessary)

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
      cout << "replaceFunction uninstall (no replacement) took "
           << unInstallTotalTime/1000 << " usecs.  But NOTE that"
           << endl 
           << "some extra work may still be required, delayed for safety." << endl;
   }
}

void replaceFunction::
removeRequestAndUninstallIfAppropriateNow(pdvector< pair<requester,prefs> >::iterator i,
                                          bool verboseSummaryTiming,
                                          bool verboseIndividualTiming) {
   const bool removingCurrentlyInstalledOne = (i==presentRequests.end()-1);
   
   if (!removingCurrentlyInstalledOne) {
      // Just slide items.  Can't swap with back and then pop_back(), because we
      // must preserve the ordering.
      while (i != presentRequests.end()-1)
         *i++ = *(i+1);
      presentRequests.pop_back(); // shrink by 1
   }
   else {
      // removing the currently installed request, so we must install a new one
      // if anything else in presentRequests[]
      assert(i == presentRequests.end() - 1);
      if (presentRequests.size() > 1) {
         installRequestReplacingExistingOneNow((i - 1)->second, // send prefs
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

void replaceFunction::
installRequestReplacingExistingOneNow(const prefs &thePrefs,
                                      bool verboseSummaryTiming,
                                      bool verboseIndividualTiming) {
   // doesn't update the vector presentRequests

   pair< launcher*, pdvector<callLauncher*> > theNewLaunchers =
      makeLaunchers(thePrefs.first, // newfn
                    thePrefs.second // flag: replaceCallSitesToo?
                    );
   
   // OK so we've created data structures for the new guy, but haven't installed
   // them yet.  Time to install, with an eye towards uninstalling the old guy,
   // if any, at the same time.
   installNow(theNewLaunchers.first, theNewLaunchers.second,
              verboseSummaryTiming, verboseIndividualTiming);
      // is smart enough to do the right thing depending on whether we're
      // overwriting an existing request.  Doesn't modify presentRequests[]
}

// --------------------

kptr_t replaceFunction::unPatch1Caller(kptr_t callSiteAddr) {
   // Find "callSiteAddr" among "patchedUpCallSites"
   
   pdvector<callLauncher*>::iterator iter = patchedUpCallSites.begin();
   pdvector<callLauncher*>::iterator finish = patchedUpCallSites.end();
   for (; iter != finish; ++iter) {
      callLauncher *theCallLauncher = *iter;

      if (theCallLauncher->getFrom() == callSiteAddr) {
         // match!

         const instr_t *origInsn = theModuleMgr.findFn(callSiteAddr, false).get1OrigInsn(callSiteAddr);
            // we'll undo back to this value

         kaddrdiff_t origCallOffset;
         if (!origInsn->isCallInstr(origCallOffset))
            assert(false);
         const kptr_t origCallDestination = callSiteAddr + origCallOffset;

         theCallLauncher->
            unInstallNoOtherAlreadyInstalled(false, // not justTesting
                                             theKernelDriver,
                                             false // no verbose timing info
                                             );

         delete theCallLauncher;
         theCallLauncher = NULL; // help purify find mem leaks

         *iter = patchedUpCallSites.back();
         patchedUpCallSites.pop_back();

         return origCallDestination;
      }
   }
   assert(false && "not found");
}
