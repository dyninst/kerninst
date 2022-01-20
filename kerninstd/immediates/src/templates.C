// templates.C

#include "uimm.C"
#include "simm.C"

template class simmediate<13>;
template class uimmediate<13>;
template void operator|=(unsigned&, simmediate<13>);
template void operator|=(unsigned&, uimmediate<13>);
