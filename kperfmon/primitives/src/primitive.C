#include "primitive.h"
#include "dataHeap.h"
#include "kapi.h"

// Compute log2(size)
unsigned primitive::logSize(unsigned size) const
{
    assert(size == 64 && "Primitive of unsupported size");
    return 6;
}

// Return a snippet that computes (cpu_id * cpu_stride)
kapi_arith_expr primitive::computeCpuOffset() const
{
    unsigned cpu_stride = getStrideSize();
    unsigned log2_cpu_stride = logSize(cpu_stride);

    return kapi_arith_expr(kapi_shift_left, kapi_cpuid_expr(),
			   kapi_const_expr(log2_cpu_stride));
}

