// replaceFunctionCall.h
// Replaces call to an old function with call to a new one.
// Both functions must already exist and be downloaded to
// the kernel, etc.

#ifndef _REPLACE_FUNCTION_CALL_H_
#define _REPLACE_FUNCTION_CALL_H_

#include "launcher.h"
#include "callLauncher.h"
#include "moduleMgr.h"
#include "kernelDriver.h"
#include "SpringBoardHeap.h"
#include "clientConn.h"

class replaceFunctionCall {
 private:
   // Globals that we keep so we don't have to pass them around:
   const moduleMgr &theModuleMgr;
   kernelDriver &theKernelDriver;
   SpringBoardHeap &theSpringBoardHeap;
   
   const kptr_t callSiteAddr;
   const function_t *oldfn;

   typedef pair<client_connection*, unsigned> requester; // .second: reqid

   pdvector< pair<requester, const function_t*> > presentRequests;
      // An explanation is in order here.  Since in theory there can be several
      // requests to replace a function call, each with their own client/reqid 
      // and their own desired newFn, and only the most recent one is presently
      // taking effect, we need to keep track of them, in the order received.
      // We append to this vector, so the highest ndx contains the most recent
      // request, and thus the one that is presently installed

   // Presently installed (highest indexed elem in presentRequests)
   callLauncher *theLauncher; // NULL until installed


   // --------------------

   void installRequestReplacingExistingOneNow(const function_t *,
                                              bool verboseSummaryTiming,
                                              bool verboseIndividualTiming);
      // doesn't update the vector presentRequests

   void removeRequestAndUninstallIfAppropriateNow(pdvector< pair<requester,const function_t*> >::iterator,
                                                  bool verboseSummaryTiming,
                                                  bool verboseIndividualTiming);
      // Definitely modifies presentRequests[]
   
   void installNow(callLauncher *theNewLauncher,
                   bool verboseSummaryTiming,
                   bool verboseIndividualTiming);
      // Doesn't modify presentRequests[]

   callLauncher* makeLauncher(const function_t *newfn) const;

   void unInstallWithNoReplacement(bool verboseSummaryTiming,
                                   bool verboseIndividualTiming);
      // call when presentRequests.size() == 1.  
      // Will set its size() to 0 before returning.

   // --------------------

   replaceFunctionCall(const replaceFunctionCall &);
   replaceFunctionCall &operator=(const replaceFunctionCall &);
   
 public:
   replaceFunctionCall(const kptr_t callSiteAddr,
                       const function_t &ioldfn,
                       const moduleMgr &iModuleMgr,
                       kernelDriver &,
                       SpringBoardHeap &);
  ~replaceFunctionCall();

   void kerninstdIsGoingDown();
      // perform emergency un-replace NOW

   const function_t *getOldFn() const { return oldfn; }

   const function_t *getPresentlyInstalledNewFn() const {
      return presentRequests.back().second;
   }

   void makeRequestAndInstallNow(client_connection *,
                                 unsigned reqid, // unique for this client
                                 const function_t *, // new fn
                                 bool verboseSummaryInstallTiming,
                                 bool verboseIndividualInstallTiming);
   
   bool removeRequestAndUninstallIfAppropriateNow(client_connection &,
                                                  unsigned reqid,
                                                  bool verboseSummaryTiming,
                                                  bool verboseIndividualTiming);
      // uninstalls this requests, and, as appropriate, installs the next
      // one waiting in the queue.  Returns true iff the replaceFunctionCall
      // has now been gutted (no more requests) and can be deleted.
};

#endif
