// exitsFromNoUnwindTailCalls.h
// A complex metric

#ifndef _EXITS_FROM_NOUNWIND_TAILCALLS_H_
#define _EXITS_FROM_NOUNWIND_TAILCALLS_H_

#include "ctr64ComplexMetric.h"
#include "complexMetricFocus.h"
#include "add1AtExitNoUnwindTailCalls.h" // the simple metric

class exitsFromNoUnwindTailCalls : public ctr64ComplexMetric {
 private:
   // prevent copying:
   exitsFromNoUnwindTailCalls(const exitsFromNoUnwindTailCalls &);
   exitsFromNoUnwindTailCalls &operator=(const exitsFromNoUnwindTailCalls &);

 public:
   exitsFromNoUnwindTailCalls(unsigned id, unsigned clusterid,
                              const add1AtExitNoUnwindTailCalls &);
  ~exitsFromNoUnwindTailCalls() {}
};

#endif
