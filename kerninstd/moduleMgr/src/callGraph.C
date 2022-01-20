// callGraph.C

#include "callGraph.h"
#include "util/h/hashFns.h"
#include "util/h/rope-utils.h" // addr2hex()
#include <algorithm> // STL's sort()
#include <iostream.h>

// ----------------------------------------------------------------------

callGraph::infoForOneCallee::infoForOneCallee() {
   // "callSites" initially an empty vector
   presentlySorted = true;
}

callGraph::infoForOneCallee::infoForOneCallee(const infoForOneCallee &src) :
   callSites(src.callSites), presentlySorted(src.presentlySorted) {
}

callGraph::infoForOneCallee& callGraph::infoForOneCallee::operator=(const infoForOneCallee &src) {
   if (this != &src) {
      callSites = src.callSites;
      presentlySorted = src.presentlySorted;
   }
   return *this;
}

callGraph::infoForOneCallee::~infoForOneCallee() {
}

void callGraph::infoForOneCallee::sortIfNeeded() {
   if (presentlySorted)
      return;
         
   std::sort(callSites.begin(), callSites.end());
   
   presentlySorted = true;
}



bool callGraph::infoForOneCallee::addCallSite(addr_t newCallSite, // i.e., "from"
                                  bool assertNotDuplicate) {
   if (assertNotDuplicate) {
      sortIfNeeded();

      if (std::binary_search(callSites.begin(), callSites.end(), newCallSite)) {
         return false;
      }
      else {
         // fall thru; continue adding to callSites[]
      }
   }
         
   callSites += newCallSite;
   presentlySorted = false;
   return true;
}

bool callGraph::infoForOneCallee::removeCallSite(addr_t theCallSite, // i.e., "from"
                                     bool assertExisted) {
   assert(callSites.size() > 0);
   
   sortIfNeeded();
         
   pdvector<addr_t>::iterator iter = std::lower_bound(callSites.begin(),
                                                 callSites.end(),
                                                 theCallSite);
   if (iter == callSites.end() || *iter != theCallSite) {
      if (assertExisted)
         assert(false);
      return false;
   }

   // found
   if (iter + 1 == callSites.end()) {
      // Removing the last elem in the vector; the fast case.  Can keep sorted.
            
      callSites.pop_back();
   }
   else {
      *iter = callSites.back();
      callSites.pop_back();

      presentlySorted = false;
   }

   return callSites.size() == 0;
}

// ----------------------------------------------------------------------

callGraph::callGraph(const moduleMgr &imoduleMgr) :
   theModuleMgr(imoduleMgr),
   infoByCallee(addrHash4) {
}

callGraph::~callGraph() {
}

unsigned callGraph::getMemUsage() const {
   unsigned result = 0;
   result += sizeof(infoByCallee);
   result += infoByCallee.getMemUsage_exceptObjItself_AndExtraFromKeyOrValue();
   
   dictionary_hash<addr_t, infoForOneCallee>::const_iterator iter =
      infoByCallee.begin();
   dictionary_hash<addr_t, infoForOneCallee>::const_iterator finish =
      infoByCallee.end();
   for (; iter != finish; ++iter) {
      const infoForOneCallee &info = *iter;
      
      result += info.getMemUsage_exceptObjItself();
   }

   return result;
}

pdvector< std::pair<callGraph::addr_t, callGraph::addr_t> >
callGraph::getEverything() const {
   pdvector< std::pair<addr_t, addr_t> > result;

   dictionary_hash<addr_t, infoForOneCallee>::const_iterator calleeIter = infoByCallee.begin();
   dictionary_hash<addr_t, infoForOneCallee>::const_iterator calleeFinish = infoByCallee.end();
   for (; calleeIter != calleeFinish; ++calleeIter) {
      const addr_t calleeAddr = calleeIter.currkey();
      const infoForOneCallee &theInfoForOneCallee = calleeIter.currval();

      bool sorted;
      const pdvector<addr_t> &theCallSitesAddrs = theInfoForOneCallee.getCallSites(sorted);
      assert(theCallSitesAddrs.size() > 0);
      
      pdvector<addr_t>::const_iterator iter = theCallSitesAddrs.begin();
      pdvector<addr_t>::const_iterator finish = theCallSitesAddrs.end();
      for (; iter != finish; ++iter) {
         const addr_t callSiteInsnAddr = *iter;
         
         result += std::make_pair(callSiteInsnAddr, calleeAddr);
      }
   }

   return result;
}

void callGraph::
removeAllCallsToAFunction(addr_t fnEntryAddr) {
   // NOTE: Does *not* remove calls made *by* this function, so there could certainly
   // still be remnants of this function in the call graph!
   infoByCallee.undef(fnEntryAddr);
}

void callGraph::
removeAFunction(addr_t fnEntryAddr,
                const pdvector< std::pair<addr_t, addr_t> > &regularCallsMadeByFunction,
                const pdvector< std::pair<addr_t, addr_t> > &interProcBranchesMadeByFunction,
                bool assertCallsMadeByFunctionExisted) {
   // First fry all calls made *by* this function.
   // We do this part first since the second part, in the presence of possible
   // recursion, would cause an unfortunate assertion failure should
   // assertCallsMadeByFunctionExisted be true and should we forget to check for
   // that special case (which we don't forget, actually)

   pdvector< std::pair<addr_t,addr_t> >::const_iterator iter =
      regularCallsMadeByFunction.begin();
   pdvector< std::pair<addr_t,addr_t> >::const_iterator finish =
      regularCallsMadeByFunction.end();
   for (; iter != finish; ++iter) {
      const addr_t callSiteAddr = iter->first;
      const addr_t calleeAddr = iter->second;
      if (calleeAddr == fnEntryAddr)
         // recursion, let part 2 handle removing it
         continue;
      
      removeCallSite(callSiteAddr, // from-addr
                     calleeAddr,
                     assertCallsMadeByFunctionExisted);
   }
   
   iter = interProcBranchesMadeByFunction.begin();
   finish = interProcBranchesMadeByFunction.end();
   for (; iter != finish; ++iter) {
      const addr_t callSiteAddr = iter->first;
      const addr_t calleeAddr = iter->second;
      if (calleeAddr == fnEntryAddr)
         // recursion, let part 2 handle removing it
         continue;
      
      removeCallSite(callSiteAddr, // from-addr
                     calleeAddr,
                     assertCallsMadeByFunctionExisted);
   }
   
   // Then fry, with blazing speed, all made made *to* this function...
   removeAllCallsToAFunction(fnEntryAddr);
}

void callGraph::
addCallSite(addr_t from, addr_t to, bool assertDidntAlreadyExist) {
   //cout << "Adding call site " << addr2hex(from) << " to " << addr2hex(to) << endl;

   infoForOneCallee &i = infoByCallee[to];
      // yes, to.  And yes, use operator[], so we create
      // an entry for this fn if it didn't already exist.
   
   if (!i.addCallSite(from, assertDidntAlreadyExist)) {
      cout << "WARNING: call graph detected repeat call from "
           << addr2hex(from) << " to " << addr2hex(to) << endl;
      cout << "It is likely that the same function, or overlapping parts of it,"
           << endl;
      cout << "are being parsed twice." << endl;

      // Now, infoByCallee[to] may have created an empty infoForOneCallee structure that now needs
      // to be fried.
      bool ignore_sorted;
      if (i.getCallSites(ignore_sorted).size() == 0)
         (void)infoByCallee.get_and_remove(to);
   }

   bool ignore_sorted;
   assert(i.getCallSites(ignore_sorted).size() > 0);
}

void callGraph::
removeCallSite(addr_t from, addr_t to, bool assertExisted) {
   if (!infoByCallee.defines(to)) { // yes, to
      if (assertExisted) {
         cout << "WARNING: could not find call site "
              << addr2hex(from) << " --> " << addr2hex(to) << endl;
      }
      return;
   }
      
   infoForOneCallee &i = infoByCallee.get(to); // yes, to
   if (i.removeCallSite(from, assertExisted)) {
      // It's now empty, so remove it.  dtor depends on this
      bool ignore_sorted;
      assert(infoByCallee.get(to).getCallSites(ignore_sorted).size() == 0);
      (void)infoByCallee.get_and_remove(to);
   }
   else {
      bool ignore_sorted;
      assert(infoByCallee.get(to).getCallSites(ignore_sorted).size() > 0);
   }
}

const pdvector<callGraph::addr_t> &
callGraph::getCallsMadeToAFn(addr_t fnEntryAddr,
                             bool &isSorted) const {
   if (!infoByCallee.defines(fnEntryAddr)) {
      static pdvector<addr_t> emptyVector;
      return emptyVector;
   }
   else
      return infoByCallee.get(fnEntryAddr).getCallSites(isSorted);
}

// --------------------

void callGraph::updateCallGraphForReplaceFunction(addr_t oldFnEntryAddr,
                                                  addr_t newFnEntryAddr) {
   // change calls to "oldFnEntryAddr" with calls to "newFnEntryAddr".
   // NOTE: When all done, the call graph will say that no one calls "oldFnEntryAddr".
   // HOWEVER, "oldFnEntryAddr" will still be in the call graph, making calls to
   // others!!!!!!

   bool isSorted; // note no reference in the below vector; intentional
   const pdvector<addr_t> callSitesToOldFn = getCallsMadeToAFn(oldFnEntryAddr,
                                                               isSorted);
   
   pdvector<addr_t>::const_iterator iter = callSitesToOldFn.begin();
   pdvector<addr_t>::const_iterator finish = callSitesToOldFn.end();
   for (; iter != finish; ++iter) {
      removeCallSite(*iter, // from addr
                     oldFnEntryAddr,
                     true // yes, assert existed
                     );
   }

   iter = callSitesToOldFn.begin();
   for (; iter != finish; ++iter) {
      addCallSite(*iter, // from addr
                  newFnEntryAddr,
                  true // yes, assert didn't already exist
                  );
   }
}

void callGraph::updateCallGraphForNewReplaceFunctionDest(addr_t /*oldFnEntryAddr*/,
                                                         addr_t priorReplacementAddr,
                                                         addr_t newReplacementAddr) {
   // change calls to "priorReplacementAddr" with calls to "newReplacementAddr"
   // Note that it seems that we can use existing code from within
   // updateCallGraphForReplaceFunction(); is that right?
   updateCallGraphForReplaceFunction(priorReplacementAddr,
                                     newReplacementAddr);
}

void callGraph::
updateCallGraphForUnReplaceFunction(addr_t fnAddr,
                                    addr_t usedToBeReplacedByThisAddr) {
   // changes calls to "usedToBeReplacedByThisAddr" back to calls to "fnAddr".
   // It seems that we can use existing code; is this right?
   updateCallGraphForReplaceFunction(usedToBeReplacedByThisAddr, fnAddr);
}

void callGraph::updateCallGraphForReplaceCall(addr_t callSiteAddr,
                                              addr_t oldFnEntryAddr,
                                              addr_t newFnEntryAddr)
{
   // change call at "callSiteAddr" to have dest "newFnEntryAddr".

   removeCallSite(callSiteAddr, // from addr
                  oldFnEntryAddr,
                  true // yes, assert existed
                  );

   addCallSite(callSiteAddr, // from addr
               newFnEntryAddr,
               true // yes, assert didn't already exist
               );
}

// --------------------

sampleStatistics callGraph::getStatsOfCalledFunctions(unsigned &numZeros) const {
   // Will NOT include ANY information about any function that is never called,
   // except where numZeros is nonzero, which I don't expect.

   numZeros = 0;
   
   sampleStatistics result;
   
   dictionary_hash<addr_t, infoForOneCallee>::const_iterator iter =
      infoByCallee.begin();
   dictionary_hash<addr_t, infoForOneCallee>::const_iterator finish =
      infoByCallee.end();
   for (; iter != finish; ++iter) {
      //const addr_t calleeAddr = iter.currkey();
      const infoForOneCallee &theInfoForOneCallee = iter.currval();

      bool ignore_sorted;
      const unsigned numCallsToThisCallee = theInfoForOneCallee.getCallSites(ignore_sorted).size();

      result.addSample(numCallsToThisCallee);

      if (numCallsToThisCallee == 0) {
         cout << "zero" << endl;
         ++numZeros;
      }
   }

   return result;
}
