// templates.C

#include "sparc_reg.h"
#include "uimm.C"
#include "simm.C"

template void operator|=(unsigned &, uimmediate<5>);

template void operator|=(unsigned &, simmediate<30>);
template void operator|=(unsigned &, simmediate<22>);
template void operator|=(unsigned &, simmediate<7>);
template void operator|=(unsigned &, simmediate<10>);
template void operator|=(unsigned &, simmediate<13>);
template void operator|=(unsigned &, simmediate<19>);

#include "sparcTraits.h"
#include "mbdf_window.C"
#include "sparc_reg_set.h"
template class mbdf_window<sparcTraits>;

#include "sparc_instr.h"
#include "insnVec.C"
template class insnVec<sparc_instr>;

#include "util/h/refCounterK.h"
#include "util/src/HomogenousHeap.C"
#include "bitwise_dataflow_fn.h"
template class HomogenousHeap<refCounterK<monotone_bitwise_dataflow_fn_ll>::ss, 1>;

template class HomogenousHeap<refCounterK<pdvector<unsigned> >::ss, 1>;
// for refCounterK<unsigned>, used by simplePath

