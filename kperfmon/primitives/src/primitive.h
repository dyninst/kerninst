// primitive.h
// Ariel Tamches
// a virtual base class

#ifndef _PRIMITIVE_H_
#define _PRIMITIVE_H_

#include "util/h/kdrvtypes.h"
#include "kapi.h"

extern kapi_manager kmgr;

class primitive {
 private:
    // Compute log2(size)
    unsigned logSize(unsigned size) const;
 public:
    // Return a snippet that computes (cpu_id * cpu_stride)
    // Here, cpu_stride is the distance between instances of the
    // primitive for different CPUs.
    kapi_arith_expr computeCpuOffset() const;

    // Main method: generates AST tree for the primitive
    virtual kapi_snippet getKapiSnippet() const = 0;

    virtual ~primitive() {
    }

    virtual primitive *dup() const = 0;
   
    // Values for different CPUs are separated by that many bytes
    virtual unsigned getStrideSize() const = 0;
};

#endif
