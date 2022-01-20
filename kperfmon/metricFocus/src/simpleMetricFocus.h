// simpleMetricFocus.h

#ifndef _SIMPLE_METRIC_FOCUS_H_
#define _SIMPLE_METRIC_FOCUS_H_

#include <inttypes.h> // uint32_t
#include "util/h/kdrvtypes.h"
#include "simpleMetric.h"
#include "focus.h"
#include "snippet.h"

class simpleMetricFocus {
 private:
   const simpleMetric &theSimpleMetric;
   focus theFocus;

   // Number of complexMetricFocuses' that are subscribed to us:
   // (Incremented at cmf instantiation, which is well before splicing takes place.
   // Thus we have a separate splice reference count, too.)
   mutable unsigned subscribedRefCount;
   mutable unsigned splicedRefCount;

   pdvector< std::pair<kptr_t, const snippet*> > snippets;
   pdvector<kptr_t> uint8KernelAddrs;
   pdvector<kptr_t> timer16KernelAddrs;
   pdvector<kptr_t> vtimerKernelAddrs;

   // prevent copying
   simpleMetricFocus(const simpleMetricFocus &);
   simpleMetricFocus& operator=(const simpleMetricFocus &);
   
 public:
   simpleMetricFocus(const simpleMetric &iSimpleMetric, const focus &iFocus,
                     unsigned isubscribedRefCount,
                     unsigned isplicedRefCount) :
      theSimpleMetric(iSimpleMetric), theFocus(iFocus),
      subscribedRefCount(isubscribedRefCount),
      splicedRefCount(isplicedRefCount) {
      theSimpleMetric.reference();
   }
  ~simpleMetricFocus() {
      // are these asserts too strong?
      assert(subscribedRefCount == 0);
      assert(splicedRefCount == 0);
      theSimpleMetric.dereference();
   }

   const simpleMetric &getSimpleMetric() const { return theSimpleMetric; }
   const focus &getFocus() const { return theFocus; }
   
   void referenceSubscribed() const { ++subscribedRefCount; }
   bool dereferenceSubscribed() const {
      assert(subscribedRefCount >= 1);
      return (--subscribedRefCount == 0);
   }
   unsigned getSubscribedRefCount() const { return subscribedRefCount; }

   void referenceSpliced() const { ++splicedRefCount; }
   bool dereferenceSpliced() const {
      assert(splicedRefCount >= 1);
      return --splicedRefCount == 0;
   }
   unsigned getSplicedRefCount() const { return splicedRefCount; }
   
   void addSnippet(kptr_t kernelAddr, const snippet &s) {
      snippets += std::make_pair(kernelAddr, s.dup());
   }
   
   // Keeping track of allocated data items (counters, timers, vtimers).
   // These methods will presumably be called by specific metrics, during
   // instantiation:
   void addUint8(kptr_t kernelAddr) { uint8KernelAddrs += kernelAddr; }
   void addTimer16(kptr_t kernelAddr) { timer16KernelAddrs += kernelAddr; }
   void addVtimer(kptr_t kernelAddr) { vtimerKernelAddrs += kernelAddr; }
   const pdvector<kptr_t> &getuint8KernelAddrs() const { return uint8KernelAddrs; }
   const pdvector<kptr_t> &gettimer16KernelAddrs() const { return timer16KernelAddrs; }
   const pdvector<kptr_t> &getvtimerKernelAddrs() const { return vtimerKernelAddrs; }

   // snippets:
   unsigned getNumSnippets() const { return snippets.size(); }
   std::pair<kptr_t, const snippet *> getSnippetInfo(unsigned ndx) const {
      // no need to return a reference to the pair
      return snippets[ndx];
   }
};

#endif
