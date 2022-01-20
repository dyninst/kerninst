// foldingBucketer.h

#ifndef _FOLDING_BUCKETER_H_
#define _FOLDING_BUCKETER_H_

#include "bucketer.h"

template <class VAL>
class foldingBucketer : public bucketer<VAL> {
   typedef bucketer<VAL> inherited;
 private:
   const uint64_t theBaseBucketSize;
   unsigned currMultipleOfBaseBucketWidth;
   unsigned numBucketsAddedSinceLastFold;
      // usually a fold halves the # of buckets (e.g. from 1000 to 500); this tells
      // how many past that 500 we've added.
   unsigned lastFoldLeftThisManyBuckets;
      // this is the value discussed above that is presumably 500 (but since a fold
      // can be forced at any time, we don't assume this).  No folds yet --> 0

   // The bucketer has folded, but the visi has not (yet). It may be waiting
   // for other bucketers to fold first
   bool foldPending;

   bool fold_if_appropriate();
      // returns true iff we actually folded

   // The following two are required by the base class.  They deal with perfect
   // buckets.
   void processPerfectBucket(uint64_t startTime, uint64_t endTimePlus1,
                             const pdvector<VAL> &vals);
   void processPerfectUndefinedBucket(uint64_t startTime, uint64_t endTimePlus1);

   void finishedProcessingBucket();
      // Here, *not* in processPerfectBucket(), is where we check for a fold.
      // This is new.

 protected:
   static const unsigned numBucketsBeforeAFold = 1000; // constant forever
      // TODO -- actually this shouldn't be so hard-coded.

   // The following deal with *macro* buckets.
   virtual void process1NumberedBucket(unsigned bucketNum,
                                       const pdvector<VAL> &val) = 0;
      // "val" will be properly massaged to reflect "howToScale"
   virtual void process1NumberedUndefinedBucket(unsigned bucketNum) = 0;
   virtual void processFold(unsigned newMultipleOfBaseBucketWidth) = 0;
   
 public:
   foldingBucketer(unsigned numValues,
                   uint64_t ibaseBucketSize,
                   valueScalingOptions howToCombine) :
      inherited(numValues, ibaseBucketSize, howToCombine),
      theBaseBucketSize(ibaseBucketSize),
      currMultipleOfBaseBucketWidth(1),
      numBucketsAddedSinceLastFold(0),
      lastFoldLeftThisManyBuckets(0),
      foldPending(false) {
   }
   virtual ~foldingBucketer() { }
   
   // no copy-ctor defined (on purpose: the default [bit copy] works just fine)

   virtual uint64_t getCurrBucketSize() const {
      // "bucketSize" comes from the base class
      const uint64_t result = inherited::getCurrBucketSize();
      
      assert(result == currMultipleOfBaseBucketWidth * theBaseBucketSize);
      return result;
   }

   unsigned getBucketSizeMultiple() const {
       return currMultipleOfBaseBucketWidth;
   }

   void setGenesisTime(uint64_t gTime) {
      inherited::setGenesisTime(gTime);
   }
   
   // add() and add_undefined() are unchanged from the base class.
   // Same for setGenesisTime().
   // We only need to override the processPerfectBucket() routine.

   virtual void reset() {
      inherited::reset();

      currMultipleOfBaseBucketWidth = 1;
      numBucketsAddedSinceLastFold = 0;
      lastFoldLeftThisManyBuckets = 0;
   }

   bool forceFoldUpToABaseMultiple(unsigned multiple);
      // returns true iff anything changed (false if we've already seen this
      // multiple)

   // True if the bucketer has folded, but the visi has not. 
   // It may be waiting for other bucketers to fold first
   bool isFoldPending() {
       return foldPending;
   }
   // The visi has finally folded. Drop the flag.
   void foldCompleted() {
       foldPending = false;
   }
};

#endif
