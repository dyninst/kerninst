// entriesTo.h
// A complex metric

#ifndef _ENTRIES_TO_H_
#define _ENTRIES_TO_H_

#include "ctr64ComplexMetric.h"
#include "complexMetricFocus.h"
#include "add1AtEntry.h" // the simple metric

class entriesTo : public ctr64ComplexMetric {
 private:
   // prevent copying:
   entriesTo(const entriesTo &);
   entriesTo &operator=(const entriesTo &);

 public:
   entriesTo(unsigned id, unsigned clusterid, const add1AtEntry &);
  ~entriesTo() {}
};

#endif
