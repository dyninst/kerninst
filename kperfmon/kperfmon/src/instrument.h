// instrument.h

#ifndef _INSTRUMENT_H_
#define _INSTRUMENT_H_

#include "cmfSubscriber.h"
#include "allComplexMetricFocuses.h"
#include "metricFocusManager.h"
class Tcl_Interp;

class sampleCodeBufferer {
 public:
   struct oneSampleInfo {
      complexMetricFocus *cmf;
      
      oneSampleInfo(complexMetricFocus *icmf) : cmf(icmf) {
      }
      oneSampleInfo(const oneSampleInfo &src) :
         cmf(src.cmf) {
      }
      
    private:
      oneSampleInfo &operator=(const oneSampleInfo &);
   };
   
 private:
   unsigned numMetricFocusesBeingInstrumented;
   unsigned numMetricFocusesDoneInstrumented; // when equals above, can start sampling
   
   pdvector<oneSampleInfo> allTheSamplingCode;

   sampleCodeBufferer &operator=(const sampleCodeBufferer &);
   
 public:
   sampleCodeBufferer(unsigned inumMetricFocusesBeingInstrumented) :
      numMetricFocusesBeingInstrumented(inumMetricFocusesBeingInstrumented),
      numMetricFocusesDoneInstrumented(0) {
      allTheSamplingCode.reserve_exact(numMetricFocusesBeingInstrumented);
   }

   virtual ~sampleCodeBufferer() {}

   virtual void allTheSamplingCodeIsReady(const pdvector<oneSampleInfo> &) const = 0;
   
   bool handleSamplingCode(complexMetricFocus *cmf);
   bool noSamplingNeeded_AlreadyBeingDone();
};

class sampleCodeBufferer_genesisCMFSamplesWhenDone : public sampleCodeBufferer {
 private:
   sampleCodeBufferer_genesisCMFSamplesWhenDone(const sampleCodeBufferer_genesisCMFSamplesWhenDone &src);
   sampleCodeBufferer_genesisCMFSamplesWhenDone &operator=(const sampleCodeBufferer_genesisCMFSamplesWhenDone &);

 protected:
   metricFocusManager &theMFManager;

 public:
   sampleCodeBufferer_genesisCMFSamplesWhenDone(unsigned numMetricFocusPairs,
                                                metricFocusManager &m) :
      sampleCodeBufferer(numMetricFocusPairs),
      theMFManager(m) {
   }
   virtual ~sampleCodeBufferer_genesisCMFSamplesWhenDone() {}
   
   virtual void
   allTheSamplingCodeIsReady(const pdvector<oneSampleInfo> &theSamplingCodes) const;
};
class sampleCodeBufferer_genesisCMFSamplesWhenDone_verbose :
   public sampleCodeBufferer_genesisCMFSamplesWhenDone {
   typedef sampleCodeBufferer_genesisCMFSamplesWhenDone base_class;
 public:
   sampleCodeBufferer_genesisCMFSamplesWhenDone_verbose(unsigned numMetricFocusPairs,
                                           metricFocusManager &m) :
      base_class(numMetricFocusPairs, m) {
   }

   void allTheSamplingCodeIsReady(const pdvector<oneSampleInfo> &theSamplingCodes) const {
      base_class::allTheSamplingCodeIsReady(theSamplingCodes);
   }
};


void asyncInstrumentCurrentSelections_thenSample(cmfSubscriber &,
                                                 //uint64_t desiredSampleIntervalCycles,
                                                 Tcl_Interp *interp);

void asyncInstrument1Selection(cmfSubscriber &,
                               const complexMetric &theCMetric,
                               const focus &theFocus,
                               sampleCodeBufferer &howToHandleSamplingCode);

void unsubscribe(cmfSubscriber &, // this subscriber... [perhaps a visi]
                 complexMetricFocus &cmf // ...no longer wants this this metric/focus
                 );
   // uninstruments the kernel if applicable (if no subscribers are left subscribed
   // to cmf)

void fry1CmfSubscriberAndUninstrument(cmfSubscriber *theSubscriber,
                                      bool kperfmonGoingDownNow);
   // uninstruments, stops sampling, removes from global dictionary
   // of all cmf subscribers, and even calls delete on theSubscriber

void destroySomeCMFsDueToRemovalOfDownloadedKernelCode(kptr_t fnEntryAddr);

#endif
