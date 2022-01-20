// power_bitwise_dataflow_fn.C

#include "power_bitwise_dataflow_fn.h"

// Static vrble definitions must be in a .C file, to avoid "multiply-defined xxx"
// linker errors:

const power_bitwise_dataflow_fn 
power_bitwise_dataflow_fn::stopAllRegs(false, false);

const power_bitwise_dataflow_fn
power_bitwise_dataflow_fn::startAllRegs(false, true);

const power_bitwise_dataflow_fn
power_bitwise_dataflow_fn::passAllRegs(true, false);

const power_bitwise_dataflow_fn
power_bitwise_dataflow_fn::dummy_fn(false, false, false, false);

