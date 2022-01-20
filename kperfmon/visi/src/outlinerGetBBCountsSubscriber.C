// outlinerGetBBCountsSubscriber.C

#include "outlinerGetBBCountsSubscriber.h"
#include "util/h/hashFns.h"
#include "outlining1FnCounts.h"
#include "complexMetricFocus.h"

void outliner1CmfBucketer::processPerfectBucket(uint64_t startTime,
                                                uint64_t endTimePlus1,
                                                const pdvector<sample_t> &vals) {
   // super-strong assert:
   assert(endTimePlus1 - startTime == theSubscriber.getDelayInterval());

   // Sum accross all CPUS
   const int cpuid = cmf.getFocus().getCPUId();
   assert(cpuid == CPU_TOTAL);
   sample_t total = 0;
   for (unsigned i=0; i<vals.size(); i++) {
       total += vals[i];
   }
   
   if (startTime == getGenesisTime()) {
      // This must have been the first perfect bucket
       theSubscriber.setFirstPerfectBucket(cmf, total);
   }
   else {
       theSubscriber.setReplacementPerfectBucket(cmf, total);
   }
}

void outliner1CmfBucketer::processPerfectUndefinedBucket(uint64_t /*startTime*/,
                                                         uint64_t /*endTimePlus1*/) {
   assert(false);
}

// ----------------------------------------------------------------------

static unsigned cmfPtrHash(complexMetricFocus * const &cmfPtr) {
   const complexMetricFocus &cmf = *cmfPtr;

   std::pair<unsigned,unsigned> ids = cmf.getMetricAndFocusIds();
   return ids.first + ids.second;
}

// ----------------------------------------------------------------------

outlinerGetBBCountsSubscriber::
outlinerGetBBCountsSubscriber(uint64_t idelayInterval, // in cycles
                              uint64_t kerninstdMachineTime,
                              outlining1FnCounts &iOutliningInfo) :
      cmfSubscriber(kerninstdMachineTime),
      delayInterval(idelayInterval),
      theOutliningInfo(iOutliningInfo),
      focusid2bbid(unsignedIdentityHash),
      cmf2bucketer(cmfPtrHash)
{
}

outlinerGetBBCountsSubscriber::~outlinerGetBBCountsSubscriber() {
}

void outlinerGetBBCountsSubscriber::
newMetricFocusPair(complexMetricFocus &cmf, unsigned /*mhz*/) {
   const focus &theFocus = cmf.getFocus();
   const unsigned focusid = theFocus.getId();
   const bbid_t bbid = theFocus.getBBId();
   assert(bbid != (bbid_t)-1);

   theOutliningInfo.initialize1BBCount(bbid);
   focusid2bbid.set(focusid, bbid);
}

void outlinerGetBBCountsSubscriber::
disableMetricFocusPair(complexMetricFocus &/*cmf*/) {
   cout << "outlinerGetBBCountsSubscriber::ssDisableMetricFocusPairHook [undesired?]" << endl;
}

void outlinerGetBBCountsSubscriber::
reEnableDisabledMetricFocusPair(complexMetricFocus &/*theMetricFocus*/,
                                unsigned /*mhz*/) {
   cout << "outlinerGetBBCountsSubscriber::ssReEnableDisabledMetricFocusPairHook [undesired?]" << endl;
}

void outlinerGetBBCountsSubscriber::
subscriberHasRemovedCurve(complexMetricFocus &) {
   // Here is our opportunity to, for example, fry a bucketer for this cmf.
   // But this class doesn't need to do any such thing.

   //     cout << "outlinerGetBBCountsSubscriber::subscriberHasRemovedCurve"
   //          << endl;
}


void outlinerGetBBCountsSubscriber::ssCloseDownShopHook() {
   // This is where a subscriber gets a chance to close down; for example, the visi
   // subscriber will shut down its igen connection the visi process.  But this
   // subscriber doesn't need to do anything.

   //cout << "outlinerGetBBCountsSubscriber closeDownShop" << endl;
}

void outlinerGetBBCountsSubscriber::add_(complexMetricFocus &cmf,
                                         uint64_t startTime,
                                         uint64_t endTimePlus1,
                                         pdvector<sample_t> &valuesForThisTimeInterval) {
   // valuesForThisTimeInterval isn't const; we're allowed to trash its values.
   
   // First sample for this cmf?  If so, create bucketer (with genesis time equal to
   // cmf's genesis time).  Also, if first sample, then a cool assert: the sample's
   // startTime should exactly equal the cmf's genesis time.
   outliner1CmfBucketer *theBucketer;
   if (!cmf2bucketer.find(&cmf, theBucketer)) {
      assert(startTime == cmf.getGenesisTime());
         // super-strong assert; don't water down unless you know what you're doing.

      theBucketer =
         new outliner1CmfBucketer(*this, cmf,
                                  delayInterval,
                                     // bucket size in cycles
                                  cmf.getGenesisTime()
                                     // genesis time of bucketer...set it to the genesis
                                     // time of the cmf
                                  );
      assert(theBucketer);
      
      cmf2bucketer.set(&cmf, theBucketer);
   }
   else
      // not the first sample; assert as such:
      assert(startTime > cmf.getGenesisTime());
   
   theBucketer->add(startTime, endTimePlus1, valuesForThisTimeInterval);
}

void outlinerGetBBCountsSubscriber::setFirstPerfectBucket(complexMetricFocus &cmf,
                                                          uint64_t blockCount) {
   const bbid_t bbid = focusid2bbid.get(cmf.getFocus().getId());

//     cout << cmf.getFocus().getName() << " (bbid " << bbid
//          << "): " << blockCount << endl;

   theOutliningInfo.process1BBCount(bbid, blockCount);
      // we might think to unsubscribe after this arrives, but that would probably
      // lead to kernel uninstrumentation.  Best to not perturb that machine while
      // waiting for values to arrive.  Of course that pretty much guarantees that
      // replacement samples will be a part of life.  (Actually they'd be a part of
      // life anyway since time elapsed from taking the sample on kerninstd to
      // when we receive it, do some thinking, and async send the message back to
      // stop sampling.)
}

void outlinerGetBBCountsSubscriber::
setReplacementPerfectBucket(complexMetricFocus &cmf, uint64_t blockCount) {
   cout << "outliner subscriber: got a replacement bbcount of " << blockCount << endl;
   theOutliningInfo.process1ReplacementBBCount(focusid2bbid.get(cmf.getFocus().getId()),
                                               blockCount);
}

uint64_t outlinerGetBBCountsSubscriber::
getPresentSampleIntervalInCycles() const {
   return delayInterval;
}
