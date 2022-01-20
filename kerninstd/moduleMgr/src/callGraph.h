// callGraph.h
// Global -- for every function in the kernel

#ifndef _CALL_GRAPH_H_
#define _CALL_GRAPH_H_

#include "util/h/Dictionary.h"
#include "util/h/kdrvtypes.h"
#include "util/h/sampleStatistics.h"
#include <inttypes.h> // uint32_t, uint64_t

class moduleMgr; // avoids a recursive #include

class callGraph {
 public:
   typedef kptr_t addr_t;
   
 private:
   class infoForOneCallee {
    private:
      pdvector<addr_t> callSites;
         // who calls this function.  Unordered usually, though you can call sort().
      bool presentlySorted;
      
    public:
      infoForOneCallee();
      infoForOneCallee(const infoForOneCallee &src);
      infoForOneCallee& operator=(const infoForOneCallee &src);
     ~infoForOneCallee();

      unsigned getMemUsage_exceptObjItself() const {
         return sizeof(addr_t) * callSites.capacity();
      }

      const pdvector<addr_t> &getCallSites(bool &isPresentlySorted) const {
         isPresentlySorted = presentlySorted;
         return callSites;
      }

      void sortIfNeeded();
      
      bool addCallSite(addr_t newCallSite, // i.e., "from"
                       bool checkForDuplicate);
         // returns true iff OK

      bool removeCallSite(addr_t theCallSite, // i.e., "from"
                          bool assertExisted);
         // returns true iff noone else calls this function (i.e. caller list now empty)
   };

   const moduleMgr &theModuleMgr;
   dictionary_hash<addr_t, infoForOneCallee> infoByCallee; // indexed by fn entry addr

   callGraph(const callGraph &);
   callGraph &operator=(const callGraph &);
   
 public:
   callGraph(const moduleMgr &);
  ~callGraph();

   unsigned getMemUsage() const;

   pdvector< std::pair<addr_t, addr_t> > getEverything() const;
      // .first: caller addr (call site addr)
      // .second: callee addr (entry point of callee function)
   
   void addCallSite(addr_t from, addr_t to, bool assertDidntAlreadyExist);
   void removeCallSite(addr_t from, addr_t to, bool assertExisted);

   void removeAllCallsToAFunction(addr_t fnEntryAddr);
      // NOTE: Does *not* remove calls made *by* this function, so there could certainly
      // still be remnants of this function in the call graph!

   void removeAFunction(addr_t fnEntryAddr,
                        const pdvector< std::pair<addr_t, addr_t> > &regularCallsMadeByFn,
                        const pdvector< std::pair<addr_t, addr_t> > &interProcBranchesMadeByFn,
                        bool assertCallsMadeByFunctionExisted);
      // The second parameter has to be supplied, since we no longer have
      // a getCallsMadeByAFn() method here.  Note that we must do more than
      // simply remove infoByCallee[fnStartAddr] because that only does half the job:
      // frying all calls to this function.  Calls made by this function, on the
      // other hand, require iterating through "callsMadeByFunction"!

   const pdvector<addr_t> &getCallsMadeToAFn(addr_t fnEntryAddr,
                                           bool &isSorted) const;

   // presently no getCallsMadeByAFn(); you can call
   // function::getCallsMadeBy_asOrigParsed() or
   // function::getCallsMadeBy_withSuppliedCode() yourself.
   // (The different arguments are why we have commented out our getCallsMadeByAFn())

   void updateCallGraphForReplaceFunction(addr_t oldFnEntryAddr,
                                          addr_t newFnEntryAddr);
      // change calls to "oldFnEntryAddr" with calls to "newFnEntryAddr".
      // NOTE: When all done, the call graph will say that no one calls "oldFnEntryAddr".
      // HOWEVER, "oldFnEntryAddr" will still be in the call graph, making calls to
      // others!!!!!!

   void updateCallGraphForNewReplaceFunctionDest(addr_t oldFnEntryAddr,
                                                 addr_t priorReplacementAddr,
                                                 addr_t newReplacementAddr);
      // change calls to "priorReplacementAddr" with calls to "newReplacementAddr"

   void updateCallGraphForUnReplaceFunction(addr_t fnAddr,
                                            addr_t usedToBeReplacedByThisAddr);
      // changes calls to "usedToBeReplacedByThisAddr" back to calls to "fnAddr".

   void updateCallGraphForReplaceCall(addr_t callSiteAddr,
                                      addr_t oldFnEntryAddr,
                                      addr_t newFnEntryAddr);
      // change call at "callSiteAddr" to have dest "newFnEntryAddr".

   sampleStatistics getStatsOfCalledFunctions(unsigned &numZeros) const;
};

#endif
