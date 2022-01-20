// complexMetric.h
// Information for a single complex metric
// Complex metric: contains high-level GUI info (name, description, units, how to
// fold), as well as one or more simple metrics.  Simple metrics perform
// instantiation (data and code allocation & initialization); complex metrics
// generate sampling code & interpret perfect bucket samples for visi consumption.

#ifndef _COMPLEX_METRIC_H_
#define _COMPLEX_METRIC_H_

#include <inttypes.h> // uint64_t
#include "common/h/String.h"
#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"
#include "focus.h"
#include "simpleMetric.h"
#include "kapi.h"

class complexMetricFocus; // fwd decl to avoid recursive #include's

class postCMFInstantiationCallback {
 private:
   // disallow copying:
   postCMFInstantiationCallback &operator=(const postCMFInstantiationCallback &);

 protected:
   postCMFInstantiationCallback(const postCMFInstantiationCallback &) {}
   
 public:
   postCMFInstantiationCallback() {}
   virtual ~postCMFInstantiationCallback() {}

   virtual postCMFInstantiationCallback *dup() const = 0;

   virtual void operator()(complexMetricFocus &theMetricFocus) = 0;
};

class complexMetric {
 public:
   enum metric_normalized {unnormalized=0, // we'll divide by bucket length for you
                           normalized=1 // don't divide by bucket length
                           };
   enum aggregate_types {sum=0, avg=1};
   
 private:
   unsigned id; // complex metric id
   unsigned clusterid; // cluster id is just for grouping of complexMetrics in the GUI.
   pdstring name;
   pdstring details;
      // a possibly long (say a paragraph) description of this metric, for GUI purposes.
   pdstring currentval_unitsname, totalval_unitsname;
   metric_normalized normalized_flag;
   aggregate_types aggregate_code;

   pdvector<const simpleMetric*> componentSimpleMetrics;

   // prevent copying:
   complexMetric(const complexMetric &src);
   complexMetric& operator=(const complexMetric &);

 protected:
   static pdvector<const simpleMetric *>
   make2(const simpleMetric &one, const simpleMetric &two) {
      // This little ditty of a routine helps derived classes pass the final
      // vector-of-all-component-simpleMetrics argument to complexMetric's ctor.
      pdvector<const simpleMetric *> result;
      result.reserve_exact(2);
      result += &one;
      result += &two;
      return result;
   }

   const pdvector<const simpleMetric*> &getComponentSimpleMetrics() const {
      return componentSimpleMetrics;
   }

 public:
   complexMetric(unsigned iid,
                 unsigned iclusterid,
                 const pdstring &iname,
                 const pdstring &idetails,
                 const pdstring &icurrentval_unitsname,
                 const pdstring &itotalval_unitsname,
                 metric_normalized inormalized_flag,
                 aggregate_types iaggtypes,
                 const pdvector<const simpleMetric*> &iComponentSimpleMetrics) :
         id(iid), clusterid(iclusterid),
         name(iname), details(idetails),
         currentval_unitsname(icurrentval_unitsname),
         totalval_unitsname(itotalval_unitsname),
         componentSimpleMetrics(iComponentSimpleMetrics)
   {
      normalized_flag = inormalized_flag;
      aggregate_code = iaggtypes;
   }

   virtual ~complexMetric() {}

   unsigned getId() const { return id; }
   const pdstring &getName() const { return name; }
   const pdstring &getDetails() const { return details; }
   unsigned getClusterId() const { return clusterid; }
   const pdstring &getCurrentValUnitsName() const { return currentval_unitsname;}
   const pdstring &getTotalValUnitsName() const { return totalval_unitsname; }
      
   metric_normalized getNormalizedFlag() const { return normalized_flag; }
   aggregate_types getAggregateCode() const { return aggregate_code; }

   // To help us avoid instantiating meaningless metricFocus pairs *early* in
   // the instantiation process:
   virtual bool canInstantiateFocus(const focus &theFocus) const;
      // The default version returns true iff *all* component simpleMetricFocuses
      // can instantiate this focus.  But that's not always the right thing to do
      // for every complexMetric (because the focus part of component
      // simpleMetricFocuses isn't always the same focus as the param "theFocus" here),
      // so we've made this fn virtual so that a particular complexMetric can
      // override this, if desired.

   virtual unsigned getNumComponentMetrics() const {
       // This is just a default version; any given derived complexMetric is 
       // free to override this if desired.
       return componentSimpleMetrics.size();
   }

   unsigned getNumberValuesToBucket() const;

   virtual pdvector< std::pair<const simpleMetric*, focus> >
   simpleMetricsAndFocusesToInstantiate(const focus &theFocus) const;
      // Turns complexMetric/focus pair into a vector of simpleMetric/focus pairs.
      // (The focus in the resulting vector may or may not be the same as "theFocus"
      // param to this routine; the default version keeps it the same.)
      // A very important routine; used by queryPostInstrumentationPcr(),
      // changeInPcrWouldInvalidateMF(), and asyncInstantiate().

   virtual kapi_hwcounter_set queryPostInstrumentationPcr(
       const kapi_hwcounter_set &oldPcrValue,
       const focus &ifocus) const;
      // default version calls queryPostInstrumentationPcr() for each
      // simpleMetric/focus combo of simpleMetricsAndFocusesToInstantiate().
   virtual bool changeInPcrWouldInvalidateMF(
       const kapi_hwcounter_set &pendingNewPcrValue,
       const focus &theFocus) const;
      // default version calls changeInPcrWouldInvalidateSMF() for each
      // simpleMetric/focus combo of simpleMetricsAndFocusesToInstantiate().

   void asyncInstantiate(complexMetricFocus &cmf,
                         postCMFInstantiationCallback &cb) const;
      // Relies on simpleMetricsAndFocusesToInstantiate(), and we take it from there.
      // Thanks to simpleMetricsAndFocusesToInstantiate(), this routine is generic and
      // works for every complexMetric.
      // This routine is asynchronous; postInstantiateCallback() will be
      // invoked some time in the future; no telling exactly when (depends how
      // much async work the metric needs to perform...e.g. concurrency may need to
      // mmap an object in kerninstd space)

   // Compute regions that we need to sample. Allocate "regions" with new
   virtual void calculateRegionsToSampleAndReport(
       complexMetricFocus &,
       kapi_mem_region **regions, unsigned *pNumRegions) const = 0;

   // Fill "regions" with addresses: kernelAddr, kernelAddr + stride, ...
   void generateVectorToSample(kptr_t kernelAddr,
			       unsigned elemsPerVector, 
			       unsigned bytesPerStride,
			       kapi_mem_region *regions) const;

   virtual double
   interpretValForVisiShipment(const pdvector<sample_t> &vals,
                                  // can be > 1 val, depending on the metric
                               unsigned mhz) const = 0;
      // This routine interprets raw sampled values (probably taken over a time
      // interval known as a "perfect bucket", though who really cares) and
      // converts them into a double value ready for shipment to a visi.
      // Where applicable, this is the routine that, e.g. converts cycles to
      // seconds and does other unit conversions.
};

#endif

