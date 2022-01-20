// reg_set.C

#include "reg_set.h"

#ifdef sparc_sun_solaris2_7
#include "sparc_reg_set.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_reg_set.h"
#elif defined(rs6000_ibm_aix5_1)
#include "power_reg_set.h"
#endif

regset_t* regset_t::getRegSet(Empty e) {
   regset_t *result;
#ifdef sparc_sun_solaris2_7
   result = new sparc_reg_set(e);
#elif defined(i386_unknown_linux2_4)
   result = new x86_reg_set(e);
#elif defined(rs6000_ibm_aix5_1)
   result = new power_reg_set(e);
#endif
   return result;
}

regset_t* regset_t::getRegSet(Full f) {
   regset_t *result;
#ifdef sparc_sun_solaris2_7
   result = new sparc_reg_set(f);
#elif defined(i386_unknown_linux2_4)
   result = new x86_reg_set(f);
#elif defined(rs6000_ibm_aix5_1)
   result = new power_reg_set(f);
#endif
   return result;
}

regset_t* regset_t::getRegSet(XDR *xdr) {
   regset_t *result;
#ifdef sparc_sun_solaris2_7
   result = new sparc_reg_set(xdr);
#elif defined(i386_unknown_linux2_4)
   result = new x86_reg_set(xdr);
#elif defined(rs6000_ibm_aix5_1)
   result = new power_reg_set(xdr);
#endif
   return result;
}

regset_t* regset_t::getRegSet(const regset_t &src) {
   regset_t *result;
#ifdef sparc_sun_solaris2_7
   result = new sparc_reg_set((sparc_reg_set&)src);
#elif defined(i386_unknown_linux2_4)
   result = new x86_reg_set((x86_reg_set&)src);
#elif defined(rs6000_ibm_aix5_1)
   result = new power_reg_set((power_reg_set&)src);
#endif
   return result;
}

const regset_t& regset_t::getEmptySet() {
#ifdef sparc_sun_solaris2_7
   return sparc_reg_set::emptyset;
#elif defined(i386_unknown_linux2_4)
   return x86_reg_set::emptyset;
#elif defined(rs6000_ibm_aix5_1)
   return power_reg_set::emptyset;
#endif
}

const regset_t& regset_t::getFullSet() {
#ifdef sparc_sun_solaris2_7
   return sparc_reg_set::fullset;
#elif defined(i386_unknown_linux2_4)
   return x86_reg_set::fullset;
#elif defined(rs6000_ibm_aix5_1)
   return power_reg_set::fullset;
#endif
}

const regset_t& regset_t::getAlwaysLiveSet() {
#ifdef sparc_sun_solaris2_7
   return sparc_reg_set::always_live_set;
#elif defined(i386_unknown_linux2_4)
   return x86_reg_set::always_live_set;
#elif defined(rs6000_ibm_aix5_1)
   return power_reg_set::always_live_set;
#endif
}

const regset_t& regset_t::getRegsThatIgnoreWrites() {
#ifdef sparc_sun_solaris2_7
   return sparc_reg_set::regsThatIgnoreWrites;
#elif defined(i386_unknown_linux2_4)
   return x86_reg_set::regsThatIgnoreWrites;
#elif defined(rs6000_ibm_aix5_1)
   return power_reg_set::regsThatIgnoreWrites;
#endif
}

const regset_t& regset_t::getAllIntRegs() {
#ifdef sparc_sun_solaris2_7
   return sparc_reg_set::allIntRegs;
#elif defined(i386_unknown_linux2_4)
   return x86_reg_set::allIntRegs;
#elif defined(rs6000_ibm_aix5_1)
   return power_reg_set::allIntRegs;
#endif
}

bool regset_t::inSet(const regset_t &theset, unsigned bit) {
#ifdef sparc_sun_solaris2_7
   return sparc_reg_set::inSet(theset, bit);
#elif defined(i386_unknown_linux2_4)
   return x86_reg_set::inSet(theset, bit);
#elif defined(rs6000_ibm_aix5_1)
   return power_reg_set::inSet(theset, bit);
#endif
}

void regset_t::operator=(const regset_t &src) {
#ifdef sparc_sun_solaris2_7
   (sparc_reg_set&)*this = (const sparc_reg_set&)src;
#elif defined(i386_unknown_linux2_4)
   (x86_reg_set&)*this = (const x86_reg_set&)src;
#elif defined(rs6000_ibm_aix5_1)
   (power_reg_set&)*this = (const power_reg_set&)src;
#endif
}

// declare some static members.  Note: we mustn't use any static members
// when defining static members, since the order of construction is undefined!
// (Thus, perpetually-undefined static members, like raw, random, empty, etc. may be
// used at any time)
regset_t::Raw regset_t::raw;
regset_t::Random regset_t::random; // only for regression testing, of course
regset_t::Empty regset_t::empty;
regset_t::Full regset_t::full;
regset_t::SingleIntReg regset_t::singleIntReg;
regset_t::SingleFloatReg regset_t::singleFloatReg;
