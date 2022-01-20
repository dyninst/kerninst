#ifndef _RELOC_H_
#define _RELOC_H_

// picks the right class definition & include based upon defined platform

#if (defined(sparc_sun_solaris2_7) || defined(sparc_sun_solaris2_8))
class sparc_relocatableCode;
#include "sparc_relocatableCode.h"
#elif defined(i386_unknown_linux2_4)
class x86_relocatableCode;
#include "x86_relocatableCode.h"
#elif defined(rs6000_ibm_aix5_1)
class power_relocatableCode;
#include "power_relocatableCode.h"
#endif

#endif // _RELOC_H_
