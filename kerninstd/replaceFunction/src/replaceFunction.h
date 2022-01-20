// replaceFunction.h
// Manages jumping from an old function to a new one.
// Both functions must already exist and be downloaded to
// the kernel, etc.

#ifndef _REPLACE_FUNCTION_H_
#define _REPLACE_FUNCTION_H_

#include "launcher.h"
#include "callLauncher.h"
#include "moduleMgr.h"
#include "kernelDriver.h"
#include "SpringBoardHeap.h"
#include "clientConn.h"

class replaceFunction {
 private:
   // Globals that we keep so we don't have to pass them around:
   const moduleMgr &theModuleMgr;
   kernelDriver &theKernelDriver;
   SpringBoardHeap &theSpringBoardHeap;
   
   const function_t *oldfn;
   regset_t *deadRegsAtStartOfOldFn;

   typedef pair<client_connection*, unsigned> requester; // .second: reqid
   typedef pair<const function_t *, bool> prefs;
      // .first: new function
      // .second: flag: replaceCallSitesToo

   pdvector< pair<requester, prefs> > presentRequests;
      // An explanation is in order here.  Since in theory there can be several
      // requests to replace a function, each with their own client/reqid and
      // their own desired newFn, and only the most recent one is presently
      // taking effect, we need to keep track of them, in the order received.
      // We append to this vector, so the highest ndx contains the most recent
      // request, and thus the one that is presently installed
   // TODO: if we agree that this is too complex, then get rid of the vector
   // (or, logically, make it a vector that is always of size 1)

   // Presently installed stuff (corresponds to highest ndx'd elem in presentRequests)
   launcher *theLauncher; // NULL until installed
   pdvector<callLauncher*> patchedUpCallSites;
      // empty until installed

   // --------------------

   static void callbackForWhenACallSiteHasBeenAdded(kptr_t caller,
                                                    kptr_t callee,
                                                    void *pthis);
      // We tell module manager that we'd like to be informed (via this callback)
      // when a new call site to "callee" has been found.

   static void callbackForWhenACallSiteIsAboutToBeFried(kptr_t caller,
                                                        kptr_t callee,
                                                        void *pthis);
      // We tell module manager that we'd like to be informed (via this callback)
      // when a certain call site (to "callee") is about to be torched
      // because the function containing "caller" is about to be deleted.
      // (Presumably "caller" was within some downloadedToKernel code that
      // is no longer required)

   // --------------------

   void installRequestReplacingExistingOneNow(const prefs &,
                                              bool verboseSummaryTiming,
                                              bool verboseIndividualTiming);
      // doesn't update the vector presentRequests

   void removeRequestAndUninstallIfAppropriateNow(pdvector< pair<requester,prefs> >::iterator,
                                                  bool verboseSummaryTiming,
                                                  bool verboseIndividualTiming);
      // Definitely modifies presentRequests[]
   
   void installNow(launcher *theNewLauncher,
                   const pdvector<callLauncher*> &theNewPatchedUpCallSites,
                   bool verboseSummaryTiming,
                   bool verboseIndividualTiming);
      // Doesn't modify presentRequests[]

   pair< launcher*, pdvector<callLauncher*> >
   makeLaunchers(const function_t *newfn,
                 bool replaceCallSitesToo) const;

   void unInstallWithNoReplacement(bool verboseSummaryTiming,
                                   bool verboseIndividualTiming);
      // call when presentRequests.size() == 1.  Will set its size() to 0 before
      // returning.

   // --------------------

   replaceFunction(const replaceFunction &);
   replaceFunction &operator=(const replaceFunction &);
   
 public:
   replaceFunction(const function_t &ioldfn,
                   const moduleMgr &iModuleMgr,
                   kernelDriver &,
                   SpringBoardHeap &);
  ~replaceFunction();

   void kerninstdIsGoingDown();
      // perform emergency un-replace NOW

   const function_t *getOldFn() const { return oldfn; }

   const function_t *getPresentlyInstalledNewFn() const {
      return presentRequests.back().second.first;
   }

   void makeRequestAndInstallNow(client_connection *,
                                 unsigned reqid, // unique for this client
                                 const prefs &, // new fn & replaceCallSitesToo flag
                                 bool verboseSummaryInstallTiming,
                                 bool verboseIndividualInstallTiming);
   
   bool removeRequestAndUninstallIfAppropriateNow(client_connection &,
                                                  unsigned reqid,
                                                  bool verboseSummaryTiming,
                                                  bool verboseIndividualTiming);
      // uninstalls this requests, and, as appropriate, installs
      // the next one waiting in the queue.  Returns true iff the replaceFunction
      // has now been gutted (no more requests) and can be deleted.

   kptr_t unPatch1Caller(kptr_t callSiteAddr);
      // returns the original call destination which has now
      // been put back into place.
};

#endif
