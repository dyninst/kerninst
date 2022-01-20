// bucketer.C
// Ariel Tamches

#include "util/h/bucketer.h"
#include <assert.h>

template <class VAL>
bucketer<VAL>::bucketer(unsigned numValues, // numValues is usually 1
                        uint64_t ibucketSize,
                        valueScalingOptions iHowValuesAreCombined) :
   bucketStartTime(0), // undefined, really
   lastChangeTimePlus1(0), // undefined, really
   genesisTime(0), // undefined, really
   howValuesAreCombined(iHowValuesAreCombined),
   currValues(numValues, 0), // this many values, initialized to zero
      // the values are really uninitialized; they'll get set during "genesis"
   currValuesAreUndefined(true),
   bucketSize(ibucketSize)
{
   assert(!hasGenesisOccurredYet());
}

template <class VAL>
void bucketer<VAL>::reset() {
   // after a reset() you must go through the whole "genesis" thing again.

   bucketStartTime = 0; // undefined, really
   lastChangeTimePlus1 = 0; // undefined, really

   genesisTime = 0; // undefined, really
   assert(!hasGenesisOccurredYet());

   for (unsigned lcv=0; lcv < currValues.size(); ++lcv)
      currValues[lcv] = 0; // undefined, really
   currValuesAreUndefined = true;
}

template <class VAL>
void bucketer<VAL>::
changeBucketSize(uint64_t newBucketSize) {
   const bool shrunk = (newBucketSize < bucketSize);
   bucketSize = newBucketSize;

   if (shrunk) {
      // since the new bucket size may be (much?) smaller than the previous
      // size, we may already have enough for a ton of (new-sized) buckets:

      assert(hasGenesisOccurredYet());
      pdvector<VAL> deltavals(currValues.size(), 0); // all zeros
      add(lastChangeTimePlus1, deltavals);
   }
}

// ----------------------------------------------------------------------

template <class VAL>
void bucketer<VAL>::setGenesisTime(uint64_t gTime) {
   // note that we don't need to pass in any genesis values, since our "currValues[]"
   // aren't accumulating totals anyway, but rather the value of the present
   // bucket only.  In other words, check out add() and you'll see that delta-values
   // are already being passed in for us.

   assert(!hasGenesisOccurredYet());
   assert(gTime != 0 && "genesis time of 0 not supported");

   genesisTime = gTime;
   lastChangeTimePlus1 = genesisTime;
   bucketStartTime = genesisTime;

   assert(!hasAnythingBeenAddedYet());

   // assert that the values are 0.  We keep them as such.
   assert(currValuesAreUndefined);
   typename pdvector<VAL>::const_iterator iter = currValues.begin();
   typename pdvector<VAL>::const_iterator finish = currValues.end();
   for (; iter != finish; ++iter) {
      const VAL &v = *iter;
      assert(v == 0);
   }
}

// ----------------------------------------------------------------------

template <class VAL>
void bucketer<VAL>::addBlanksTo(uint64_t timePlus1) {
   assert(hasGenesisOccurredYet());
   assert(timePlus1 >= lastChangeTimePlus1);
   if (timePlus1 == lastChangeTimePlus1)
      // we're not adding anything!  Note that having this check way up here allows
      // us to keep the "assert(hasAnythingBeenAddedYet())" at the bottom of this func.
      return;

   // Reminder about lastChangeTimePlus1: it's at least bucketStartTime, in which
   // case 0 cycles of this bucket are defined.

   while (timePlus1 - bucketStartTime >= bucketSize) {
      // We have enough to finish filling up the current bucket

      // The following assert takes the place of assert(timeAlreadyFilled >= 0)
      // (which is meaningless because timeAlreadyFilled is *unsigned*)
      assert(lastChangeTimePlus1 >= bucketStartTime);

      const uint64_t timeAlreadyFilled = lastChangeTimePlus1 - bucketStartTime;
         // Thus, if lastChangeTimePlus1 == bucketStartTime, then 0 cycles are defined.

      const uint64_t timeToUse = bucketSize - timeAlreadyFilled;
      
      lastChangeTimePlus1 += timeToUse;
      assert(lastChangeTimePlus1 - bucketStartTime == bucketSize);
         // extra strength assert!  Don't water down unless you know what you're doing

      const uint64_t bucketEndTimePlus1 = bucketStartTime + bucketSize;
      assert(bucketEndTimePlus1 == lastChangeTimePlus1); // fairly strong assert
      
      // Process this undefined bucket:
      const uint64_t oldBucketSize = bucketSize; // important
         // important for the call to "startWorkingOnNextBucket()" to capture
         // bucketSize as it was *before* processPerfectBucket() has a chance
         // to, for example, double it (should this actually be a folding bucketer)
      
      if (currValuesAreUndefined)
         processPerfectUndefinedBucket(bucketStartTime, bucketEndTimePlus1);
      else {
         // Hmmm...part of the bucket is defined; this last part isn't.  All in all,
         // we say that it's defined.  So process the perfect bucket now.
         makeFullBucketCurrValuesPresentable();
            // XXX -- this isn't really correct, becuase it's going to divide
            // each value by the bucket length, when it should really divide
            // each value by the portion of the bucket length that contained
            // defined values.  Unfortunately, we don't store such information!!!

         processPerfectBucket(bucketStartTime, bucketEndTimePlus1, currValues);
      }

      startWorkingOnNextBucket(oldBucketSize);
      finishedProcessingBucket();
   }

   // Not enough for a whole bucket
   assert(timePlus1 >= lastChangeTimePlus1);
   lastChangeTimePlus1 = timePlus1;
      // i.e., values up to and including timePlus1-1 have been defined

   // leave currval as is: if it was undefined before now, it still is; if is was
   // defined before now, then it still is.

   assert(hasAnythingBeenAddedYet());
}

// ----------------------------------------------------------------------

template <class VAL>
void bucketer<VAL>::add(uint64_t timePlus1, pdvector<VAL> &deltavals) {
   // NOTE to caller: changes the contents of "deltavals"!
   assert(hasGenesisOccurredYet());
   assert(bucketSize > 0);
   assert(deltavals.size() == currValues.size());

   assert(timePlus1 >= lastChangeTimePlus1 && "rollback in bucketer.C?");
   assert(timePlus1 > bucketStartTime && "time < bucketStartTime not allowed");

   if (howValuesAreCombined == independentOfTime) {
      // behind the scenes, we're going to change deltavals[] to deltaareas[]
      const uint64_t definedTime = timePlus1 - lastChangeTimePlus1;
      
      typename pdvector<VAL>::iterator iter = deltavals.begin();
      typename pdvector<VAL>::iterator finish = deltavals.end();
      for (; iter != finish; ++iter)
         *iter *= definedTime;
   }
   
   while (timePlus1 - bucketStartTime >= bucketSize) {
      // we have enough for an entire bucket.  Fill it.

      assert(lastChangeTimePlus1 >= bucketStartTime);
         // if equal then we have an empty bucket

      const uint64_t timeAlreadyFilled = lastChangeTimePlus1 - bucketStartTime;
      
      const uint64_t timeAvailable = timePlus1 - lastChangeTimePlus1;
      const uint64_t timeToUse = bucketSize - timeAlreadyFilled;

      assert(deltavals.size() == currValues.size());
      
      for (unsigned vallcv=0; vallcv < deltavals.size(); ++vallcv) {
         // Whether we're bucketing averages (independentOfTime) or not,
         // the following adjustment is the right thing to do.

         const VAL valueToUse = scale(deltavals[vallcv],
                                      timeToUse, timeAvailable);
      
         currValues[vallcv] += valueToUse;
         assert(deltavals[vallcv] >= valueToUse);
            // assert that the immiment subtract won't underflow uint64_t, leading to
            // a huge value.
         deltavals[vallcv] -= valueToUse; // helps prepare us for the next pass
      }

      currValuesAreUndefined = false;
      lastChangeTimePlus1 += timeToUse;
      assert(lastChangeTimePlus1 - bucketStartTime == bucketSize);
         // extra strength assert.  Don't water down unless you know what you're
         // doing.

      const uint64_t bucketEndTimePlus1 = bucketStartTime + bucketSize;
      assert(bucketEndTimePlus1 == lastChangeTimePlus1); // fairly strong assert

      const uint64_t oldBucketSize = bucketSize;
         // important to capture the bucket size as it was *before*
         // processPerfectBucket() has a chance to, for example,
         // muck with it (think of a folding bucketer)

      processPerfectBucket(bucketStartTime, bucketEndTimePlus1, currValues);

      // Start working on a new bucket, which, as always, starts off with "undefined"
      // values.
      startWorkingOnNextBucket(oldBucketSize);
      finishedProcessingBucket();
   }

   // Not enough for a whole bucket
   assert(timePlus1 >= lastChangeTimePlus1);

   lastChangeTimePlus1 = timePlus1;
      // Time "time" has been defined, so lastChangeTimePlus1 must add 1

   // Add "deltavals[]" to "currValues[]":
   typename pdvector<VAL>::iterator cviter = currValues.begin();
   typename pdvector<VAL>::iterator cvfinish = currValues.end();
   typename pdvector<VAL>::const_iterator dviter = deltavals.begin();
   while (cviter != cvfinish)
      *cviter++ += *dviter++;

   currValuesAreUndefined = false;
}

// ----------------------------------------------------------------------

template <class VAL>
void bucketer<VAL>::maybeReduceBucketStartTime(uint64_t newBucketStartTime) {
   // In practice, this curious-looking routine is called when a foldingBucketer
   // has folded earlier than expected.  But of course, this routine knows nothing
   // about folding, so it denies the above sentence was ever written :)

   const uint64_t newBucketEndTimePlus1 = newBucketStartTime + getCurrBucketSize();
   assert(lastChangeTimePlus1 >= bucketStartTime && // equal --> bucket is empty
          lastChangeTimePlus1 <= newBucketEndTimePlus1 // equal --> bucket is full
          );

   const uint64_t oldDefinedBucketTime = lastChangeTimePlus1 - bucketStartTime;
   const uint64_t newDefinedBucketTime = lastChangeTimePlus1 - newBucketStartTime;
   assert(newDefinedBucketTime >= oldDefinedBucketTime);
   assert(newDefinedBucketTime <= bucketSize); // too strong an assert?

   assert(newBucketStartTime <= bucketStartTime);
   if (newBucketStartTime == bucketStartTime)
      return; // nothing needed
   
   bucketStartTime = newBucketStartTime;

   // Now scale values[] from oldDefinedBucketTime to newDefinedBucketTime
   if (!currValuesAreUndefined && oldDefinedBucketTime > 0) {
      typename pdvector<VAL>::iterator iter = currValues.begin();
      typename pdvector<VAL>::iterator finish = currValues.end();
      for (; iter != finish; ++iter)
         *iter = scale(*iter, newDefinedBucketTime, oldDefinedBucketTime);
   }

}


// ----------------------------------------------------------------------

template <class VAL>
VAL bucketer<VAL>::scale(VAL val,
                         uint64_t timeLengthToAdjustTo, // i.e., the new time length
                         uint64_t timeLengthWhereValIsDefined // the old time length
                         ) {
   // a static method.

   const VAL temp = val * timeLengthToAdjustTo;
      // I hope overflow won't happen (think uint64_t)
   const VAL result = temp / timeLengthWhereValIsDefined;
      // I hope that division won't lose precision (think uint64_t vs. double)
   return result;
}

