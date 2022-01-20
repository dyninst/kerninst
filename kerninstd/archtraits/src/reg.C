// reg.C - static members of reg_t class

#include "reg.h"

reg_t::reg_t(const reg_t &src) : regnum(src.regnum) {
   assert(regnum <= getMaxRegNum());
}

reg_t::reg_t(unsigned inum) : regnum(inum) {
   assert(regnum <= getMaxRegNum());
}

reg_t::~reg_t() {} // according to gcc, must be defined even if pure-virtual

bool reg_t::operator<(const reg_t &src) const {
   return regnum < src.regnum;
}

bool reg_t::operator<=(const reg_t &src) const {
   return regnum <= src.regnum;
}

bool reg_t::operator>=(const reg_t &src) const {
   return regnum >= src.regnum;
}

bool reg_t::operator>(const reg_t &src) const {
   return regnum > src.regnum;
}

bool reg_t::operator==(const reg_t &src) const { 
   return (regnum == src.regnum); 
}

bool reg_t::operator!=(const reg_t &src) const { 
   return (regnum != src.regnum); 
}

#ifdef sparc_sun_solaris2_7
#include "sparc_reg.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_reg.h"
#elif defined(rs6000_ibm_aix5_1)
#include "power_reg.h"
#endif

reg_t& reg_t::operator=(const reg_t &src) {
#ifdef sparc_sun_solaris2_7
   (sparc_reg&)*this = (const sparc_reg &)src;
#elif defined(i386_unknown_linux2_4)
   (x86_reg&)*this = (const x86_reg &)src;
#elif defined(rs6000_ibm_aix5_1)
   (power_reg&)*this = (const power_reg &)src;
#endif
   return *this;
}


const unsigned reg_t::getMaxRegNum() {
#ifdef sparc_sun_solaris2_7
   return sparc_reg::max_reg_num;
#elif defined(i386_unknown_linux2_4)
   return x86_reg::max_reg_num;
#elif defined(rs6000_ibm_aix5_1)
   return power_reg::max_reg_num;
#endif
}

reg_t& reg_t::getRegByBit(unsigned bit) {
#ifdef sparc_sun_solaris2_7
   return sparc_reg::getRegByBit(bit);
#elif defined(i386_unknown_linux2_4)
   return x86_reg::getRegByBit(bit);
#elif defined(rs6000_ibm_aix5_1)
   return (reg_t &) power_reg::getRegByBit(bit);
#endif
}
