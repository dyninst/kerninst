// templates.C

#include "util/src/Dictionary.C"
#include "util/h/refCounterK.h"
#include "util/h/StaticString.h"
#include "util/h/StaticVector.h"

template class static_vector<char>;

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
template void ipmin(long long &, long long);
template void ipmin(long &, long);
template void ipmin(int &, int);
template void ipmin(unsigned long &, unsigned long);
template void ipmax(unsigned &, unsigned);
template void ipmax(unsigned long &, unsigned long);
template void ipmin(unsigned long long&, unsigned long long);
template float max(float, float);
template int max(int, int);

//#include <iomanip.h>
//template ostream& operator<<(ostream&, const smanip<int>&);

#include "util/h/mrtime.h"
#include "pdutil/src/PriorityQueue.C"
#include "internalEvent.h"
template class PriorityQueue<mrtime_t, const internalEvent*>;

