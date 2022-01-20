// x86_bitwise_dataflow_fn.C

#include "x86_bitwise_dataflow_fn.h"

// static member declaration:
const x86_bitwise_dataflow_fn
x86_bitwise_dataflow_fn::passAllRegs(true, false);

const x86_bitwise_dataflow_fn
x86_bitwise_dataflow_fn::startAllRegs(false, true);

const x86_bitwise_dataflow_fn
x86_bitwise_dataflow_fn::stopAllRegs(false, false);

const x86_bitwise_dataflow_fn
x86_bitwise_dataflow_fn::dummy_fn(false, false, false, false);
