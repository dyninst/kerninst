// instr.C

#ifdef sparc_sun_solaris2_7
#include "sparc_instr.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_instr.h"
#include "x86_reg.h"
#elif defined(rs6000_ibm_aix5_1)
#include "power_instr.h"
#endif

#include "insnVec.h"

// ----------------------------------------------------------------------

instr_t::registerUsage::registerUsage() {
   definitelyUsedBeforeWritten = regset_t::getRegSet(regset_t::empty);
   maybeUsedBeforeWritten = regset_t::getRegSet(regset_t::empty);
   definitelyWritten = regset_t::getRegSet(regset_t::empty);
   maybeWritten = regset_t::getRegSet(regset_t::empty);
}

instr_t::registerUsage::~registerUsage() {
   if (definitelyUsedBeforeWritten) 
      delete definitelyUsedBeforeWritten;
   if (maybeUsedBeforeWritten) 
      delete maybeUsedBeforeWritten;
   if (definitelyWritten) 
      delete definitelyWritten;
   if (maybeWritten) 
      delete maybeWritten;
}

// ----------------------------------------------------------------------

instr_t::address::address(addrtype itype, reg_t *r1, reg_t *r2,
			  int32_t ioffset, int32_t iscale) :
   thetype(itype), reg1(r1), reg2(r2), offset(ioffset), scale(iscale)
{ /* we should probably verify that itype matches rest of args */ }

instr_t::address::address(addrtype itype, reg_t *r1, int32_t ioffset) :
   thetype(itype), reg1(r1), reg2(NULL), offset(ioffset), scale(0) {
   assert(itype == singleRegPlusOffset);
}

instr_t::address::address(addrtype itype, reg_t *r1, reg_t *r2) :
   thetype(itype), reg1(r1), reg2(r2), offset(0), scale(0) {
   assert(itype == doubleReg);
}

instr_t::address::address(const address &src) :
   thetype(src.thetype), reg1(src.reg1), reg2(src.reg2),
   offset(src.offset), scale(src.scale)
{}

instr_t::address& 
instr_t::address::operator=(const address &src) {
   thetype = src.thetype; 
   reg1 = src.reg1;
   reg2 = src.reg2;
   offset = src.offset; 
   scale = src.scale;
   return *this;
}

instr_t::address::~address() {
       switch(thetype) {
       case singleReg:
       case singleRegPlusOffset:
       case scaledSingleRegPlusOffset:
          if(reg1) delete reg1;
          break;
       case doubleReg:
       case doubleRegPlusOffset:
       case scaledDoubleRegPlusOffset:
          if(reg1) delete reg1;
	  if(reg2) delete reg2;
          break;
       case absoluteAddr:
       case relativeOffset:
       case addrUndefined:
          break;
       default:
          assert(false);
       }
}

instr_t::address::equality 
instr_t::address::equality_check(const address &other) const {
  if(thetype == other.thetype) {
     switch(thetype) {
     case absoluteAddr: case relativeOffset:
        if(offset == other.offset)
	   return definitelyEqual;
	else
	   return definitelyNotEqual;
	break;
     case singleReg:
        if(*reg1 == *(other.reg1)) 
	   return definitelyEqual;
	else
	   return definitelyNotEqual;
	break;
     case singleRegPlusOffset:
        if(*reg1 == *(other.reg1)) {
           if(offset == other.offset)
	      return definitelyEqual;
	   else
	      return definitelyNotEqual;
	}
	break;
     case scaledSingleRegPlusOffset:
        if((*reg2 == *(other.reg2)) && (scale == other.scale)) {
	   if(offset == other.offset)
	      return definitelyEqual;
	   else
	      return definitelyNotEqual;
        }
	break;
     case doubleReg:
        if((*reg1 == *(other.reg1)) && (*reg2 == *(other.reg2)))
	   return definitelyEqual;
	else if((*reg1 == *(other.reg2)) && (*reg2 == *(other.reg1)))
	   return definitelyEqual;
	break;
     case doubleRegPlusOffset:
        if((*reg1 == *(other.reg1)) && (*reg2 == *(other.reg2))) {
	   if(offset == other.offset)
	      return definitelyEqual;
	   else
	      return definitelyNotEqual;
	}
	else if((*reg1 == *(other.reg2)) && (*reg2 == *(other.reg1))) { 
	   if(offset == other.offset)
	      return definitelyEqual;
	   else
	      return definitelyNotEqual;
	}
	break;
     case scaledDoubleRegPlusOffset:
        if((*reg1 == *(other.reg1)) && (*reg2 == *(other.reg2))) {
	   if((offset == other.offset) && (scale == other.scale))
	      return definitelyEqual;
	   else if((offset != other.offset) && (scale == other.scale))
	      return definitelyNotEqual;
	   else if((offset == other.offset) && (scale != other.scale))
	      return definitelyNotEqual;
	}
	break;
     case addrUndefined:
        break;
     };   
  } 
  return unsureEqual;
}

// ----------------------------------------------------------------------

instr_t::controlFlowInfo::controlFlowInfo() 
   : cctype(intcc), conditionReg(NULL), condition(0), bit_stuff(0),
     delaySlot(dsNone), destination(dstUndefined), 
     destreg1(NULL), destreg2(NULL), offset_nbytes(0) 
{}

instr_t::controlFlowInfo::~controlFlowInfo()
{
   if(conditionReg) delete conditionReg;
   if(destreg1) delete destreg1;
   if(destreg2) delete destreg2;
}

// ----------------------------------------------------------------------

bool instr_t::instructionHasValidAlignment(kptr_t addr) {
#ifdef sparc_sun_solaris2_7
   return sparc_instr::instructionHasValidAlignment(addr);
#elif defined(i386_unknown_linux2_4)
   return x86_instr::instructionHasValidAlignment(addr);
#elif defined(rs6000_ibm_aix5_1)
   return power_instr::instructionHasValidAlignment(addr);
#endif
}

bool instr_t::hasUniformInstructionLength() {
#ifdef sparc_sun_solaris2_7
   return sparc_instr::hasUniformInstructionLength();
#elif defined(i386_unknown_linux2_4)
   return x86_instr::hasUniformInstructionLength();
#elif defined(rs6000_ibm_aix5_1)
   return power_instr::hasUniformInstructionLength();
#endif
}

bool instr_t::condBranchesUseDelaySlot() {
#ifdef sparc_sun_solaris2_7
   return sparc_instr::condBranchesUseDelaySlot();
#elif defined(i386_unknown_linux2_4)
   return x86_instr::condBranchesUseDelaySlot();
#elif defined(rs6000_ibm_aix5_1)
   return power_instr::condBranchesUseDelaySlot();
#endif
}

// ----------------------------------------------------------------------

void instr_t::gen32imm(insnVec_t *piv, uint32_t val, const reg_t &rd) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::gen32imm(piv, val, (const sparc_reg &)rd);
#elif defined(i386_unknown_linux2_4)
   x86_instr::gen32imm(piv, val, (const x86_reg &)rd);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::gen32imm(piv, val, (const power_reg &)rd);
#endif
}

void instr_t::genImmAddr(insnVec_t *piv, uint32_t addr, const reg_t &rd) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::genImmAddr(piv, addr, (const sparc_reg &)rd);
#elif defined(i386_unknown_linux2_4)
   x86_instr::genImmAddr(piv, addr, (const x86_reg &)rd);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::genImmAddr(piv, addr, (const power_reg &)rd);
#endif
}

void instr_t::genImmAddr(insnVec_t *piv, uint64_t addr, const reg_t &rd) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::genImmAddr(piv, addr, (const sparc_reg &)rd);
#elif defined(i386_unknown_linux2_4)
   x86_instr::genImmAddr(piv, addr, (const x86_reg &)rd);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::genImmAddr(piv, addr, (const power_reg &)rd);
#endif
}

void instr_t::gen64imm(insnVec_t *piv, uint64_t val, 
                       const reg_t &rd, const reg_t &rtemp) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::gen64imm(piv, val, (const sparc_reg&)rd, (const sparc_reg&)rtemp);
#elif defined(i386_unknown_linux2_4)
   x86_instr::gen64imm(piv, val, (const x86_reg&)rd, (const x86_reg&)rtemp);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::gen64imm(piv, val, (const power_reg&)rd,(const power_reg&)rtemp);
#endif
}

void instr_t::genImmAddrGeneric(insnVec_t *piv, uint32_t addr, 
                                const reg_t &rd, bool setLow) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::genImmAddrGeneric(piv, addr, (const sparc_reg &)rd, setLow);
#elif defined(i386_unknown_linux2_4)
   x86_instr::genImmAddrGeneric(piv, addr, (const x86_reg &)rd, setLow);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::genImmAddrGeneric(piv, addr, (const power_reg &)rd, setLow);
#endif 
}

void instr_t::genImmAddrGeneric(insnVec_t *piv, uint64_t addr, 
                                const reg_t &rd, bool setLow) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::genImmAddrGeneric(piv, addr, (const sparc_reg &)rd, setLow);
#elif defined(i386_unknown_linux2_4)
   x86_instr::genImmAddrGeneric(piv, addr, (const x86_reg &)rd, setLow);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::genImmAddrGeneric(piv, addr, (const power_reg &)rd, setLow);
#endif 
}

void instr_t::genCallAndLink(insnVec_t *piv, kptr_t from, kptr_t to) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::genCallAndLink(piv, from, to);
#elif defined(i386_unknown_linux2_4)
   x86_instr::genCallAndLink(piv, from, to);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::genCallAndLink(piv, from, to);
#endif
}

void instr_t::genCallAndLinkGeneric_unknownFromAndToAddr(insnVec_t *piv) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::genCallAndLinkGeneric_unknownFromAndToAddr(piv);
#elif defined(i386_unknown_linux2_4)
   x86_instr::genCallAndLinkGeneric_unknownFromAndToAddr(piv);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::genCallAndLinkGeneric_unknownFromAndToAddr(piv);
#endif
}

void instr_t::genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, kptr_t to) {
#ifdef sparc_sun_solaris2_7
   sparc_instr::genCallAndLinkGeneric_unknownFromAddr(piv, to);
#elif defined(i386_unknown_linux2_4)
   x86_instr::genCallAndLinkGeneric_unknownFromAddr(piv, to);
#elif defined(rs6000_ibm_aix5_1)
   power_instr::genCallAndLinkGeneric_unknownFromAddr(piv, to);
#endif
}
// ----------------------------------------------------------------------

const instr_t& instr_t::getInstr(uint32_t raw) {
   //WARNING: anyone who calls this must worry about destructing the
   //         returned instr_t or it will be leaked
#ifdef sparc_sun_solaris2_7
   return *(const instr_t*)(new sparc_instr(raw));
#elif defined(i386_unknown_linux2_4)
   assert(!"instr_t::getInstr(uint32_t raw) - x86 should never use this method of construction");
   return *(const instr_t*)(new x86_instr((const char*)NULL));;
#elif defined(rs6000_ibm_aix5_1)
   return *(const instr_t*)(new power_instr(raw));
#endif
}

instr_t* instr_t::getInstr(const instr_t &i) {
#ifdef sparc_sun_solaris2_7
   return new sparc_instr((const sparc_instr &)i);
#elif defined(i386_unknown_linux2_4)
   return new x86_instr((const x86_instr &)i);
#elif defined(rs6000_ibm_aix5_1)
   return new power_instr((const power_instr &)i);
#endif
}

void instr_t::putInstr(const instr_t *i, void *where) {
#ifdef sparc_sun_solaris2_7
   (void)new(where) sparc_instr(*((const sparc_instr *)i));
#elif defined(i386_unknown_linux2_4)
   (void)new(where) x86_instr(*((const x86_instr *)i));
#elif defined(rs6000_ibm_aix5_1)
   (void)new(where) power_instr(*((const power_instr *)i));
#endif
}

void instr_t::putInstr(XDR *xdr, void *where) {
#ifdef sparc_sun_solaris2_7
   (void)new(where) sparc_instr(xdr);
#elif defined(i386_unknown_linux2_4)
   (void)new(where) x86_instr(xdr);
#elif defined(rs6000_ibm_aix5_1)
   (void)new(where) power_instr(xdr);
#endif
}

bool instr_t::inRangeOf(kptr_t fromaddr, kptr_t toaddr,
                        unsigned numBitsOfSignedDisplacement) {
   // a static method

   kptr_t min_reachable, max_reachable;
   getRangeOf(fromaddr, numBitsOfSignedDisplacement, min_reachable, max_reachable);
   
   return (toaddr >= min_reachable && toaddr <= max_reachable);
}

void instr_t::getRangeOf(kptr_t fromaddr,
                         unsigned numBitsOfSignedDisplacement,
                         kptr_t &min_reachable_result,
                         kptr_t &max_reachable_result) {
   // a static method

   kptr_t negative_displacement = (kptr_t)1U << (numBitsOfSignedDisplacement-1);
   kptr_t positive_displacement = negative_displacement - 1;

   if (negative_displacement > fromaddr)
      min_reachable_result = 0;
   else
      min_reachable_result = fromaddr - negative_displacement;

   if (positive_displacement > (kptr_t)-1U - fromaddr)
      max_reachable_result = (kptr_t)-1U;
   else
      max_reachable_result = fromaddr + positive_displacement;
   
   assert(min_reachable_result <= fromaddr);
   assert(fromaddr <= max_reachable_result);
}
