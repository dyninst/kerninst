#include "util/src/minmax.C"
#include "x86Traits.h"
#include "basicblock.C"
#include "funkshun.C"

#include "x86_basicblock.C"
#include "x86_kmem.C"
#include "x86_bitwise_dataflow_fn.h"
#include "x86_controlFlow.C"

template void ipmax(uint32_t &, uint32_t);

template class mbdf_window<x86Traits>;
template class basicblock<x86Traits>;
template class function<x86Traits>;
