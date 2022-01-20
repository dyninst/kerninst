// outlining1FnCounts.h
// This class manages collecting block counts for a single function.
// Our helper class "outlininerGetBBCountsSubscriber" does most of the work for us.

#ifndef _OUTLINING_1FN_COUNTS_H_
#define _OUTLINING_1FN_COUNTS_H_

#include "kapi.h"
#include "common/h/String.h"
#include "outlinerGetBBCountsSubscriber.h"

class blockCounters; // avoid recursive #include

class outlining1FnCounts {
 private:
   // A reference back to our "parent" class:
   blockCounters &theBlockCounters;
   
   // Identify the function:
   pdstring modName;
   kapi_function fn;

   // The cmfSubscriber (pseudo-visi, if you will) for collecting block counts
   // of this function.
   // QUESTION: is it proper to have one per function; perhaps
   //   we should have one per multi-function-group that we're presently collecting
   //   block counts for.)
   // QUESTION: should this be a pointer?  Yes I think so, because we want to 
   //   be able to fry the subscriber once block counts have been collected, yet
   //   not fry the entire "outlining1FnCounts" object, so that one can leisurely
   //   query the collected block counts at any time.
   outlinerGetBBCountsSubscriber *theGetBBCountsSubscriber;

   pdvector<unsigned> bbCounts; // indexed by bb_id.  -1U indicates not yet received
   unsigned numBBCountsReceived;
   bool toldOutlinedGroupAllBlocksRecvd;

   outlining1FnCounts(const outlining1FnCounts &);
   outlining1FnCounts &operator=(const outlining1FnCounts &);

 public:
   outlining1FnCounts(blockCounters &, const pdstring &modName,
		      const kapi_function &ifn);
  ~outlining1FnCounts();

   void kperfmonIsGoingDownNow();
      // clean everything up (dtor will assert as such)

   // --------------------

   const pdstring &getModName() const { return modName; }
   const kapi_function &getFn() const { return fn; }

   // --------------------

   const cmfSubscriber &getCmfSubscriber() const {
      assert(theGetBBCountsSubscriber);
      return *theGetBBCountsSubscriber;
   }
   cmfSubscriber &getCmfSubscriber() {
      assert(theGetBBCountsSubscriber);
      return *theGetBBCountsSubscriber;
   }

   void createGetBBCountsSubscriber(uint64_t delayInterval, // in cycles
                                    uint64_t kerninstdMachineTime) {
      assert(NULL == theGetBBCountsSubscriber);
      theGetBBCountsSubscriber =
         new outlinerGetBBCountsSubscriber(delayInterval, kerninstdMachineTime, *this);
      assert(theGetBBCountsSubscriber);
   }
   
   void fryGetBBCountsSubscriberAndUninstrument();
   void fryGetBBCountsSubscriberAndUninstrumentIfExists();

   // --------------------

   void initialize1BBCount(bbid_t bb_id) {
      assert(bbCounts[bb_id] == -1U);
   }

   void process1BBCount(bbid_t bb_id, unsigned count);
   void process1ReplacementBBCount(bbid_t bb_id, unsigned count);
   
   bool xreceivedAllBBCounts() const {
      return numBBCountsReceived == fn.getNumBasicBlocks();
   }
   bool xallDoneCollectingAndTelling() const {
      // returns true iff all block counts have been received *AND* we've informed
      // the outlining mgr of such.  (The 'and' part is important)
      if (toldOutlinedGroupAllBlocksRecvd) {
         assert(xreceivedAllBBCounts());
         return true;
      }
      else
         return false;
   }
   bool allDoneCollectingAndTellingAndUninstrumenting() const {
      return xallDoneCollectingAndTelling() && theGetBBCountsSubscriber == NULL;
   }

   // --------------------

   const pdvector<unsigned> &getAllBlockCounts() const {
      assert(xreceivedAllBBCounts());
      return bbCounts;
   }
};

#endif
