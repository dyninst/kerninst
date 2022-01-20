// templates.C

#include "uimm.C"
template class uimmediate<1>;
template class uimmediate<2>;
template class uimmediate<3>;
template class uimmediate<4>;
template class uimmediate<5>;
template class uimmediate<6>;
template class uimmediate<8>;
template class uimmediate<13>;
template class uimmediate<14>;
template class uimmediate<22>;

#include "simm.C"
template class simmediate<7>;
template class simmediate<10>;
template class simmediate<13>;
template class simmediate<16>;
template class simmediate<19>;
template class simmediate<22>;
template class simmediate<30>;

template void operator|=(unsigned &, simmediate<7>);
template void operator|=(unsigned &, simmediate<10>);
template void operator|=(unsigned &, simmediate<13>);
template void operator|=(unsigned &, simmediate<16>);
template void operator|=(unsigned &, simmediate<19>);
template void operator|=(unsigned &, simmediate<22>);
template void operator|=(unsigned &, simmediate<30>);

#include "util/src/minmax.C"
template void ipmin(unsigned &, unsigned);
template void ipmax(unsigned &, unsigned);
template void ipmin(unsigned long &, unsigned long);
template void ipmax(unsigned long &, unsigned long);
template float max(float, float);
template int max(int, int);

#include "kapi.h"
#include "kapi_vector.C"
template class kapi_vector<int>;
template class kapi_vector<uint16_t>;
template class kapi_vector<uint32_t>;
template class kapi_vector<uint64_t>;
template class kapi_vector<kapi_snippet>;
template class kapi_vector<kapi_module>;
template class kapi_vector<kapi_function>;
template class kapi_vector<kapi_basic_block>;
template class kapi_vector<kapi_point>;
template class kapi_vector<kapi_disass_insn>;
template class kapi_vector<kapi_disass_chunk>;
template class kapi_vector< kapi_vector<int> >;
