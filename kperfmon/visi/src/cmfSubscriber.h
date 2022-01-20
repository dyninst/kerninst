// cmfSubscriber.h
// A cmfSubscriber (the cmf stands for complexMetricFocus) is one who subscribes
// to (that is, samples values from) a complexMetricFocus.  That sounds harder to
// understand than it really is: the typical example of a cmfSubscriber
// is simply a "visi".  (Although there are others, such as outlining managers.)

#ifndef _CMF_SUBSCRIBER_H_
#define _CMF_SUBSCRIBER_H_

#include <inttypes.h>
#include "util/h/Dictionary.h"
#include "kapi.h" // sample_t

// ----------------------------------------------------------------------

class complexMetricFocus; // fwd reference needed to avoid recursive #include

class cmfSubscriber {
 private:
   static dictionary_hash<unsigned, cmfSubscriber*> allSubscribers;
      // we add to it in ctor; remove in dtor
   static unsigned nextUniqueId;

   unsigned id;
   
   uint64_t cmfSubscriberGenesisTime;
      // kerninstd-machine time when this cmfSubscriber was started.
   
   // prevent copying:
   cmfSubscriber(const cmfSubscriber &);
   cmfSubscriber& operator=(const cmfSubscriber &);

   virtual void ssCloseDownShopHook() = 0;
      // ss stands for subscriber-specific

 public:
   cmfSubscriber(uint64_t kerninstdMachineTime);
      // param: genesis time (but on the *kerninstd* machine) of the visi process
   virtual ~cmfSubscriber();

   uint64_t getGenesisTime() const {
      return cmfSubscriberGenesisTime;
   }
   
   static cmfSubscriber *findCmfSubscriber(unsigned id) {
      cmfSubscriber *result = NULL;
      if (!allSubscribers.find(id, result))
         return NULL;
      else
         return result;
   }
   static const dictionary_hash<unsigned, cmfSubscriber*> &getAllCmfSubscribers() {
      return allSubscribers;
   }

   unsigned getId() const { return id; }
   
   virtual void newMetricFocusPair(complexMetricFocus &, unsigned mhz) = 0;
   virtual void disableMetricFocusPair(complexMetricFocus &) = 0;
   virtual void reEnableDisabledMetricFocusPair(complexMetricFocus &theMetricFocus,
                                                unsigned mhz) = 0;
   virtual void subscriberHasRemovedCurve(complexMetricFocus &) = 0;

   // Perform some cleanup upon deletion
   void closeDownShop();

   void add(complexMetricFocus &, // can't be const
            // needed at least for the metric component, which can massage
            // the delta values into something the cmfSubscriber feels
            // comfortable with.
            uint64_t startTime, uint64_t endTimePlus1,
            pdvector<sample_t> &valuesForThisTimeInterval
               // NOT accumulating totals.  Last param isn't const since class bucketer
               // mucks with the values while doing its work.
            );
      // does one thing: strips off time & values that occurred before the subscriber's
      // genesis time -- if any -- and then calls add_ (note the underbar), below.
      // valuesForThisTimeInterval isn't const because its contents will get trashed.

   virtual void add_(complexMetricFocus &,
                        // can't be const (see visiSubscriber for an example why)
                     uint64_t startTime, uint64_t endTimePlus1,
                     pdvector<sample_t> &valuesForThisTimeInterval
                        // NOT accumulating totals
                        // valuesForThisTimeInterval isn't const; we allow
                        // its contents to get trashed by the call
                     ) = 0;
      // called from add(), above.
      // valuesForThisTimeInterval isn't const because its contents will get trashed.
   
   virtual uint64_t getPresentSampleIntervalInCycles() const = 0;
      // needed by complexMetricFocus, for recalculating its sample interval
};

#endif
