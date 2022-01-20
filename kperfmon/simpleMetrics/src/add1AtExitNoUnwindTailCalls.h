// add1AtExitNoUnwindTailCalls.h
// A simple metric

#ifndef _ADD1_AT_EXIT_NO_UNWIND_TAILCALLS_H_
#define _ADD1_AT_EXIT_NO_UNWIND_TAILCALLS_H_

#include "util/h/kdrvtypes.h"
#include "ctr64SimpleMetric.h"
#include "snippet.h"

class add1AtExitNoUnwindTailCalls : public ctr64SimpleMetric {
 private:
   // prevent copying:
   add1AtExitNoUnwindTailCalls(const add1AtExitNoUnwindTailCalls &);
   add1AtExitNoUnwindTailCalls &operator=(const add1AtExitNoUnwindTailCalls &);

   // required by ctr64SimpleMetric:
   pdvector<kptr_t> getInstrumentationPoints(const focus &) const;
   snippet::placement getPlacement(const focus &) const;
   
 public:
   add1AtExitNoUnwindTailCalls(unsigned iSimpleMetricId) : ctr64SimpleMetric(iSimpleMetricId) {}
  ~add1AtExitNoUnwindTailCalls() {}
};

#endif
