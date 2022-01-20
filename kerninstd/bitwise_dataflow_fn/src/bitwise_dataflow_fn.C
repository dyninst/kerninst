// bitwise_dataflow_fn.C

#include "bitwise_dataflow_fn.h"

monotone_bitwise_dataflow_fn_ll::monotone_bitwise_dataflow_fn_ll() {}
monotone_bitwise_dataflow_fn_ll::~monotone_bitwise_dataflow_fn_ll() {}

monotone_bitwise_dataflow_fn::StartAllRegs monotone_bitwise_dataflow_fn::startall;
monotone_bitwise_dataflow_fn::StopAllRegs monotone_bitwise_dataflow_fn::stopall;
monotone_bitwise_dataflow_fn::PassAllRegs monotone_bitwise_dataflow_fn::passall;

#ifdef sparc_sun_solaris2_7
#include "sparc_bitwise_dataflow_fn.h"
#elif defined(i386_unknown_linux2_4) 
#include "x86_bitwise_dataflow_fn.h"
#elif defined(ppc64_unknown_linux2_4) 
#include "power_bitwise_dataflow_fn.h"
#endif

monotone_bitwise_dataflow_fn* 
monotone_bitwise_dataflow_fn::getDataflowFn(StartAllRegs) {
#ifdef sparc_sun_solaris2_7
   return (monotone_bitwise_dataflow_fn *) new 
      sparc_bitwise_dataflow_fn(sparc_bitwise_dataflow_fn::startAllRegs);
#elif defined(i386_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) new 
      x86_bitwise_dataflow_fn(x86_bitwise_dataflow_fn::startAllRegs);
#elif defined(ppc64_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) new 
      power_bitwise_dataflow_fn(power_bitwise_dataflow_fn::startAllRegs);
#endif
}

monotone_bitwise_dataflow_fn* 
monotone_bitwise_dataflow_fn::getDataflowFn(StopAllRegs) {
#ifdef sparc_sun_solaris2_7
   return (monotone_bitwise_dataflow_fn *) new 
      sparc_bitwise_dataflow_fn(sparc_bitwise_dataflow_fn::stopAllRegs);
#elif defined(i386_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) new 
      x86_bitwise_dataflow_fn(x86_bitwise_dataflow_fn::stopAllRegs);
#elif defined(ppc64_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) new 
      power_bitwise_dataflow_fn(power_bitwise_dataflow_fn::stopAllRegs);
#endif
}

monotone_bitwise_dataflow_fn* 
monotone_bitwise_dataflow_fn::getDataflowFn(PassAllRegs) {
#ifdef sparc_sun_solaris2_7
   return (monotone_bitwise_dataflow_fn *) new 
      sparc_bitwise_dataflow_fn(sparc_bitwise_dataflow_fn::passAllRegs);
#elif defined(i386_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) new 
      x86_bitwise_dataflow_fn(x86_bitwise_dataflow_fn::passAllRegs);
#elif defined(ppc64_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) new 
      power_bitwise_dataflow_fn(power_bitwise_dataflow_fn::passAllRegs);
#endif
}

monotone_bitwise_dataflow_fn* 
monotone_bitwise_dataflow_fn::getDataflowFn(XDR *xdr) {
#ifdef sparc_sun_solaris2_7
   return (monotone_bitwise_dataflow_fn *) new sparc_bitwise_dataflow_fn(xdr);
#elif defined(i386_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) new x86_bitwise_dataflow_fn(xdr);
#elif defined(ppc64_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) new power_bitwise_dataflow_fn(xdr);
#endif
}

monotone_bitwise_dataflow_fn* 
monotone_bitwise_dataflow_fn::getDataflowFn(const monotone_bitwise_dataflow_fn &src) {
#ifdef sparc_sun_solaris2_7
   return (monotone_bitwise_dataflow_fn *) 
      new sparc_bitwise_dataflow_fn((sparc_bitwise_dataflow_fn&)src);
#elif defined(i386_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) 
      new x86_bitwise_dataflow_fn((x86_bitwise_dataflow_fn&)src);
#elif defined(ppc64_unknown_linux2_4) 
   return (monotone_bitwise_dataflow_fn *) 
      new power_bitwise_dataflow_fn((power_bitwise_dataflow_fn&)src);
#endif
}

monotone_bitwise_dataflow_fn& monotone_bitwise_dataflow_fn::operator=(const monotone_bitwise_dataflow_fn &src) {
#ifdef sparc_sun_solaris2_7
   (sparc_bitwise_dataflow_fn &)*this = (const sparc_bitwise_dataflow_fn&)src;
#elif defined(i386_unknown_linux2_4)
   (x86_bitwise_dataflow_fn &)*this = (const x86_bitwise_dataflow_fn&)src;
#elif defined(ppc64_unknown_linux2_4)
   (power_bitwise_dataflow_fn &)*this = (const power_bitwise_dataflow_fn&)src;
#endif
   return *this; 
}
