// outlinerGetBBCountsSubscriber.h

#ifndef _OUTLINER_GETBBCOUNTS_SUBSCRIBER_H_
#define _OUTLINER_GETBBCOUNTS_SUBSCRIBER_H_

#include "cmfSubscriber.h"
#include "complexMetricFocus.h"
#include "util/h/Dictionary.h"
#include "util/h/bucketer.h"

class outlining1FnCounts; // avoid recursive #include
class outliningMgr; // avoid recursive #include
class outlinerGetBBCountsSubscriber; // fwd decl

class outliner1CmfBucketer : public bucketer<sample_t> {
 private:
   typedef bucketer<sample_t> base_class;
   
   outlinerGetBBCountsSubscriber &theSubscriber;
   complexMetricFocus &cmf;
   
   // required:
   void processPerfectBucket(uint64_t startTime, uint64_t endTimePlus1,
                             const pdvector<sample_t> &vals);
   void processPerfectUndefinedBucket(uint64_t startTime, uint64_t endTimePlus1);

 public:
   outliner1CmfBucketer(outlinerGetBBCountsSubscriber &iSubscriber,
                        complexMetricFocus &icmf,
                        uint64_t initialBucketSize, // in cycles
                        uint64_t bucketerGenesisTime) :
      base_class(icmf.getMetric().getNumberValuesToBucket(), // num values
                 initialBucketSize,
                 base_class::proportionalToTime),
      theSubscriber(iSubscriber),
      cmf(icmf) {
      
      setGenesisTime(bucketerGenesisTime);
   }

   // add() is unchanged from the base class
};

// ----------------------------------------------------------------------

class outlinerGetBBCountsSubscriber : public cmfSubscriber {
 private:
   typedef uint16_t bbid_t; // XXX -- ugly

   uint64_t delayInterval; // in cycles
   outlining1FnCounts &theOutliningInfo;

   dictionary_hash<unsigned, bbid_t> focusid2bbid;

   dictionary_hash<complexMetricFocus*, outliner1CmfBucketer*> cmf2bucketer;
      // a given cmf bucketer isn't created until its first sample arrived; this
      // is necessary because it's not until then that we know what value to use as its
      // (the bucketer's) genesis time.

   outlinerGetBBCountsSubscriber(const outlinerGetBBCountsSubscriber &);
   outlinerGetBBCountsSubscriber &operator=(const outlinerGetBBCountsSubscriber &);
   
 public:
   outlinerGetBBCountsSubscriber(uint64_t delayInterval, // in cycles
                                 uint64_t kerninstdMachineTime,
                                 outlining1FnCounts &);
  ~outlinerGetBBCountsSubscriber();

   uint64_t getDelayInterval() const { return delayInterval; }
   
   void newMetricFocusPair(complexMetricFocus &, unsigned mhz);
   void disableMetricFocusPair(complexMetricFocus &);
   void reEnableDisabledMetricFocusPair(complexMetricFocus &theMetricFocus,
                                        unsigned mhz);
   void subscriberHasRemovedCurve(complexMetricFocus &theMetricFocus);

   void ssCloseDownShopHook();

   void add_(complexMetricFocus &,
             uint64_t startTime, uint64_t endTime,
             pdvector<sample_t> &valuesForThisTimeInterval);
      // valuesForThisTimeInterval isn't const; we're allowed to trash
      // its contents

   uint64_t getPresentSampleIntervalInCycles() const;

   // These have to be public since they're called by outside code...the
   // outliner1CmfBucketer class, above.
   void setFirstPerfectBucket(complexMetricFocus &, uint64_t blockCount);
   void setReplacementPerfectBucket(complexMetricFocus &, uint64_t blockCount);
};


#endif
