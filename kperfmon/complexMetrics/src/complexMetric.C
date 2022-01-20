// complexMetric.C

#include "complexMetric.h"
#include "complexMetricFocus.h"
#include "sampleHandler.h"
#include "allSimpleMetricFocuses.h"
#include "kapi.h"

bool complexMetric::canInstantiateFocus(const focus &theFocus) const {
   pdvector<const simpleMetric*>::const_iterator iter = componentSimpleMetrics.begin();
   pdvector<const simpleMetric*>::const_iterator finish = componentSimpleMetrics.end();
   while (iter != finish) {
      const simpleMetric *met = *iter++;
      if (!met->canInstantiateFocus(theFocus))
         return false;
   }
   return true;
}

unsigned complexMetric::getNumberValuesToBucket() const 
{
    extern kapi_manager kmgr;
    
    unsigned num = kmgr.getMaxCPUId() + 1;
    return num * getNumComponentMetrics();
}

pdvector< std::pair<const simpleMetric*, focus> >
complexMetric::simpleMetricsAndFocusesToInstantiate(const focus &theFocus) const {
   // Default version assumes that 'theFocus', applied to each component
   // simpleMetric, is the way to go.  A given complexMetric is free to
   // override this method.

   pdvector< std::pair<const simpleMetric*, focus> > result;
      
   pdvector<const simpleMetric*>::const_iterator iter = componentSimpleMetrics.begin();
   pdvector<const simpleMetric*>::const_iterator finish = componentSimpleMetrics.end();
   while (iter != finish)
      result += std::make_pair(*iter++, theFocus);
      
   return result;
}

kapi_hwcounter_set
complexMetric::queryPostInstrumentationPcr(const kapi_hwcounter_set &old_set,
					   const focus &ifocus) const 
{
   // This default version should suffice for most, if not all metrics.
   // But in case some complexMetric would like to override, this routine is virtual.
   typedef pdvector< std::pair<const simpleMetric*, focus> > vecsmf_t;

   vecsmf_t smfs = simpleMetricsAndFocusesToInstantiate(ifocus);
   vecsmf_t::const_iterator iter = smfs.begin();
   vecsmf_t::const_iterator finish = smfs.end();
   kapi_hwcounter_set result = old_set;

   // calculate the new value:
   while (iter != finish) {
      const std::pair<const simpleMetric *, focus> &smf = *iter++;
      const simpleMetric *theSimpleMetric = smf.first;
      const focus &theFocus = smf.second;

      result = theSimpleMetric->queryPostInstrumentationPcr(result, theFocus);
   }
   return result;
}

bool complexMetric::changeInPcrWouldInvalidateMF(
    const kapi_hwcounter_set & pendingNewPcrValue, const focus &ifocus) const {
   // This default version should suffice for most, if not all metrics.
   // But in case some complexMetric would like to override, this routine is virtual.
   typedef pdvector< std::pair<const simpleMetric*, focus> > vsmf_t;

   vsmf_t smfs = simpleMetricsAndFocusesToInstantiate(ifocus);
   vsmf_t::const_iterator iter = smfs.begin();
   vsmf_t::const_iterator finish = smfs.end();

   while (iter != finish) {
      const std::pair<const simpleMetric *, focus> &smf = *iter++;
      const simpleMetric *theSimpleMetric = smf.first;
      const focus &theFocus = smf.second;

      if (theSimpleMetric->changeInPcrWouldInvalidateSMF(pendingNewPcrValue, theFocus))
         return true;
   }
   
   return false;
}

class cmfInstantiationInfo {
 private:
   complexMetricFocus &theComplexMetricFocus;
   
   pdvector< std::pair<const simpleMetric*, focus> > theComponents;
   unsigned curr_ndx;

   postCMFInstantiationCallback *cb;
   
 public:
   cmfInstantiationInfo(complexMetricFocus &iComplexMetricFocus,
                        const pdvector< std::pair<const simpleMetric*, focus> > &iComponents,
                        postCMFInstantiationCallback &icb) :
      theComplexMetricFocus(iComplexMetricFocus),
      theComponents(iComponents), curr_ndx(0),
      cb(icb.dup()) {
   }
  ~cmfInstantiationInfo() {
      delete cb;
   }

   complexMetricFocus &getComplexMetricFocus() { return theComplexMetricFocus; }
      
   bool done() const {
      return curr_ndx == theComponents.size();
   }
   const std::pair<const simpleMetric*, focus> &getCurrComponent() const {
      return theComponents[curr_ndx];
   }
   bool advanceToNextComponent() {
      // returns true iff done
      return ++curr_ndx == theComponents.size();
   }

   postCMFInstantiationCallback *getPostCMFInstantiationCallback() {
      return cb;
   }
};


class postSmfInstantiate : public cmfInstantiatePostSmfInstantiation {
 private:
   cmfInstantiationInfo *theCmfInstantiationInfo;

 public:
   postSmfInstantiate(cmfInstantiationInfo *iCmfInstantiationInfo) :
      cmfInstantiatePostSmfInstantiation(iCmfInstantiationInfo->getComplexMetricFocus()),
      theCmfInstantiationInfo(iCmfInstantiationInfo) {
   }

   void continueCMFInstantiation();
   void smfInstantiationHasCompleted() {
      theCmfInstantiationInfo->advanceToNextComponent();
         // we may or may not be done at this point.
         // continueInstantiationOfCMFComponents() will detect & act appropriately.
      continueCMFInstantiation();
   }
};
void postSmfInstantiate::continueCMFInstantiation() {
   complexMetricFocus &theComplexMetricFocus =
      theCmfInstantiationInfo->getComplexMetricFocus();
   
   if (!theCmfInstantiationInfo->done()) {
      // Subscribe to, and if needed, instantiate a simpleMetric/focus combination
      const std::pair<const simpleMetric *, focus> &theComponent =
         theCmfInstantiationInfo->getCurrComponent();
      const simpleMetric *theSimpleMetric = theComponent.first;
      const focus &theFocus = theComponent.second;

      extern allSimpleMetricFocuses *globalAllSimpleMetricFocuses;
      simpleMetricFocus *smf =
         globalAllSimpleMetricFocuses->subscribeTo(*theSimpleMetric, theFocus);
      assert(smf);

      sampleHandler &theHandler = theComplexMetricFocus.getSampleHandler();
      theHandler.addComponentSimpleMetricFocus(smf);

      assert(smf->getSubscribedRefCount() > 0);
      if (smf->getSubscribedRefCount() == 1) {
         postSmfInstantiate callback(theCmfInstantiationInfo);
            // the callback will simply bump up the current component ndx & re-call
            // this routine.

         theSimpleMetric->asyncInstantiate(*smf, callback);
      }
      else {
         // don't need to instantiate the smf; can continue manually (i.e., bump up
         // component ndx & recurse)
         theCmfInstantiationInfo->advanceToNextComponent();
         continueCMFInstantiation();
      }
   }
   else {
      // All done instantiating the individual components.
      // Invoke the CMF-all-done callback.

      postCMFInstantiationCallback &cb = *theCmfInstantiationInfo->getPostCMFInstantiationCallback();
      cb(theComplexMetricFocus);

      delete theCmfInstantiationInfo;
      theCmfInstantiationInfo = NULL;
   }
}

void complexMetric::asyncInstantiate(complexMetricFocus &theComplexMetricFocus,
                                     postCMFInstantiationCallback &cb) const {
   const focus &theFocus = theComplexMetricFocus.getFocus();
   assert(canInstantiateFocus(theFocus));
   
   const pdvector< std::pair<const simpleMetric*, focus> > componentStuffToInstantiate =
      simpleMetricsAndFocusesToInstantiate(theFocus);

   // we used to assert that componentStuffToInstantiate.size() > 0, but that doesn't
   // hold for codeSize.

   cmfInstantiationInfo *theCmfInstantiationInfo =
      new cmfInstantiationInfo(theComplexMetricFocus,
                               componentStuffToInstantiate, cb);
   assert(theCmfInstantiationInfo);

   postSmfInstantiate callback(theCmfInstantiationInfo);
   callback.continueCMFInstantiation();
}

// Fill "regions" with addresses: kernelAddr, kernelAddr + stride, ...
void complexMetric::generateVectorToSample(kptr_t kernelAddr,
					   unsigned elemsPerVector, 
					   unsigned bytesPerStride,
					   kapi_mem_region *regions) const
{
    for (unsigned i=0; i<elemsPerVector; i++) {
	regions[i].addr = kernelAddr;
	regions[i].nbytes = sizeof(sample_t);
	kernelAddr += bytesPerStride;
    }
}
