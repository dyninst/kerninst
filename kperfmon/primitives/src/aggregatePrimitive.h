// aggregatePrimitive.h

#ifndef _AGGREGATE_PRIMITIVE_H_
#define _AGGREGATE_PRIMITIVE_H_

#include "primitive.h"
#include "common/h/Vector.h"

class aggregatePrimitive : public primitive {
 private:
   pdvector<primitive *> thePrims; // ordered vector.  Should be change it to a vector of *const* primitive *?

 public:
   aggregatePrimitive(const pdvector<primitive *> &iprims) : thePrims(iprims) {
   }
  ~aggregatePrimitive() {
   }
};

#endif
