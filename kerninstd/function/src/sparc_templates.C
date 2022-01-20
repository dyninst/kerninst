#include "util/src/minmax.C"

template void ipmax(uint32_t &, uint32_t);

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
