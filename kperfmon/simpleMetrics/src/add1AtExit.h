// add1AtExit.h
// A simple metric

#ifndef _ADD1_AT_EXIT_H_
#define _ADD1_AT_EXIT_H_

#include "util/h/kdrvtypes.h"
#include "ctr64SimpleMetric.h"
#include "snippet.h"

class add1AtExit : public ctr64SimpleMetric {
 private:
   // prevent copying:
   add1AtExit(const add1AtExit &);
   add1AtExit &operator=(const add1AtExit &);

   // required by ctr64SimpleMetric:
   pdvector<kptr_t> getInstrumentationPoints(const focus &) const;
   snippet::placement getPlacement(const focus &) const;
   
 public:
   add1AtExit(unsigned iSimpleMetricId) : ctr64SimpleMetric(iSimpleMetricId) {}
  ~add1AtExit() {}
};

#endif
