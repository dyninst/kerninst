// vtimerCountedPrimitive.h
// Ariel Tamches

// Like vtimerPrimitive.h but is recursion-safe.  Due to the incidence of indirect
// recursion in the kernel, it is recommended that vtimerPrimitive.h be retired in
// favor of this file, once it's finished & debugged.

// The implementation of this class is a lot like cumulativeThreadEvents.[hC]: we
// have the same structure (64 bit total, 16 bit count, 48 bit start).  The only
// real difference is that on a stop operation, we don't try to multiply number of
// threads (count field) by the delta.  In addition, we only update total when
// count is decremented to zero, so everything about the "y-axis" is treated
// differently here.  Here we assume that count is due to (perhaps indirect) recursion,
// period, not due to execution by multiple threads of control.  That's the key
// difference.

// This class is generic enough to support many kinds of vtimer primitives; the key
// is the EmitCtr argument passed to the ctor...you can make a new class derived from
// EmitCtr to provide new functionality.

#ifndef _VTIMER_COUNTED_PRIMITIVE_H_
#define _VTIMER_COUNTED_PRIMITIVE_H_

#include <inttypes.h>
#include "util/h/kdrvtypes.h"
#include "primitive.h"

class vtimerCountedStartPrimitive : public primitive {
 private:
   kptr_t vtimer_addr;
   kapi_snippet rawCtrExpr;
   vtimerCountedStartPrimitive &operator=(const vtimerCountedStartPrimitive &);

 public:
   vtimerCountedStartPrimitive(kptr_t ivtimer_addr,
			       const kapi_snippet &iRawCtrExpr) :
       vtimer_addr(ivtimer_addr), rawCtrExpr(iRawCtrExpr) {
   }
   // Main method: generates AST tree for the primitive
   kapi_snippet getKapiSnippet() const;

   // Values for different CPUs are separated by that many bytes
   unsigned getStrideSize() const;
 
   primitive *dup() const { return new vtimerCountedStartPrimitive(*this); }
};

class vtimerCountedStopPrimitive : public primitive {
 private:
   kptr_t vtimer_addr;
   kapi_snippet rawCtrExpr;
    
   vtimerCountedStopPrimitive &operator=(const vtimerCountedStopPrimitive &);

 public:
   // Create a dummy stop primitive. It helps us emit the switch-out code
   vtimerCountedStopPrimitive() { 
   }

   vtimerCountedStopPrimitive(kptr_t ivtimer_addr, 
			      const kapi_snippet &iRawCtrExpr) :
	vtimer_addr(ivtimer_addr), rawCtrExpr(iRawCtrExpr) {
   }
   // Main method: generates AST tree for the primitive
   kapi_snippet getKapiSnippet() const;

   // Values for different CPUs are separated by that many bytes
   unsigned getStrideSize() const;

   primitive *dup() const { return new vtimerCountedStopPrimitive(*this); }
};

#endif
