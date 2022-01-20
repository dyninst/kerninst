// visiSubscriber.h
// Derived from cmfSubscriber

#ifndef _VISI_SUBSCRIBER_H_
#define _VISI_SUBSCRIBER_H_

#include "visi.xdr.CLNT.h"
#include "cmfSubscriber.h"
#include "util/h/foldingBucketer.h"

class visiSubscriber;

// The values being bucketed up by visi1CmfBucketer *still* correspond to rawly
// sampled values; in particular, the size can still be > 1.  Only later do
// we melt things down into a single-valued double, ready for visi consumption.
class visi1CmfBucketer :
public foldingBucketer<sample_t> { // the sample_t is a change
 private:
   visiSubscriber &theVisi;
   complexMetricFocus &cmf;
      // not const since visiBucketer has a dictionary with a non-const
      // complexMetricFocus key that would get in the way.
   
   // required by base class:
   void process1NumberedBucket(unsigned bucketNum, const pdvector<sample_t> &vals);
   void process1NumberedUndefinedBucket(unsigned bucketNum);
   void processFold(unsigned newMultipleOfBaseBucketWidth);

 public:
   visi1CmfBucketer(visiSubscriber &iVisi,
                    complexMetricFocus &icmf,
                    unsigned numValuesToBucket,
                    uint64_t initialBucketSize, // in cycles
                    uint64_t visiGenesisTime,
                    valueScalingOptions howToCombine) :
      foldingBucketer<sample_t>(numValuesToBucket,
                                   // no, numValues is NOT (yet) hardwired to 1.
                                   // We're not quite that close to the visi interface
                                   // yet.  We're still bucketing up rawly sampled
                                   // values.
                                initialBucketSize, howToCombine),
      theVisi(iVisi),
      cmf(icmf) {

      setGenesisTime(visiGenesisTime);
   }

   // add() is unchanged from base class
};

class visiSubscriber : public cmfSubscriber {
 private:
   visualizationUser *igenHandle;
   dictionary_hash<complexMetricFocus*, visi1CmfBucketer*> theBucketers;
      // one per complexMetricFocus that this visi is subscribed to.

   pdvector<dataValue> reusableDataBuffer; // we ship this to the visi itself

   const uint64_t baseBucketSizeCycles; // hardwired to 0.2 secs, right?
   unsigned bucketSizeMultiple; // due to folds

   unsigned kerninstdMachineMhZ;
   unsigned kerninstdMachineMhZTimes1000;

 public:
   visiSubscriber(int igenfd,
                  uint64_t kerninstdMachineTime,
                  unsigned ikerninstdMachineMhZ,
                  uint64_t ibucketSizeCycles);
  ~visiSubscriber();
   
   bool igenHandleExists() const { // true unless closeDownShop() has been executed
      return igenHandle != NULL;
   }

   visualizationUser *getIGenHandle() { return igenHandle; }
      // needed, e.g., so that outside code can do the mandatory igenHandle->waitLoop()

   int getIGenHandle_fd() const {
      return igenHandle->get_sock();
   }
   
   bool compareIgenHandle(const visualizationUser *cmp) const {
      return igenHandle == cmp;
   }

   void shipBufferedSampleDataToVisiIfAny(); 

   void newMetricFocusPair(complexMetricFocus &, unsigned mhz);
   void disableMetricFocusPair(complexMetricFocus &);
   void reEnableDisabledMetricFocusPair(complexMetricFocus &theMetricFocus,
                                        unsigned mhz);
   void subscriberHasRemovedCurve(complexMetricFocus &theMetricFocus);

   void ssCloseDownShopHook();
   
   // required by the base class:
   void add_(complexMetricFocus &,
                // can't be const since we want non-const dictionary key
             uint64_t startTime, uint64_t endTimePlus1,
             pdvector<sample_t> &valuesForThisTimeInterval);
      // note: trashes the contents of "valuesForThisTimeInterval", courtesy of
      // util/h/bucketer's add()

   // These must be public since they're invoked from visi1CmfBucketer class
   void processPerfectBucket(complexMetricFocus &,
                             unsigned bucketNum,
                             const pdvector<sample_t> &vals);
      // can't be const since we want non-const dictionary key

   void processPerfectUndefinedBucket(const complexMetricFocus &,
                                      unsigned bucketNum);
                                     
   void processFold(unsigned newMultiple);

   uint64_t getPresentBucketSizeInCycles() const {
      return baseBucketSizeCycles * bucketSizeMultiple;
   }
   uint64_t getPresentSampleIntervalInCycles() const {
      return baseBucketSizeCycles * bucketSizeMultiple;
   }
};

#endif
