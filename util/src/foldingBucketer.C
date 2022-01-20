// foldingBucketer.C

#include "util/h/foldingBucketer.h"
#include <assert.h>

template <class VAL>
void foldingBucketer<VAL>::processPerfectBucket(uint64_t startTime,
                                                uint64_t endTimePlus1,
                                                const pdvector<VAL> &vals) {
   // Okay, now we've got a "real" bucket.  Process it.

   // It shouldn't be time to fold, though it may be time to after we add this bucket.
   unsigned currNumBuckets = lastFoldLeftThisManyBuckets + numBucketsAddedSinceLastFold;
   
   assert(currNumBuckets < numBucketsBeforeAFold);

   const uint64_t currBucketSize = getCurrBucketSize();
   assert(endTimePlus1 == startTime + currBucketSize);
      // strong assert; don't water down unless you know what you're doing

   assert(getGenesisTime() + currNumBuckets*currBucketSize == startTime);
      // very strong assert; don't water down unless you know what you're doing
   
   process1NumberedBucket(currNumBuckets, vals);
   ++numBucketsAddedSinceLastFold;

   // It might now be time to fold, but since the base class hasn't yet called
   // startWorkingOnNextBucket() (which correctly bumps up bucketStartTime, for
   // one thing), we don't yet do anything about it.  We must be patient and
   // wait for the soon-to-come call to finishedProcessingBucket() to check for folds.
}

template <class VAL>
void foldingBucketer<VAL>::processPerfectUndefinedBucket(uint64_t startTime,
                                                         uint64_t endTimePlus1) {
   // Note that much of this code is stolen from processPerfectBucket(), above.
   // It would be nice to share some code.

   // It shouldn't be time to fold, though it may be time to after we add this bucket.
   unsigned currNumBuckets = lastFoldLeftThisManyBuckets + numBucketsAddedSinceLastFold;
   assert(currNumBuckets < numBucketsBeforeAFold);

   const uint64_t currBucketSize = getCurrBucketSize();
   assert(endTimePlus1 == startTime + currBucketSize);
      // strong assert; don't water down unless you know what you're doing
   assert(getGenesisTime() + currNumBuckets*currBucketSize == startTime);
      // very strong assert; don't water down unless you know what you're doing
   
   process1NumberedUndefinedBucket(currNumBuckets);
   ++numBucketsAddedSinceLastFold;
   
   // It might now be time to fold, but since the base class hasn't yet called
   // startWorkingOnNextBucket() (which correctly bumps up bucketStartTime, for
   // one thing), we don't yet do anything about it.  We must be patient and
   // wait for the soon-to-come call to finishedProcessingBucket() to check for folds.
}

// ----------------------------------------------------------------------

template <class VAL>
void foldingBucketer<VAL>::finishedProcessingBucket() {
   // Time to fold?
   if (fold_if_appropriate())
      ;

   // It shouldn't be time to fold
   const unsigned currNumBuckets =
      lastFoldLeftThisManyBuckets + numBucketsAddedSinceLastFold;
   assert(currNumBuckets < numBucketsBeforeAFold);
}

template <class VAL>
bool foldingBucketer<VAL>::fold_if_appropriate() {
   bool folded = false;

   unsigned currNumBuckets = lastFoldLeftThisManyBuckets + numBucketsAddedSinceLastFold;
   assert(currNumBuckets <= numBucketsBeforeAFold);
   if (currNumBuckets == numBucketsBeforeAFold) {
      // Fold!
      folded = forceFoldUpToABaseMultiple(currMultipleOfBaseBucketWidth * 2);
      assert(folded);
   }
   currNumBuckets = lastFoldLeftThisManyBuckets + numBucketsAddedSinceLastFold;
   assert(currNumBuckets < numBucketsBeforeAFold);

   return folded;
}

template <class VAL>
bool foldingBucketer<VAL>::forceFoldUpToABaseMultiple(unsigned new_multiple) {
   // Called from the above routine, but also made public so the outside world
   // can call it.  Yes, that's been necessary in practice...when multiple buckets
   // exist and we want to keep their base bucket width in sync at all times.
   if (currMultipleOfBaseBucketWidth >= new_multiple)
      // we've already seen this multiple and have folded up to this point, so
      // nothing is needed.
      return false; // no changes

   unsigned numFolds = 0;
   
   while (currMultipleOfBaseBucketWidth < new_multiple) {
      // Fold once
      const uint64_t oldBucketSize = getCurrBucketSize();
      assert(oldBucketSize == currMultipleOfBaseBucketWidth * theBaseBucketSize);

      currMultipleOfBaseBucketWidth *= 2;
      ++numFolds;

      const unsigned oldNumBuckets =
         lastFoldLeftThisManyBuckets + numBucketsAddedSinceLastFold;
      lastFoldLeftThisManyBuckets = oldNumBuckets / 2;
         // if, as is the normal case, we are folding because we've completely filled
         // up [numBucketsBeforeAFold] buckets, then lastFoldLeftThisManyBuckets will
         // be 500, which is correct.  In the less common case, a fold can in theory
         // get forced at any time, in which case
         // this is also the correct thing to do.  (And yes, the division by 2 will
         // truncate, which is also exactly the right thing to do.)
      numBucketsAddedSinceLastFold = 0;
      
      changeBucketSize(oldBucketSize * 2);
         // Q: should this be deferred until *after* the "processFold" "callback"
         // is invoked below?  No I think this is correct as is.

      // Here's a tricky one: now that bucket sizes have doubled, the start time
      // for the current bucket might no longer be properly "aligned".  This happens,
      // for example, when we are force-folded after 999+ buckets.  So, reduce
      // its start time if appropriate.  The real tricky thing in this case is what
      // to assign values[] in such a case.  Think about this case: we have 999+change
      // buckets filled up (999 have been sent to the visi) when a fold command
      // comes in; each bucket has now doubled in size, so in theory we have
      // 499 buckets now (numbered 0 to 498, representing old buckets 0 to 997)
      // plus some change that represented old bucket 998 (all of it) and some
      // change beyond that.  OUR PROBLEM is that the visi, upon doing its fold,
      // will fold down to completed buckets only, meaning that its maximum
      // bucket number will now be 498.  That means it's our responsibility to
      // next send up number 499.  Bucket 499 contains the old buckets 998 and 999.
      // Unfortunately, we already shipped bucket ndx 998 (and thus we do not have
      // its contents any more!); we only have the change that comprised the part of
      // old bucket 999 that we were working on before the fold.  We need to think
      // of a way to recover the lost old bucket ndx 998 value!!!!  OPEN PROBLEM!!!
      // For now we take what we have (the change that is part of old bucket ndx 999)
      // and "stretch" it out to encompass old bucket ndx 998 too.
      
      const unsigned newNumBuckets =
         lastFoldLeftThisManyBuckets + numBucketsAddedSinceLastFold;
      const uint64_t newBucketStartTime =
         getGenesisTime() + newNumBuckets*getCurrBucketSize();

      maybeReduceBucketStartTime(newBucketStartTime);
   }

   // We don't want to execute the next line if there weren't any folds.
   // Hence the "if" statement at the entry to the function.
   assert(numFolds > 0);
   assert(currMultipleOfBaseBucketWidth == new_multiple);
   foldPending = true; // We have folded, now it's the visi's turn
   processFold(currMultipleOfBaseBucketWidth);

   return true;
}
