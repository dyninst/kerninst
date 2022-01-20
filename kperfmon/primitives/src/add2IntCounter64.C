// add2IntCounter64.C
// see .h file for comments

#include "add2IntCounter64.h"
#include "dataHeap.h"
#include "kapi.h"

// Main method: generates AST tree for the primitive
kapi_snippet add2IntCounter64::getKapiSnippet() const
{
    kapi_int_variable intCounter(intCounterAddr, computeCpuOffset());
    kapi_arith_expr rv(kapi_assign, intCounter, 
		       kapi_arith_expr(kapi_plus, intCounter, 
				       kapi_const_expr(delta)));

    // Finally, wrap the code with lock/unlock to protect from preemption
    // in the middle of the primitive.
    return kmgr.getPreemptionProtectedCode(rv);
}

// Return stride_size -- that many bytes separate counters for different CPUs
unsigned add2IntCounter64::getStrideSize() const
{
    extern dataHeap *theGlobalDataHeap;

    return theGlobalDataHeap->getBytesPerStride8();
}
