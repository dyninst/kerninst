// include the correct .C based upon defined platform

#if (defined(sparc_sun_solaris2_7) || defined(sparc_sun_solaris2_8))
#include "sparc_controlFlow.C"
#elif defined(x86_unknown_linux2_4)
#include "x86_controlFlow.C"
#elif defined(rs6000_ibm_aix5_1)
#include "power_controlFlow.C"
#endif
