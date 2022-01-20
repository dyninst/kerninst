// instrumentationMgr.C

#include "instrumentationMgr.h"
#include "moduleMgr.h"
#include "kernelDriver.h"
#include "patchHeap.h"
#include "SpringBoardHeap.h"
#include "util/h/hashFns.h" // stringHash()
#include "RTSymtab.h" // lookup1NameInSelf, lookup1NameInKernel
#include "util/h/out_streams.h"
#include "util/h/hashFns.h"
#include "mappedKernelSymbols.h"

#include <algorithm> // STL's sort()

extern moduleMgr *global_moduleMgr;
extern kernelDriver *global_kernelDriver;
extern mappedKernelSymbols *global_mappedKernelSymbols;
extern patchHeap *global_patchHeap;
extern SpringBoardHeap *global_springboardHeap;
extern bool replaceFunctionSummaryVerboseTimings;
extern bool replaceFunctionIndividualVerboseTimings;

instrumentationMgr::instrumentationMgr() :
   theLaunchSites(addrHash4),
   replacedFnsMap(addrHash4),
   replacementFnsMap(addrHash4),
   replacedCallsMap(addrHash4)
{
}

instrumentationMgr::~instrumentationMgr() {
   // kerninstdIsGoingDownNow() should already have been called:
   assert(theLaunchSites.size() == 0);
   assert(replacedFnsMap.size() == 0);
   assert(replacementFnsMap.size() == 0);
   assert(replacedCallsMap.size() == 0);
}

void instrumentationMgr::kerninstdIsGoingDownNow() {
   dictionary_hash<kptr_t, LaunchSite *>::const_iterator lsiter = 
	   theLaunchSites.begin();
   dictionary_hash<kptr_t, LaunchSite *>::const_iterator lsfinish = 
	   theLaunchSites.end();
   while (lsiter != lsfinish) {
      kptr_t addr = lsiter.currkey();
      LaunchSite *theLaunchSite = lsiter.currval();

      dout << "Doing emergency unsplice of instrumentation code at "
           << addr2hex(addr) << endl;
      
      theLaunchSite->kerninstdIsGoingDown(addr);

      delete theLaunchSite;
      theLaunchSite = NULL; // help purify find mem leaks
      
      ++lsiter;
   }
   theLaunchSites.clear();

   dictionary_hash<kptr_t, replaceFunction*>::const_iterator rfiter =
      replacedFnsMap.begin();
   dictionary_hash<kptr_t, replaceFunction*>::const_iterator rffinish =
      replacedFnsMap.end();
   for (; rfiter != rffinish; ++rfiter) {
      replaceFunction *theReplaceFunction = rfiter.currval();
      (void)replacementFnsMap.get_and_remove(theReplaceFunction->getPresentlyInstalledNewFn()->getEntryAddr()); // is this right???
      
      theReplaceFunction->kerninstdIsGoingDown();
      
      delete theReplaceFunction;
      theReplaceFunction = NULL; // help purify find mem leaks
   }
   replacedFnsMap.clear();
   assert(replacementFnsMap.size() == 0);

   dictionary_hash<kptr_t, replaceFunctionCall*>::const_iterator rciter =
      replacedCallsMap.begin();
   dictionary_hash<kptr_t, replaceFunctionCall*>::const_iterator rcfinish =
      replacedCallsMap.end();
   for (; rciter != rcfinish; ++rciter) {
      replaceFunctionCall *theReplacedCall = rciter.currval();
      theReplacedCall->kerninstdIsGoingDown();
      delete theReplacedCall;
      theReplacedCall = NULL; // help purify find mem leaks
   }
   replacedCallsMap.clear();
}

void instrumentationMgr::clientIsGoingDownNow(client_connection &theClient) {
   pdvector< pair<unsigned, kptr_t> > instrumentationStillActiveThisClient =
      theClient.getActiveInstrumentationInfo();
   // returns a vector of [splice-id, spliceAddr], where splice-id is 
   // unique for this client

   const bool was_active_instrumentation =
      instrumentationStillActiveThisClient.size() > 0;

   for (pdvector< pair<unsigned, kptr_t> >::const_iterator iter =
           instrumentationStillActiveThisClient.begin();
        iter != instrumentationStillActiveThisClient.end(); ++iter) {
      unsigned uniqSpliceReqId = iter->first;
      kptr_t spliceAddr = iter->second;

      assert(spliceAddr == theClient.spliceReqId2SpliceAddr(uniqSpliceReqId));
      
      // do something similar to method unSplice() of this class.

      LaunchSite *theLaunchSite = NULL;
      if (!theLaunchSites.find(spliceAddr, theLaunchSite))
         assert(false);
      
      assert(theLaunchSite);

      // true --> reinstrument now
      if (theLaunchSite->removeCodeSnippet(theClient, uniqSpliceReqId, true)) {
         // there's nothing left at this launch site; orig code has been
	 // put back.  (If sterilized due to competing replaceFunction, nothing
	 // would have been installed to begin with)
         delete theLaunchSite;
         theLaunchSite = NULL;
         theLaunchSites.undef(spliceAddr);
      }

      theClient.destroySpliceReqId(uniqSpliceReqId);
      
      // Here's where we'd normally send the client an unsplice ack, but since it's
      // probably already died, there doesn't seem to be much use in that!
   }
   assert(theClient.getActiveInstrumentationInfo().size() == 0);

   // Now remove all replaceFunction's that were put in place by this client
   const pdvector< pair<unsigned, pair<kptr_t, kptr_t > > >
      stillActiveReplaceFnsByThisClient = theClient.getAllReplaceFnRequests();
     // .first: reqid .second: enough to get a replaceFunction*

   for (pdvector< pair<unsigned, pair<kptr_t, kptr_t> > >::const_iterator iter =
           stillActiveReplaceFnsByThisClient.begin();
        iter != stillActiveReplaceFnsByThisClient.end(); ++iter) {
      const unsigned replaceFnReqId = iter->first;
      const kptr_t replacedFnAddr = iter->second.first;
      const kptr_t replacementFnAddr = iter->second.second;

      replaceFunction *theReplaceFunction = replacedFnsMap.get(replacedFnAddr);
      assert(theReplaceFunction);
      assert(theReplaceFunction == replacementFnsMap.get(replacementFnAddr));

      assert(theClient.replaceFnReqId2ReplacedFnEntryAddr(replaceFnReqId) ==
             replacedFnAddr);
      assert(theClient.replaceFnReqId2ReplacementFnEntryAddr(replaceFnReqId) ==
             replacementFnAddr);
      
      // Now un-replaceFunction...
      const bool canFryReplaceFunction =
      theReplaceFunction->removeRequestAndUninstallIfAppropriateNow(theClient,
                                                                    replaceFnReqId,
                                                                    replaceFunctionSummaryVerboseTimings,
                                                                    replaceFunctionIndividualVerboseTimings);

      theClient.destroyReplaceFnReqId(replaceFnReqId);

      // ...and reflect the un-replaceFunction in the call graph:
      global_moduleMgr->updateCallGraphForUnReplaceFunction(replacedFnAddr,
                                                            replacementFnAddr);
      
      if (canFryReplaceFunction) {
         (void)replacedFnsMap.get_and_remove(replacedFnAddr);
         (void)replacementFnsMap.get_and_remove(replacementFnAddr);
         delete theReplaceFunction;
         theReplaceFunction = NULL; // help purify find mem leaks
      }
   }
   assert(theClient.getAllReplaceFnRequests().size() == 0);
   
   // --------------------

   if (was_active_instrumentation) {
      // We just uninstrumented some code.  
      // Wait a while for safety before continuing.
      dout << "Removed some leftover instrumentation; pausing 3 secs to ensure it finishes executing" << endl;
      dout << "before freeing data" << endl;

      sleep(3);
   }

   theClient.destroyDownloadedToKernelCode(); // may also remove a parsed "function"
   theClient.destroyDownloadedToKerninstdCode();

   // Any maining parseFunction requests (some may have already been removed by virtue
   // of removing downloadedToKernel code).
   theClient.destroyAllParsedKernelCode();

   theClient.destroySampleRequests();

   // --------------------

   pdvector<pair<pdstring, unsigned> > allKernelSymMMaps = theClient.getAllKernelSymMMaps();
   for (pdvector<pair<pdstring, unsigned> >::const_iterator iter =
           allKernelSymMMaps.begin(); iter != allKernelSymMMaps.end(); ++iter) {
      const pair<pdstring, unsigned> &info = *iter;
      
      const pdstring &symName = info.first;
      const unsigned nbytes = info.second;

      global_mappedKernelSymbols->unmap_from_readonly(symName, nbytes);
   }
   theClient.destroyKernelSymMMaps();
   
   // Allocated (but unmapped) kernel memory:
   theClient.freeAllAllocatedUnmappedKernelMemory();

   // Allocated & mapped (into kerninstd space) kernel memory:
   theClient.freeAllAllocatedMappedKernelMemory();

   // Allocated kerninstd-only memory:
   theClient.freeAllAllocatedKerninstdMemory();

   // Last thing to clean up is any vtimers the client may have added
   // to the in-kernel array
   global_kernelDriver->clearClientState();
}

LaunchSite* instrumentationMgr::getLaunchSiteForSplice(client_connection &theClient,
                                                       unsigned reqid,
                                                       kptr_t spliceAddr) {
   // Create the launch site if it doesn't already exist:
   LaunchSite *theLaunchSite = NULL;
   if (!theLaunchSites.find(spliceAddr, theLaunchSite)) {
      const moduleMgr &theModuleMgr = *global_moduleMgr;
      kernelDriver &theKernelDriver = *global_kernelDriver;
      patchHeap &thePatchHeap = *global_patchHeap;
      SpringBoardHeap &theSpringBoardHeap = *global_springboardHeap;

      // We should sterilize if the function encompassing "spliceAddr" is presently
      // being replaced.
      const function_t &fnEncompassingSpliceAddr = theModuleMgr.findFn(spliceAddr,
                                                                       false);
         // false --> not startOnly

      const bool fnEncompassingSpliceAddrIsPresentlyReplaced = 
         replacedFnsMap.defines(fnEncompassingSpliceAddr.getEntryAddr());
      
      const bool sterilizedByCompetingReplaceFn =
         fnEncompassingSpliceAddrIsPresentlyReplaced;
      
      theLaunchSite = new LaunchSite(spliceAddr,
                                     sterilizedByCompetingReplaceFn,
                                     theModuleMgr, theKernelDriver,
                                     thePatchHeap, theSpringBoardHeap);
      
      theLaunchSites.set(spliceAddr, theLaunchSite);
   }

   assert(theLaunchSite);

   // Remember this reqid
   theClient.hereIsANewSpliceReqId(reqid, spliceAddr);

   return theLaunchSite;
}

// --------------------

static const function_t &findFn(kptr_t fnEntryAddr) {
   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   return theModuleMgr.findFn(fnEntryAddr, true); // true --> startOnly
}

void instrumentationMgr::replaceAFunction(client_connection &theClient,
                                          unsigned reqid,
                                          kptr_t oldFunctionEntryAddr,
                                          kptr_t newFunctionEntryAddr,
                                          bool replaceCallSitesToo) {
   // can't call this method "replaceFunction" that that name's in use
   // (another class)
   assert(oldFunctionEntryAddr != 0 && newFunctionEntryAddr != 0);
   if (replacedFnsMap.defines(oldFunctionEntryAddr)) {
      // oldFn was already replaced to get to replacedFnsMap[oldFunctionEntryAddr].
      // We're going to change that to get to newFunctionEntryAddr, while
      // keeping the old request around so that we can go back to it if
      // it later becomes possible.

      replaceFunction *theReplaceFn = replacedFnsMap.get(oldFunctionEntryAddr);

      const kptr_t priorReplacementAddr =
         theReplaceFn->getPresentlyInstalledNewFn()->getEntryAddr();
      
      cout << "instrumentationMgr::replaceFunction() -- function at "
           << addr2hex(oldFunctionEntryAddr) << endl
	   << "was already being replaced by one at " 
	   << addr2hex(priorReplacementAddr) << endl
	   << "Now changing its destination to " 
	   << addr2hex(newFunctionEntryAddr) << endl;

      const moduleMgr &theModuleMgr = *global_moduleMgr;
      const function_t &theNewFn = theModuleMgr.findFn(newFunctionEntryAddr,
						       true);

      theReplaceFn->makeRequestAndInstallNow(&theClient,
                                             reqid,
                                             make_pair(&theNewFn, replaceCallSitesToo),
                                             replaceFunctionSummaryVerboseTimings,
                                             replaceFunctionIndividualVerboseTimings);

      // The global call graph now needs to be updated.  In particular "oldFn"
      // will appear to make (among other calls) calls to some previous replacement
      // function.  We need to change those calls (and only those calls; oldFn may
      // have made other calls) to be calls to "newFn".
      global_moduleMgr->updateCallGraphForNewReplaceFunctionDest
         (oldFunctionEntryAddr, // the function being replaced
          priorReplacementAddr, // it used to be replaced by this
          newFunctionEntryAddr // and it's now being replaced by this
          );
      
      return;
   }

   if (replacedFnsMap.defines(newFunctionEntryAddr)) {
      // before: new --> new2
      // Now we're trying to install the replacement: old --> new
      // Clearly it would make more sense for old to jump directly to new2
      // So do: old --> new2.  Of course we leave that other new --> new2 alone.
      
      const replaceFunction *existingReplaceFn=replacedFnsMap.get(newFunctionEntryAddr);
         // new --> new2
      const function_t *newFn2 = existingReplaceFn->getPresentlyInstalledNewFn();

      cout << "instrumentationMgr::replaceFunction() -- request to replace\n" 
	   << addr2hex(oldFunctionEntryAddr) << " with " 
	   << addr2hex(newFunctionEntryAddr) << endl
	   << "has been changed to " 
	   << addr2hex(newFn2->getEntryAddr()) << endl
	   << "Since " << addr2hex(newFunctionEntryAddr)
	   << " had already been replaced with " 
	   << addr2hex(newFn2->getEntryAddr()) << endl;

      // re-submit the request:
      replaceAFunction(theClient, reqid, oldFunctionEntryAddr,
                       newFn2->getEntryAddr(), replaceCallSitesToo);
      return;
   }
   
   // Regular processing; neither "old" nor "new" functions were replaced before now.
   const function_t &oldFn  = findFn(oldFunctionEntryAddr);
   const function_t &newFn  = findFn(newFunctionEntryAddr);
   
   replaceFunction *newReplaceFn = new replaceFunction(oldFn,
                                                       *global_moduleMgr,
                                                       *global_kernelDriver,
                                                       *global_springboardHeap);
   assert(newReplaceFn);

   replacedFnsMap.set(oldFunctionEntryAddr, newReplaceFn);
   replacementFnsMap.set(newFunctionEntryAddr, newReplaceFn);

   // Let the client know:
   theClient.hereIsANewReplaceFnId(reqid, oldFunctionEntryAddr,
                                   newFunctionEntryAddr);
   
   newReplaceFn->makeRequestAndInstallNow(&theClient, reqid,
                                      make_pair(&newFn, replaceCallSitesToo),
                                      replaceFunctionSummaryVerboseTimings,
                                      replaceFunctionIndividualVerboseTimings);

   // And update the global call graph.
   global_moduleMgr->updateCallGraphForReplaceFunction
      (oldFunctionEntryAddr, // function being replaced; change calls to here...
       newFunctionEntryAddr // ...with calls to here
       );
}

void instrumentationMgr::
unReplaceAFunction(client_connection &theClient,
                   unsigned reqid) {
   const kptr_t replacedFnEntryAddr =
      theClient.replaceFnReqId2ReplacedFnEntryAddr(reqid);
   const kptr_t replacementFnEntryAddr =
      theClient.replaceFnReqId2ReplacementFnEntryAddr(reqid);

   replaceFunction *theReplaceFn = replacedFnsMap.get(replacedFnEntryAddr);
   const bool canBeGutted =
      theReplaceFn->removeRequestAndUninstallIfAppropriateNow(theClient, reqid,
                                     replaceFunctionSummaryVerboseTimings,
                                     replaceFunctionIndividualVerboseTimings);

   // ...and don't forget to update the call graph to reflect the changes
   // (possible bug: what if the unreplace didn't do anything, because it wasn't
   // the currently installed request.  We're probably going to get rid of that
   // 'feature' of replaceFunction soon anyway)
   global_moduleMgr->updateCallGraphForUnReplaceFunction(replacedFnEntryAddr,
                                                         replacementFnEntryAddr);
      // IMPORTANT CALL GRAPH NOTE: Although the above will ensure that now, no one
      // makes calls to "replacementFnEntryAddr", "replacementFnEntryAddr" will
      // still be in the call graph insofar as IT MAKES CALLS TO OTHER FOLKS.
   // ... and along the same lines, let's say that it made calls to some function
   // that's presently being replaced.  The replaceFunction structure for that callee
   // (not "theReplaceFn", above) needs to be informed that a certain someone is no
   // longer is calling it.  [NO I THINK THIS SHOULD BE HANDLED WHEN AND IF THE
   // FUNCTION BEING UNREPLACED HERE IS FRIED.  UNTIL THEN IT MIGHT BE REUSED, AT LEAST
   // IN THEORY.]

   if (canBeGutted) {
      (void)replacedFnsMap.get_and_remove(replacedFnEntryAddr);
      (void)replacementFnsMap.get_and_remove(replacementFnEntryAddr);
      delete theReplaceFn;
   }
   
   theClient.destroyReplaceFnReqId(reqid);
}

void instrumentationMgr::replaceAFunctionCall(client_connection &theClient,
                                              unsigned reqid,
                                              kptr_t callSiteAddr,
                                              kptr_t newFunctionEntryAddr) {
   // can't call this method "replaceFunctionCall" since that names a class
   assert(callSiteAddr != 0 && newFunctionEntryAddr != 0);
   if (replacedCallsMap.defines(callSiteAddr)) {
      // callsite already replaced to get to replacedCallsMap[callSiteAddr].
      // We're going to change that to get to newFunctionEntryAddr, while
      // keeping the old request around so that we can go back to it if
      // it later becomes possible.

      replaceFunctionCall *theReplacedCall = replacedCallsMap.get(callSiteAddr);

      const kptr_t priorReplacementAddr =
         theReplacedCall->getPresentlyInstalledNewFn()->getEntryAddr();
      
      cout << "instrumentationMgr::replaceFunctionCall() -- call at "
           << addr2hex(callSiteAddr)
	   << "\nwas already replaced to call function at " 
	   << addr2hex(priorReplacementAddr)
	   << "\nNow changing its destination to " 
	   << addr2hex(newFunctionEntryAddr) << endl;

      const moduleMgr &theModuleMgr = *global_moduleMgr;
      const function_t &theNewFn = theModuleMgr.findFn(newFunctionEntryAddr,
						       true);

      theReplacedCall->makeRequestAndInstallNow(&theClient,
                                                reqid,
                                                &theNewFn,
                                      replaceFunctionSummaryVerboseTimings,
                                      replaceFunctionIndividualVerboseTimings);

      // The global call graph now needs to be updated.
      global_moduleMgr->updateCallGraphForReplaceCall
         (callSiteAddr, // the call being replaced
          priorReplacementAddr, // call dest is currently this
          newFunctionEntryAddr // and it's now being replaced by this
          );
      
      return;
   }

   // Regular processing; callsite not replaced before now.
   const moduleMgr &theModuleMgr = *global_moduleMgr;
   const function_t &callFn = theModuleMgr.findFn(callSiteAddr,
                                                  false); // not start
   instr_t *callInsn = callFn.get1OrigInsn(callSiteAddr);
   kptr_t oldDestAddr = 0;
   if(! callInsn->isCallToFixedAddress(callSiteAddr, oldDestAddr))
      assert(!"instrumentationMgr::replaceAFunctionCall() - callsite not found");
   const function_t &oldFn  = findFn(oldDestAddr);
   const function_t &newFn  = findFn(newFunctionEntryAddr);
   
   replaceFunctionCall *newReplacedCall = new replaceFunctionCall(callSiteAddr,
                                                                  oldFn,
                                                      *global_moduleMgr,
                                                      *global_kernelDriver,
                                                      *global_springboardHeap);
   assert(newReplacedCall);

   replacedCallsMap.set(callSiteAddr, newReplacedCall);

   // Let the client know:
   theClient.hereIsANewReplaceCallId(reqid, callSiteAddr,
                                     newFunctionEntryAddr);
   
   newReplacedCall->makeRequestAndInstallNow(&theClient, reqid, &newFn,
                                      replaceFunctionSummaryVerboseTimings,
                                      replaceFunctionIndividualVerboseTimings);

   // And update the global call graph.
   global_moduleMgr->updateCallGraphForReplaceCall
      (callSiteAddr, // callsite being replaced
       oldDestAddr, // call dest is currently this 
       newFunctionEntryAddr // call dest will change to this
       );
}

void instrumentationMgr::unReplaceAFunctionCall(client_connection &theClient,
                                                unsigned reqid) {
   const kptr_t replacedCallSiteAddr =
      theClient.replaceCallReqId2ReplacedCallSiteAddr(reqid);
   const kptr_t replacedCalleeEntryAddr =
      theClient.replaceCallReqId2ReplacedCalleeEntryAddr(reqid);

   replaceFunctionCall *theReplacedCall = replacedCallsMap.get(replacedCallSiteAddr);
   const bool canBeGutted =
      theReplacedCall->removeRequestAndUninstallIfAppropriateNow(theClient, 
                                                                 reqid,
                                      replaceFunctionSummaryVerboseTimings,
                                      replaceFunctionIndividualVerboseTimings);

   // ...and don't forget to update the call graph to reflect the changes
   global_moduleMgr->updateCallGraphForReplaceCall(replacedCallSiteAddr,
            replacedCalleeEntryAddr, // current callee addr
            theReplacedCall->getOldFn()->getEntryAddr()); // orig callee addr

   if (canBeGutted) {
      (void)replacedCallsMap.get_and_remove(replacedCallSiteAddr);
      delete theReplacedCall;
   }
   
   theClient.destroyReplaceCallReqId(reqid);
}

// --------------------

void instrumentationMgr::
splice_preinsn(client_connection &theClient, unsigned reqid,
               kptr_t spliceAddr,
               unsigned conservative_numInt32RegsNeeded,
               unsigned conservative_numInt64RegsNeeded,
               // The code, if no save/restore is needed:
               const relocatableCode_t *relocatable_code_ifnosave,
               // The code, if a save/restore is needed:
               const relocatableCode_t *relocatable_code_ifsave) {
   LaunchSite *theLaunchSite = getLaunchSiteForSplice(theClient, reqid, 
						      spliceAddr);
      // will not return NULL, not even if spliceAddr is within a replaced function.
      // This is on purpose: we want to keep around all splice info, since the
      // replaceFn might be undone at a later time.
   
   theLaunchSite->addCodeSnippetPreInsn(theClient, reqid,
                                        conservative_numInt32RegsNeeded,
                                        conservative_numInt64RegsNeeded,
                                        relocatable_code_ifnosave,
                                        relocatable_code_ifsave,
                                        true); // true --> re-instrument now
}

void instrumentationMgr::
splice_prereturn(client_connection &theClient, unsigned reqid,
                 kptr_t spliceAddr,
                 unsigned conservative_numInt32RegsNeeded,
                 unsigned conservative_numInt64RegsNeeded,
                 // The code, if no save/restore is needed:
                 const relocatableCode_t *irelocatable_code_prereturn_ifnosave,
                 const relocatableCode_t *irelocatable_code_prereturn_ifsave,
                 const relocatableCode_t *irelocatable_code_preinsn_ifnosave,
                 const relocatableCode_t *irelocatable_code_preinsn_ifsave) {
   LaunchSite *theLaunchSite = getLaunchSiteForSplice(theClient, reqid, 
						      spliceAddr);
      // will not return NULL, not even if spliceAddr is within a replaced function.
      // This is on purpose: we want to keep around all splice info, since the
      // replaceFn might be undone at a later time.

   theLaunchSite->addCodeSnippetPreReturn(theClient, reqid,
                                          conservative_numInt32RegsNeeded,
                                          conservative_numInt64RegsNeeded,
                                          irelocatable_code_prereturn_ifnosave,
                                          irelocatable_code_prereturn_ifsave,
                                          irelocatable_code_preinsn_ifnosave,
                                          irelocatable_code_preinsn_ifsave,
                                          true); // true --> re-instrument now
}

void instrumentationMgr::unSplice(client_connection &theClient,
                                  unsigned spliceReqId) {
   // Note that this routine does not do anything special to check and see
   // if we have already been sterilized due to a competing replaceFn.  It doesn't
   // need to, and in any event, we at least want to proceed with removing some
   // meta-data.

   kptr_t spliceAddr = theClient.spliceReqId2SpliceAddr(spliceReqId);

   LaunchSite *theLaunchSite = theLaunchSites.get(spliceAddr);
   assert(theLaunchSite);

   if (theLaunchSite->removeCodeSnippet(theClient, spliceReqId, true)) {
      // true --> re-instrument now (does no re-instrumenting if already sterilized due
      // to a competing replaceFn.

      // there's nothing left at this launch site.  All orig code has been put back.
      // Fry this launch site.
      delete theLaunchSite;
      theLaunchSite = NULL;
      theLaunchSites.undef(spliceAddr);
   }

   theClient.destroySpliceReqId(spliceReqId);
}

void instrumentationMgr::
requestReadOnlyMMapOfKernelSymIntoKerninstd(client_connection &theClient,
                                            unsigned reqid, const pdstring &symName,
                                            unsigned nbytes) {
   void *result_ptr = global_mappedKernelSymbols->map_for_readonly(symName, nbytes);
   dptr_t result = (dptr_t)result_ptr;

   if (result != 0)
      theClient.noteMMapOfAKernelSymIntoKerninstd(reqid, symName, nbytes);

   theClient.mMapOfKernelSymIntoKerninstdResponse(reqid, result);
}

void instrumentationMgr::
unmapKernelSymInKerninstd(client_connection &theClient, unsigned reqid) {
   pair<pdstring, unsigned> info = theClient.getMMapOfAKernelSymInKerninstdInfo(reqid);

   global_mappedKernelSymbols->unmap_from_readonly(info.first, info.second);
}

// -------------------------------------------------------------------------
// Allocating kernel memory
// -------------------------------------------------------------------------

void instrumentationMgr::allocateUnmappedKernelMemory(client_connection &theClient,
                                                      unsigned reqid,
                                                      unsigned nbytes,
                                                      bool tryForNucleusText) {
   theClient.allocateUnmappedKernelMemory(reqid, nbytes,
                                          tryForNucleusText);
}

kptr_t
instrumentationMgr::queryAllocatedUnmappedKernelMemoryAddr(client_connection &theClient,
                                                           unsigned reqid) {
   return theClient.queryAllocatedUnmappedKernelMemoryAddr(reqid);
}

void instrumentationMgr::freeAllocatedUnmappedKernelMemory(client_connection &theClient,
                                                           unsigned reqid) {
   theClient.freeAllocatedUnmappedKernelMemory(reqid);
}

// -------------------------------------------------------------------------
// Allocating kerninstd-only memory
// -------------------------------------------------------------------------

void instrumentationMgr::allocateKerninstdMemory(client_connection &theClient,
                                                 unsigned reqid, unsigned nbytes) {
   theClient.allocateKerninstdMemory(reqid, nbytes);
}

void *
instrumentationMgr::queryAllocatedKerninstdMemoryAddr(client_connection &theClient,
                                                      unsigned reqid) {
   return theClient.queryAllocatedKerninstdMemoryAddr(reqid);
}

void instrumentationMgr::freeKerninstdMemory(client_connection &theClient,
                                             unsigned reqid) {
   theClient.freeKerninstdMemory(reqid);
}



// ---------------------------------------------------------------------------
// Allocating kernel memory that is also mapped into kerninstd's address space
// ---------------------------------------------------------------------------

void instrumentationMgr::allocateMappedKernelMemory(client_connection &theClient,
                                                    unsigned reqid,
                                                    unsigned inbytes,
                                                    bool tryForNucleusText) {
   theClient.allocateMappedKernelMemory(reqid, inbytes,
                                        tryForNucleusText);
}

kptr_t
instrumentationMgr::
queryAllocatedMappedKernelMemoryAddrInKernel(client_connection &theClient,
                                             unsigned reqid) {
   return theClient.queryAllocatedMappedKernelMemoryAddrInKernel(reqid);
}

void *
instrumentationMgr::
queryAllocatedMappedKernelMemoryAddrInKerninstd(client_connection &theClient,
                                                unsigned reqid) {
   return theClient.queryAllocatedMappedKernelMemoryAddrInKerninstd(reqid);
}

void instrumentationMgr::freeMappedKernelMemory(client_connection &theClient,
                                                unsigned reqid) {
   theClient.freeAllocatedMappedKernelMemory(reqid);
}



// Downloading into the kernel:

void instrumentationMgr::
downloadToKernelAndParse(client_connection &theClient,
                         unsigned reqid,
                         const pdstring &modName,
                         const pdstring &modDescriptionIfNew,
                         const pdstring &fnName,
                         const relocatableCode_t *theCode,
                         unsigned entry_chunk_ndx,
                         unsigned chunkNdxContainingDataIfAny,
                            // -1U if none
                         bool tryForNucleus
                         ) {
   // This is the vanilla version; just a single "function" is being downloaded
   theClient.downloadToKernelAndParse(reqid, modName, modDescriptionIfNew,
                                      fnName, theCode, 
				      entry_chunk_ndx,
                                      chunkNdxContainingDataIfAny,
                                      tryForNucleus);
}

void instrumentationMgr::
downloadToKernelAndParse(client_connection &theClient,
                         unsigned reqid,
                         const pdvector<downloadToKernelFn> &fns,
                         const pdvector< pair<unsigned,unsigned> > &emitOrdering,
                         bool tryForNucleus
   ) {
   // This is the deluxe version.
   theClient.downloadToKernelAndParse(reqid, fns, emitOrdering, tryForNucleus);
}

void instrumentationMgr::
downloadToKernel(client_connection &theClient,
                 unsigned reqid,
                 const relocatableCode_t *theCode,
                 unsigned entry_chunk_ndx,
                 bool tryForNucleus) {
   theClient.downloadToKernel(reqid, theCode, entry_chunk_ndx, tryForNucleus);
}

static bool addrIsWithinAnyGivenFn(kptr_t addr,
                                   const pdvector<const function_t *> &fns) {
   pdvector<const function_t *>::const_iterator iter = fns.begin();
   pdvector<const function_t *>::const_iterator finish = fns.end();
   for (; iter != finish; ++iter) {
      const function_t *fn = *iter;
      
      if (fn->containsAddr(addr))
         return true;
   }

   return false;
}

static void removeCallsFromCertainFns(pdvector<kptr_t> &remainingCallsTo,
                                      const pdvector<const function_t *> &fns) {
   pdvector<kptr_t>::iterator iter = remainingCallsTo.begin();
   for (; iter != remainingCallsTo.end(); ) {
      // yes, we must explicitly re-read remainingCallsTo.end() each time through
      // the loop, since it may change.
      const kptr_t callSite = *iter;
         // this insn contains a "call"

      if (addrIsWithinAnyGivenFn(callSite, fns)) {
         // remove from the vector
         *iter = remainingCallsTo.back();
         remainingCallsTo.pop_back();

         // do *NOT* bump up "iter" in this case
      }
      else
         ++iter;
   }
}

bool instrumentationMgr::
remove1DownloadedToKernel_preliminary(kptr_t entryAddr,
                                      const function_t *parsedFnOrNULL,
                                      const pdvector<const function_t *> &allFnsAboutToBeRemoved
                                      ) {
   // Makes this fn OK to remove.  Returns true iff successful, false if it's NOT
   // OK to remove this fn.

   // This helper routine does the following:
   // 1) Verifies that this fn isn't serving as a replacement for some other fn
   //    (because then removing it would be rather hazardous!).  Returns false
   //    if not OK to remove.
   // 2) Checks to see if anyone is still calling this fn.  If so then we return
   //    false, because it would not be safe to remove such a fn.  Exceptions to this
   //    rule are if all calls are from functions within "allFnsAboutToBeRemoved".
   // 3) Checks our callees.  Let's say we are calling "bar'", which is serving as
   //    a replacement for "bar".  Then there are replaceFunction structures that 
   //    note we are calling "bar'" (useful structures in general, since should "bar"
   //    ever get un-replaced, we'd normally like to change this call site from
   //    calling "bar'" to calling "bar").  Anyway, in such a situation, what we'll
   //    do is change the call site (a little silly, since the call site is about to
   //    become fried code, but it's harmless) and (more usefully, in this case)
   //    update the "replaceFunction" structure to let it know that "bar'" has one
   //    less callee that'll need to be changed should "bar" ever become un-replaced.

   if (parsedFnOrNULL)
      assert(parsedFnOrNULL->getEntryAddr() == entryAddr);
   
   if (replacementFnsMap.defines(entryAddr)) {
      cout << "WARNING: tried to remove a downloaded-to-kernel fn that's still" << endl;
      cout << "serving as a replacement function!!! Ignoring remove request!!!"
           << endl;
      return false;
   }

   bool ignore_sorted;
   pdvector<kptr_t> remainingCallsTo =
      global_moduleMgr->getCallSitesTo(entryAddr, ignore_sorted);
      // not made a const vector since we're going to be shrinking it
   removeCallsFromCertainFns(remainingCallsTo, allFnsAboutToBeRemoved);
   if (remainingCallsTo.size() > 0) {
      cout << "WARNING: tried to remove a downloaded-to-kernel fn ("
           << addr2hex(entryAddr) << ") which, according"
           << endl
           << "to the call graph, is presently the destination of the following"
           << endl
           << "call sites." << endl;
      
      pdvector<kptr_t>::const_iterator iter = remainingCallsTo.begin();
      pdvector<kptr_t>::const_iterator finish = remainingCallsTo.end();
      for (; iter != finish; ++iter) {
         cout << addr2hex(*iter) << endl;
      }

      return false; // NOT OK to remove this fn.
   }

   if (parsedFnOrNULL != NULL) { // don't do the following code if we weren't parsed!
      // What routines is this function that we're about to fry calling?

      const fnCode_t currFnContents = peekCurrFnCode(*parsedFnOrNULL);

      pdvector< pair<kptr_t,kptr_t> > callsThisFnMade;
      pdvector< pair<kptr_t,kptr_t> > interProcBranchesThisFnMade;
      pdvector<kptr_t> ignore_unanalyzableCalls;

      parsedFnOrNULL->getCallsMadeBy_WithSuppliedCode(currFnContents,
                                                      callsThisFnMade,
                                                      true,
                                                      interProcBranchesThisFnMade,
                                                      false,
                                                      ignore_unanalyzableCalls
                                                      );
         // true --> interproc branches, too

      callsThisFnMade += interProcBranchesThisFnMade;
         // Since, at least in this routine, we really only wanted one result
         // in the first place...

      // If we're calling any routine that's being replaced, tell the replacement
      // structure that this is no longer the case.
      pdvector< pair<kptr_t,kptr_t> >::const_iterator iter = callsThisFnMade.begin();
      pdvector< pair<kptr_t,kptr_t> >::const_iterator finish = callsThisFnMade.end();
      for (; iter != finish; ++iter) {
         const kptr_t calleeAddr = iter->second; // yes, second

         if (replacedFnsMap.defines(calleeAddr)) {
	    cout << "removeDownloadedToKernel gentle warning: removing code that's presently calling " << num2string(calleeAddr) << ", yet that's been replaced by someone.  How come we weren't correctly patched up to call the replacement function?" << endl;
         }
         else if (replacementFnsMap.defines(calleeAddr)) {
            cout << "removeDownloadedToKernel: removing "
                 << addr2hex(entryAddr) << ", which at "
                 << addr2hex(iter->first) << " is calling a replacement function at "
                 << addr2hex(calleeAddr)
                 << ".  Telling that replacement function that we're no longer a callee; it will alter the call instruction back to its original form.  Then we'll update the call graph to reflect that change."
                 << endl;

            replaceFunction *theReplaceFunction = replacementFnsMap.get(calleeAddr);
            const kptr_t origCallDestAddrThatIsNowBackInPlace = 
               theReplaceFunction->unPatch1Caller(iter->first); // ->first: the call site

            if (origCallDestAddrThatIsNowBackInPlace == calleeAddr) {
               cout << "Well it looks like after undoing this one call site, there was no change" << endl;
               cout << "to the call instruction.  So not updating the call graph." << endl;
            }
            else {
	       cout << "Due to call change, removing call site "
		    << num2string(iter->first) << "-->" 
		    << num2string(calleeAddr) << endl
		    << "and replacing it with "
                    << num2string(iter->first) << "-->" 
		    << num2string(origCallDestAddrThatIsNowBackInPlace) << endl;
               global_moduleMgr->change1CallSite(iter->first,
                                                 calleeAddr,
                                                 origCallDestAddrThatIsNowBackInPlace);
            }
         }
      }
   }

   return true; // OK to remove this fn.
}

void instrumentationMgr::removeDownloadedToKernel(client_connection &theClient,
                                                  unsigned reqid) {
   const pdvector<downloadedIntoKernelEntry1Fn> &info =
      theClient.queryDownloadedToKernelAddresses(reqid);
      // note that any given "downloadedIntoKernelEntry1Fn" may or may not
      // have also be parsed into a "proper" function.  If it wasn't, it's certainly
      // nothing to panic about.

   bool OKToRemoveChunk = true; // for now

   const moduleMgr &theModuleMgr = *global_moduleMgr;

   pdvector<const function_t*> parsedFnsBeingRemoved;

   pdvector<downloadedIntoKernelEntry1Fn>::const_iterator iter = info.begin();
   pdvector<downloadedIntoKernelEntry1Fn>::const_iterator finish = info.end();
   for (; iter != finish; ++iter) {
      const downloadedIntoKernelEntry1Fn &fninfo = *iter;
      const kptr_t entryAddr = fninfo.logicalChunks[fninfo.entry_chunk_ndx];
      const function_t *fnOrNULL =
         theModuleMgr.findFnOrNULL(entryAddr, true); // true --> entry only

      if (fnOrNULL != NULL)
         parsedFnsBeingRemoved += fnOrNULL;
   }

   // assert no duplicate entries in parsedFnsBeingRemoved:
   pdvector<const function_t *> copy_parsedFnsBeingRemoved(parsedFnsBeingRemoved);
   std::sort(copy_parsedFnsBeingRemoved.begin(), copy_parsedFnsBeingRemoved.end());
      // no operator() is provided, so we'll end up sorting by pointer addresses,
      // which is good enough for what we want to do: check for duplicates
   if (std::adjacent_find(copy_parsedFnsBeingRemoved.begin(),
                     copy_parsedFnsBeingRemoved.end()) !=
       copy_parsedFnsBeingRemoved.end())
      assert(false);

   // re-set "iter"; "finish" need not be changed.
   iter = info.begin();
   for (; iter != finish; ++iter) {
      const downloadedIntoKernelEntry1Fn &fninfo = *iter;

      const kptr_t EntryAddr = fninfo.logicalChunks[fninfo.entry_chunk_ndx];
   
      const function_t *parsedFnIfAny =
         theClient.downloadedToKernel2ParsedFnOrNULL(EntryAddr);

      if (!remove1DownloadedToKernel_preliminary(EntryAddr, parsedFnIfAny,
                                                 parsedFnsBeingRemoved))
         OKToRemoveChunk = false;
   }

   if (OKToRemoveChunk)
      theClient.removeDownloadedToKernel(reqid);
   else
      cout << "instrumentationMgr::removeDownloadedToKernel() -- not removing this chunk, since it's not safe to do so.  Ignoring the remove request." << endl;
}

pdvector<kdownloadedLocations1Fn> // see the .I file for definition
instrumentationMgr::
queryDownloadedToKernelAddresses(client_connection &theClient, unsigned reqid) const {
   const pdvector<downloadedIntoKernelEntry1Fn> &initial_result = 
      theClient.queryDownloadedToKernelAddresses(reqid);

   // For igen purposes, we need to convert "result" to type "kdownloadedLocations";
   // May sound like a hack, but if it needs to be done, then this class is
   // the *perfect* candidate to do it; it exists for just such purposes.

   pdvector<kdownloadedLocations1Fn> final_result;
      // a pair.  .first: vector of kptr_t (one per chunk)   .second: the entry chunk

   pdvector<downloadedIntoKernelEntry1Fn>::const_iterator iter = initial_result.begin();
   pdvector<downloadedIntoKernelEntry1Fn>::const_iterator finish = initial_result.end();
   for (; iter != finish; ++iter) {
      const downloadedIntoKernelEntry1Fn &initial_entry = *iter;

      kdownloadedLocations1Fn *final_entry_ptr = 
         final_result.append_with_inplace_construction();

      new((void*)final_entry_ptr)
         kdownloadedLocations1Fn(initial_entry.logicalChunks,
                                 initial_entry.entry_chunk_ndx);
   }

   return final_result;
}

kdownloadedLocations1Fn
instrumentationMgr::
queryDownloadedToKernelAddresses1Fn(client_connection &theClient,
                                    unsigned reqid) const {
   pdvector<kdownloadedLocations1Fn> result =
      queryDownloadedToKernelAddresses(theClient, reqid);
   
   assert(result.size() == 1);
   return result[0];
}

// --------------------

void instrumentationMgr::
parseKernelRangesAsFns(client_connection &theClient,
                       const pdvector<parseNewFunction> &fnsInfo) {
   typedef client_connection::parseKernelRangeAs1FnInfo parseKernelRangeAs1FnInfo;
   
   pdvector<parseKernelRangeAs1FnInfo> fnsInfoPlusCode;

   pdvector<parseNewFunction>::const_iterator fniter = fnsInfo.begin();
   pdvector<parseNewFunction>::const_iterator fnfinish = fnsInfo.end();
   for (; fniter != fnfinish; ++fniter) {
      const parseNewFunction &theParseInfo = *fniter;
      
      fnCode_t theCode(fnCode_t::empty);

      pdvector<char*> buffers;
      pdvector< pair<kptr_t,unsigned> >::const_iterator chunk_iter =
         theParseInfo.chunkAddrs.begin();
      pdvector< pair<kptr_t,unsigned> >::const_iterator chunk_finish =
         theParseInfo.chunkAddrs.end();
      for (; chunk_iter != chunk_finish; ++chunk_iter) {
         const kptr_t addr = chunk_iter->first;
         const unsigned nbytes = chunk_iter->second;
      
         assert(nbytes < 100000);
         char *buffer = new char[nbytes];
         assert(buffer);
      
         unsigned nBytesActuallyRead =
            global_kernelDriver->peek_block(buffer, addr, nbytes, 
					    true); // Read as much as possible

         insnVec_t *theInsnVec = insnVec_t::getInsnVec(buffer, nBytesActuallyRead);
         theCode.addChunk(addr, theInsnVec, false);
            // false --> don't resort chunks now
      }
      theCode.sortChunksNow();

      parseKernelRangeAs1FnInfo *i = fnsInfoPlusCode.append_with_inplace_construction();
      new((void*)i)parseKernelRangeAs1FnInfo(theParseInfo, theCode);
      
      pdvector<char*>::const_iterator biter = buffers.begin();
      pdvector<char*>::const_iterator bfinish = buffers.end();
      for (; biter != bfinish; ++biter) {
         const char *buffer = *biter;
         delete [] buffer;
      }
   }

   theClient.parseKernelRangesAsFns(fnsInfoPlusCode);
}

instrumentationMgr::fnCode_t
instrumentationMgr::peekCurrFnCode(const function_t &fn) {
   const fnCode_t &origCode = fn.getOrigCode();

   pdvector<fnCode_t::codeChunk> result;
   result.reserve_exact(origCode.end() - origCode.begin());

   fnCode_t::const_iterator finish = origCode.end();
   for (fnCode_t::const_iterator iter = origCode.begin(); iter != finish; ++iter) {
      const fnCode_t::codeChunk &c = *iter;
      
      const kptr_t startAddr = c.startAddr;
      const unsigned numBytes = c.theCode->numInsnBytes();

      void *buffer = malloc(numBytes);
      assert(buffer);
      unsigned numRead = 
	  global_kernelDriver->peek_block(buffer, startAddr, 
					  numBytes, 
					  true); // Read as much as possible
      
      insnVec_t *theInsnVec = insnVec_t::getInsnVec(buffer, numRead);

      free(buffer);
      
      result += fnCode_t::codeChunk(startAddr, theInsnVec);
   }

   return fnCode_t(result); // lots of copying; ugh
}


// Downloading into kerninstd:

void instrumentationMgr::
downloadToKerninstd(client_connection &theClient,
                    unsigned reqid,
                       // only needs to be unique for this client's
                       // downloadToKerninstd requests
                    const relocatableCode_t *theCode, unsigned entry_chunk_ndx) {
   theClient.downloadToKerninstd(reqid, theCode, entry_chunk_ndx);
}

void instrumentationMgr::removeDownloadedToKerninstd(client_connection &theClient,
                                                     unsigned reqid) {
   // doesn't try to stop sampling or anything like that
   theClient.removeDownloadedToKerninstd(reqid);
}

pair<pdvector<dptr_t>, unsigned>
instrumentationMgr::
queryDownloadedToKerninstdAddresses(client_connection &theClient,
                                    unsigned reqid) const {
   return theClient.queryDownloadedToKerninstdAddresses(reqid);
}

void instrumentationMgr::
doDownloadedToKerninstdCodeOnceNow(client_connection &theClient,
                                   unsigned uniqDownloadedCodeId) {
   theClient.doDownloadedToKerninstdCodeOnceNow(uniqDownloadedCodeId);
}

void instrumentationMgr::
periodicallyDoDownloadedToKerninstdCode(client_connection &theClient,
                                        unsigned uniqDownloadedCodeId,
                                        unsigned period_millisecs) {
   // can be used to set or change an interval
   assert(period_millisecs != 0);
   theClient.periodicallyDoDownloadedToKerninstdCode(uniqDownloadedCodeId,
                                                     period_millisecs);
}

void instrumentationMgr::
stopPeriodicallyDoingDownloadedToKerninstdCode(client_connection &theClient,
                                               unsigned uniqDownloadedCodeId) {
   theClient.stopPeriodicallyDoingDownloadedToKerninstdCode(uniqDownloadedCodeId);
}

void instrumentationMgr::periodicallyDoSampling(client_connection &theClient,
                                                unsigned uniqSampleReqId,
                                                unsigned period_millisecs) {
   // can be used to set or change an interval
   assert(period_millisecs != 0);
   theClient.periodicallyDoSampling(uniqSampleReqId, period_millisecs); 
}

void instrumentationMgr::
stopPeriodicallyDoingSampling(client_connection &theClient,
                              unsigned uniqSampleReqId) {
   theClient.stopPeriodicallyDoingSampling(uniqSampleReqId); 
}

void instrumentationMgr::doSamplingOnceNow(client_connection &theClient,
                                           unsigned uniqSampleReqId) {
   theClient.doSamplingOnceNow(uniqSampleReqId); 
}

void instrumentationMgr::temporarilyTurnOffSampling(client_connection &theClient) {
   theClient.temporarilyTurnOffSampling();
}

void instrumentationMgr::resumeSampling(client_connection &theClient) {
   theClient.resumeSampling();
}

   
void instrumentationMgr::presentSampleData1Value(client_connection &theClient,
                                                 unsigned sampReqId,
                                                 uint64_t sampCycles,
                                                 sample_type sampValue) {
   theClient.presentSampleData1Value(sampReqId, sampCycles, sampValue);
}

void instrumentationMgr::
presentSampleDataSeveralValues(client_connection &theClient,
                               unsigned sampReqId,
                               uint64_t sampCycles,
                               unsigned numSampleValues,
                               sample_type *theSampleValues) {
   theClient.presentSampleDataSeveralValues(sampReqId, sampCycles,
                                            numSampleValues, theSampleValues);
}

