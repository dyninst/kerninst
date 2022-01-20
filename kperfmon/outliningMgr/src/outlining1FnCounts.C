// outlining1FnCounts.C

#include "outlining1FnCounts.h"
#include "complexMetricFocus.h"
#include "instrument.h" // fry1CmfSubscriberAndUninstrument()
#include "allComplexMetricFocuses.h"
#include "util/h/minmax.h"
#include "blockCounters.h"

outlining1FnCounts::
outlining1FnCounts(blockCounters &iBlockCounters,
                   const pdstring &imodName, const kapi_function &ifn) :
   theBlockCounters(iBlockCounters),
   modName(imodName), fn(ifn),
   theGetBBCountsSubscriber(NULL),
   bbCounts(fn.getNumBasicBlocks(), -1U),
   numBBCountsReceived(0),
   toldOutlinedGroupAllBlocksRecvd(false) {
}

void outlining1FnCounts::fryGetBBCountsSubscriberAndUninstrumentIfExists() {
   if (theGetBBCountsSubscriber) {
      cout << "outlining1FnCounts dtor: detected a live bbCounts subscriber; frying it"
           << endl;

      fryGetBBCountsSubscriberAndUninstrument();
   }
   assert(NULL == theGetBBCountsSubscriber);
}

outlining1FnCounts::~outlining1FnCounts() {
   assert(NULL == theGetBBCountsSubscriber);
}

// --------------------

void outlining1FnCounts::kperfmonIsGoingDownNow() {
   fryGetBBCountsSubscriberAndUninstrumentIfExists();
   assert(NULL == theGetBBCountsSubscriber);
}

// --------------------

void outlining1FnCounts::fryGetBBCountsSubscriberAndUninstrument() {
   assert(theGetBBCountsSubscriber);

   fry1CmfSubscriberAndUninstrument(theGetBBCountsSubscriber, false); // instrumenter.C
      // false --> kperfmon is not dying now
      // This also removes form global dictionary of all cmf subscribers
   
   theGetBBCountsSubscriber = NULL; // so dtor doesn't get confused
}

// --------------------

void outlining1FnCounts::process1BBCount(bbid_t bb_id, unsigned count) {
   assert(!xreceivedAllBBCounts());
   assert(bbCounts[bb_id] == -1U);
   bbCounts[bb_id] = count;
   ++numBBCountsReceived;

   if (xreceivedAllBBCounts() && !toldOutlinedGroupAllBlocksRecvd) {
      theBlockCounters.blockCountsFor1FnHaveBeenReceived(fn);
      toldOutlinedGroupAllBlocksRecvd = true;
      assert(xallDoneCollectingAndTelling()); // but we *haven't* yet uninstrumented!
   }
}

void outlining1FnCounts::process1ReplacementBBCount(bbid_t bb_id, unsigned count) {
   assert(bbCounts[bb_id] != -1U);
   bbCounts[bb_id] = count;
      // we do NOT increment numBBCountsReceived
}

