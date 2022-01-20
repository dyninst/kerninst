// bucketer.h
// Ariel Tamches

#ifndef _BUCKETER_H_
#define _BUCKETER_H_

#include <inttypes.h> // uint64_t
#include "common/h/Vector.h"

template <class VAL>
class bucketer {
 public:
   typedef enum {proportionalToTime, independentOfTime} valueScalingOptions;

 private:
   uint64_t bucketStartTime; // in cycles

   // lastChangeTimePlus1: if it equals bucketStartTime, then 0 cycles of this
   // bucket are defined.  The "Plus1" is in the spirit of STL iterators; it really
   // does make things a bit simpler.
   uint64_t lastChangeTimePlus1; // cycles

   uint64_t genesisTime; // only set once.  0 --> no genesis yet

 protected:
   valueScalingOptions howValuesAreCombined;
      // Usual is "proportionalToTime".  If "independentOfTime"
      // (e.g. bucketing up a normalized IPC metric), then within "currValues[]",
      // we'll actually store the *area* under the curve (this makes combining
      // such values as easy as the proportionalToTime case).  Don't worry, by
      // the time we call processPerfectBucket(), we'll divide by the time interval,
      // thus ending the charade, so the outside world never has to know of our
      // subterfuge.

 private:
   // The value(s) present in the current bucket: (this bucket only, not an
   // accumulating total; always gets reset to 0 when we start working on a new bucket)
   pdvector<VAL> currValues; // a fixed-sized vector that we reuse
   bool currValuesAreUndefined;

   uint64_t bucketSize;
      // in cycles.  Used to be const, but now bucket sizes can be changed at runtime.

   // ----------------------------------------------------------------------

   void makeFullBucketCurrValuesPresentable() {
      if (howValuesAreCombined == independentOfTime) {
         // up till now, we've been deviously accumulating area of the curve within
         // values[].  To average out those "Reimann sums", simply divide by
         // the total time interval
         typename pdvector<VAL>::iterator iter = currValues.begin();
         typename pdvector<VAL>::iterator finish = currValues.end();
         for (; iter != finish; ++iter)
            *iter /= bucketSize;
      }
   }

   // In the next 2 routines, endTime will equal startTime + bucketSize - 1
   virtual void processPerfectBucket(uint64_t startTime, uint64_t endTimePlus1,
                                     const pdvector<VAL> &vals) = 0;
   virtual void processPerfectUndefinedBucket(uint64_t startTime,
                                              uint64_t endTimePlus1) = 0;
   virtual void finishedProcessingBucket() { }
      // called after *both* processPerfectBucket/processPerfectUndefinedBucket AND
      // startWorkingOnNextBucket().  Here's a good time for a derived foldingBucketer
      // class to check for folds.  Of course, in this class, I have never heard of
      // a "fold" and strenuously deny that this is a hack to accomodate such
      // curiousities. :)

   void startWorkingOnNextBucket(uint64_t oldBucketSize) {
      // "oldBucketSize" is "bucketSize" as it was before the call to
      // processPerfectBucket() or processPerfectUndefinedBucket() which surely
      // immediately preceded this routine being called.  Since those routines have
      // an opportunity to change "bucketSize", it was pretty important to set aside
      // the prior value of bucket size so that we add the correct value to
      // bucketStartTime.

      bucketStartTime += oldBucketSize;
      assert(bucketStartTime == lastChangeTimePlus1);
         // strong assert; don't water down unless you know what you're doing

      currValuesAreUndefined = true;
      typename pdvector<VAL>::iterator iter = currValues.begin();
      typename pdvector<VAL>::iterator finish = currValues.end();
      for (; iter != finish; ++iter)
         *iter = 0;
   }
   
   void addBlanksTo(uint64_t timePlus1);
   void add(uint64_t timePlus1, pdvector<VAL> &deltavals);
      // usually the vector is a single element, but some bucketers
      // have multiple values.
      // NOTE: changes the values in deltavals[]

   // prevent copying:
   bucketer(const bucketer &);
   bucketer& operator=(const bucketer &);
   
 public:
   bucketer(unsigned numValues, uint64_t initialBucketSize, valueScalingOptions);
      // you still need to call setGenesisTime() before calling add()
   virtual ~bucketer() {}

   virtual void reset();
      // It's anticipated that the derived class will do some work and then call us

   bool hasGenesisOccurredYet() const { return genesisTime != 0; }
   uint64_t getGenesisTime() const {
      assert(hasGenesisOccurredYet());
      return genesisTime;
   }
   virtual void setGenesisTime(uint64_t gTime);
      // call this after the constructor, and before calling add() for the
      // first time.  virtual because derived class foldingBucketer wants to do
      // a smidge of additional work at this time.
   
   bool hasAnythingBeenAddedYet() const {
      assert(hasGenesisOccurredYet());
      assert(lastChangeTimePlus1 >= genesisTime);
      return (lastChangeTimePlus1 > genesisTime);
   }

   void changeBucketSize(uint64_t newBucketSize);
   
   virtual uint64_t getCurrBucketSize() const {
      // new feature: the bucket size can change at runtime.
      // This routine must be virtual because (e.g.) foldingBucketer provides
      // its own version.
      return bucketSize;
   }

   // The following 2 aren't virtual because I don't anticipate any derived class
   // having any need to override it.
   void add(uint64_t startTime, uint64_t endTimePlus1,
            pdvector<VAL> &trashableDeltaVals) {
      // NOTE: the contents of (but not the size of) trashableDeltaVals will be,
      // well, um, trashed by this call.  (In case you're wondering, there's a good
      // reason: it allows add() to avoid making a copy of the vector while it
      // makes intermediate calculations.)  That's why it's not a const vector.

      // startTime and endTime must be absolute times, each >= genesisTime
      assert(hasGenesisOccurredYet());
      assert(endTimePlus1 > startTime && "don't call bucketer::add() w/zero time interval");
      assert(trashableDeltaVals.size() == currValues.size());

      if (startTime > lastChangeTimePlus1)
         // there is some blank time
         addBlanksTo(startTime); // startTime will be endOfBlanksTimePlus1

      assert(endTimePlus1 >= lastChangeTimePlus1);

      add(endTimePlus1, trashableDeltaVals);
         // trashes trashtableDeltaVals (w/o changing its .size())
   }

   void add_undefinedx(uint64_t /* startTime */, uint64_t endTimePlus1) {
      addBlanksTo(endTimePlus1);
   }

   void maybeReduceBucketStartTime(uint64_t newBucketStartTime);

   static VAL scale(VAL ival,
                    uint64_t timeLengthToAdjustTo, // new length
                    uint64_t timeLengthWhereValIsDefined // old
                    );
};

#endif
