// picks the right include file based upon defined platform

#if (defined(sparc_sun_solaris2_7) || defined(sparc_sun_solaris2_8))
#include "sparc_controlFlow.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_controlFlow.h"
#elif defined(rs6000_ibm_aix5_1)
#include "power_controlFlow.h"
#endif
