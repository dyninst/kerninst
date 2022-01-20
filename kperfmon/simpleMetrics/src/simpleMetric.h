// simpleMetric.h

#ifndef _SIMPLE_METRIC_H_
#define _SIMPLE_METRIC_H_

class focus; // fwd decl more efficient
#include "kapi.h"
#include <assert.h>

class complexMetricFocus; // needed forward declaration to avoid recursive #include

class cmfInstantiatePostSmfInstantiation {
 private:
   complexMetricFocus &cmf;

   // prevent copying:
   cmfInstantiatePostSmfInstantiation(const cmfInstantiatePostSmfInstantiation &);
   cmfInstantiatePostSmfInstantiation& operator=(const cmfInstantiatePostSmfInstantiation &);
 protected:
   complexMetricFocus& getComplexMetricFocus() { return cmf; }
   const complexMetricFocus& getComplexMetricFocus() const { return cmf; }
   
 public:
   cmfInstantiatePostSmfInstantiation(complexMetricFocus &icmf) : cmf(icmf) {}
   virtual ~cmfInstantiatePostSmfInstantiation() {}
   virtual void smfInstantiationHasCompleted() = 0;
};

class simpleMetricFocus; // necessary fwd declaration to avoid recursive #include

// fwd decl:
class function;

class simpleMetric {
 private:
   unsigned id;

   // Number of simpleMetricFocuses using this simpleMetric
   mutable unsigned subscribedRefCount;
   
   // prevent copying:
   simpleMetric(const simpleMetric &src);
   simpleMetric& operator=(const simpleMetric &);
 protected:
   typedef uint16_t bbid_t;
      // I'd MUCH prefer function_t::bbid_t, but function_t is an incomplete type
      // at this point, since we've just done a fwd decl.
   
 public:
   simpleMetric(unsigned iid) : id(iid), subscribedRefCount(0) {}
   virtual ~simpleMetric() {}

   unsigned getId() const { return id; }

   void reference() const { subscribedRefCount++; }
   void dereference() const { 
      assert(subscribedRefCount > 0); 
      subscribedRefCount--; 
   }

   unsigned getRefCount() const { return subscribedRefCount; }

   virtual bool canInstantiateFocus(const focus &theFocus) const = 0;
   virtual unsigned getNumValues() const = 0;

   virtual kapi_hwcounter_kind getHwCtrKind() const {
       return HWC_NONE;
   }

   virtual kapi_hwcounter_set queryPostInstrumentationPcr(
       const kapi_hwcounter_set &oldPcrValue,
       const focus &) const {
       return oldPcrValue; // The default version does not mess with hwcounters
   }
   virtual bool changeInPcrWouldInvalidateSMF(
       const kapi_hwcounter_set &/*proposedNewPcrValue*/,
       const focus &) const {
      return false;
   }

   // The central method of this class:
   virtual void asyncInstantiate(simpleMetricFocus &smf, // fill this in
                                 cmfInstantiatePostSmfInstantiation &) const = 0;

   virtual void cleanUpAfterUnsplice(const simpleMetricFocus &) const {
      // default does nothing; certain classes will override.  virtualization metrics
      // come to mind as those which have *lots* of cleanup work to do at this time.
   }
};

#endif
