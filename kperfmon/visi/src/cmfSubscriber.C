// cmfSubscriber.C

#include "cmfSubscriber.h"
#include "allComplexMetricFocuses.h"
#include "allComplexMetrics.h"
#include "util/h/rope-utils.h" // num2str()
#include "util/h/hashFns.h"

// sorry for this:
extern allComplexMetricFocuses *globalAllComplexMetricFocuses;

dictionary_hash<unsigned, cmfSubscriber*> cmfSubscriber::allSubscribers(unsignedIdentityHash);
unsigned cmfSubscriber::nextUniqueId = 0;

cmfSubscriber::cmfSubscriber(uint64_t kerninstdMachineTime) :
      id(nextUniqueId++),
      cmfSubscriberGenesisTime(kerninstdMachineTime) {
   allSubscribers.set(id, this);
}

cmfSubscriber::~cmfSubscriber() {
   if (allSubscribers.get_and_remove(id) != this)
      assert(false);
}

void cmfSubscriber::closeDownShop() {
    // Hook for subscriber-specific closing down (i.e., visi subscribers will
    // fry their igen connections).
    ssCloseDownShopHook();
}

void cmfSubscriber::add(complexMetricFocus &cmf,
                        uint64_t startTime, uint64_t endTimePlus1,
                        pdvector<sample_t> &valuesForThisTimeInterval) {
   // valuesForThisTimeInterval is not const since its contents will get trashed.

   // Let's think this one through carefully: I'd love to assert that startTime is
   // >= cmfSubscriberGenesisTime.  But it's unsafe.  Consider the following
   // example: a complexMetricFocus is created by some other cmfSubscriber; samples have
   // already been received for it.  Furthermore, other samples for it are presently
   // on their way from kerninstd as we speak (but have not yet arrived here in
   // kperfmon).  Meanwhile, in this cmfSubscriber, we start (getting cmfSubscriber
   // genesis time from kerninstd via a sync igen call) and subscribe to that
   // complexMetricFocus.
   // The samples that are already on their way will have a timestamp of some time
   // *before* the cmfSubscriber's genesis time.  This would trip the assert.
   // So the way to handle it?  Discard such in-flight samples.  BUT, to salvage
   // some semblance of a useful assertion, we can assert that this discarding
   // situation will only happen when the bucketer has yet to received any add()
   // calls.

   assert(startTime >= cmf.getGenesisTime());
      // we can't assert that it's >= cmfSubscriberGenesisTime, but we can assert
      // that it's >= the cmf's own genesis time.

   if (startTime >= cmfSubscriberGenesisTime) {
      // the common case.  No time/values need to be stipped off.
      add_(cmf, startTime, endTimePlus1, valuesForThisTimeInterval);
      return;
   }

   if (endTimePlus1 <= cmfSubscriberGenesisTime) {
      // the entire interval needs to be ignored by this subscriber, since it entirely
      // takes place before our genesis
      return;
   }

   // *part* of the interval needs to be ignored; *some* of it took place before
   // our genesis.  Discard time and data from startTime to genesisTime-1.
   // Note that even normalized metrics will be scaled, or should we say that their
   // components are scaled.  (If we were dealing with a double that was the result
   // of those components, we would need to be cognizant of normalized v. unnormalized).
   // But component values are always monotonically increasing totals, so it's always
   // correct to scale them.

   const uint64_t newTimeLength = endTimePlus1 - cmfSubscriberGenesisTime;
   const uint64_t timeLengthWhereValIsDefined = endTimePlus1 - startTime;

   pdvector<sample_t> valuesToUse(valuesForThisTimeInterval);
   pdvector<sample_t>::iterator iter = valuesToUse.begin();
   pdvector<sample_t>::iterator finish = valuesToUse.end();
   typedef bucketer<sample_t> bucketer_type;
   for (; iter != finish; ++iter) {
      *iter = bucketer_type::scale(*iter,
                                   newTimeLength,
                                   timeLengthWhereValIsDefined);
   }

   add_(cmf,
        cmfSubscriberGenesisTime, // start time to use
        endTimePlus1, valuesToUse);
}
