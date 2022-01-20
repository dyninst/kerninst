// exitsFrom.h
// A complex metric

#ifndef _EXITS_FROM_H_
#define _EXITS_FROM_H_

#include "ctr64ComplexMetric.h"
#include "complexMetricFocus.h"
#include "add1AtExit.h" // the simple metric

class exitsFrom : public ctr64ComplexMetric {
 private:
   // prevent copying:
   exitsFrom(const exitsFrom &);
   exitsFrom &operator=(const exitsFrom &);

 public:
   exitsFrom(unsigned id, unsigned iclusterid, const add1AtExit &);
  ~exitsFrom() {}
};

#endif
