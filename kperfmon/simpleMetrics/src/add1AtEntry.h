// add1AtEntry.h
// A simple metric

#ifndef _ADD1_AT_ENTRY_H_
#define _ADD1_AT_ENTRY_H_

#include "util/h/kdrvtypes.h"
#include "ctr64SimpleMetric.h"
#include "snippet.h"

class add1AtEntry : public ctr64SimpleMetric {
 private:
   // prevent copying:
   add1AtEntry(const add1AtEntry &);
   add1AtEntry &operator=(const add1AtEntry &);

   // required by ctr64SimpleMetric:
   pdvector<kptr_t> getInstrumentationPoints(const focus &) const;
   snippet::placement getPlacement(const focus &) const;
   
 public:
   add1AtEntry(unsigned iSimpleMetricId) : ctr64SimpleMetric(iSimpleMetricId) {}
  ~add1AtEntry() {}
};

#endif
