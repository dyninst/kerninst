// add2IntCounter64.h
// Ariel Tamches
// A type of primitive, and thus derived from class primitive

// Here is the code we generate.  Note that we avoid clobbering condition codes,
// which would put the burden on someone to save & restore them.
//    sethi %hi(addr), R0
//    ldx   [R0 + %lo(addr)], R1  // note that there are 2 quick register insns before
//    // the casx...a good thing (hide the load latency perhaps)
//   retry:
//    // invariant: R1 contains the initial value
//    add R1, delta, R2
//    // invariants: R1 contains initial value; R2 contains new value
//    bset  %lo(addr), R0
//    // invariants: R0 contains the addr of the integer; R1 contains initial value;
//    //             R2 contains new value
//    // note: can leave this out if %lo(addr)==0
//    casx R0, R1, R2
//    // invariant: R2 contains the old value of M[R0].  If it equals R1
//    // then we've succeeded
//    sub R1, R2, R1 // perhaps xor would be faster
//    brnz, pn R1 retry // no need to annul, and UltraSparc user's guide says
//                      // annulling hurts performance.
//    mov R2, R1    // only needed if branch taken (cas failure); avoids need to re-load

#ifndef _ADD_2_INT_COUNTER64_H_
#define _ADD_2_INT_COUNTER64_H_

#include <inttypes.h> // uint32_t
#include "util/h/kdrvtypes.h"
#include "primitive.h"

class add2IntCounter64 : public primitive {
 private:
    kptr_t intCounterAddr;
    int delta;

    // disallow:
    add2IntCounter64 &operator=(const add2IntCounter64 &);

 public:
    add2IntCounter64(kptr_t iIntCounterAddr, int iDelta) : 
	intCounterAddr(iIntCounterAddr), delta(iDelta) {
    }

    // Main method: generates AST tree for the primitive
    kapi_snippet getKapiSnippet() const;

    // Values for different CPUs are separated by that many bytes
    unsigned getStrideSize() const;

    primitive *dup() const { return new add2IntCounter64(*this); }
};

#endif
