// ctr64SimpleMetric.h
// Allows for some sharing of code between the simple metrics
// add1AtEntry, add1AtExit, and add1AtExitNoUnwindTC

#ifndef _CTR64_SIMPLE_METRIC_H_
#define _CTR64_SIMPLE_METRIC_H_

#include "util/h/kdrvtypes.h"
#include "common/h/Vector.h"
#include "simpleMetric.h"
#include "snippet.h"

class simpleMetricFocus;
class tempBufferEmitter;

class ctr64SimpleMetric : public simpleMetric {
 private:
   // prevent copying:
   ctr64SimpleMetric(const ctr64SimpleMetric &);
   ctr64SimpleMetric &operator=(const ctr64SimpleMetric &);
   
   unsigned getNumValues() const { return 1; }
      // required by the base class

   // Initialize the counter data in the kernel memory
   void initialize_counter(kptr_t ctrAddrInKernel,
			   unsigned elemsPerVector,
			   unsigned bytesPerStride) const;

   virtual pdvector<kptr_t> getInstrumentationPoints(const focus &) const = 0;
   virtual snippet::placement getPlacement(const focus &) const = 0;

   virtual bool canInstantiateFocus(const focus &theFocus) const;
      // we'll return true so long as the function has been successfully parsed
      // into basic blocks.  Virtual, so a given derived class can override, should
      // it wish to.

 public:
   ctr64SimpleMetric(unsigned iSimpleMetricId) : simpleMetric(iSimpleMetricId) {}
   virtual ~ctr64SimpleMetric() {}
   
   void asyncInstantiate(simpleMetricFocus &smf,
                         cmfInstantiatePostSmfInstantiation &cb) const;
      // required by the base class
};

#endif
