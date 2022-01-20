// templates.C

#include "uimm.C"
#include "simm.C"

template class uimmediate<1>;
template class uimmediate<2>;
template class uimmediate<3>;
template class uimmediate<4>;
template class uimmediate<5>;
template class uimmediate<6>;
template class uimmediate<8>;
template class uimmediate<14>;
template class uimmediate<13>;
template class uimmediate<22>;
template class uimmediate<30>;
template void operator|=(unsigned &, uimmediate<1>);
template void operator|=(unsigned &, uimmediate<2>);
template void operator|=(unsigned &, uimmediate<3>);
template void operator|=(unsigned &, uimmediate<4>);
template void operator|=(unsigned &, uimmediate<5>);
template void operator|=(unsigned &, uimmediate<6>);
template void operator|=(unsigned &, uimmediate<8>);
template void operator|=(unsigned &, uimmediate<14>);
template void operator|=(unsigned &, uimmediate<13>);
template void operator|=(unsigned &, uimmediate<22>);

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
