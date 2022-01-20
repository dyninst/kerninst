// x86_reg.C

#include "x86_reg.h"

#include <assert.h>
#include <stdio.h>

// Constructor selectors
x86_reg::NullReg          x86_reg::nullReg;
x86_reg::RawIntReg        x86_reg::rawIntReg;
x86_reg::GeneralModRMReg  x86_reg::generalModRMReg;
x86_reg::SegmentReg       x86_reg::segmentReg;
x86_reg::ControlReg       x86_reg::controlReg;
x86_reg::DebugReg         x86_reg::debugReg;

// General integer registers
// If you change the ordering here, you must fix x86_reg_set
const unsigned x86_reg::min_int_reg = 0;
x86_reg x86_reg::AL(0, "al");  x86_reg x86_reg::AX(8,  "ax");  x86_reg x86_reg::eAX(16, "eax");
x86_reg x86_reg::CL(1, "cl");  x86_reg x86_reg::CX(9,  "cx");  x86_reg x86_reg::eCX(17, "ecx");
x86_reg x86_reg::DL(2, "dl");  x86_reg x86_reg::DX(10, "dx");  x86_reg x86_reg::eDX(18, "edx");
x86_reg x86_reg::BL(3, "bl");  x86_reg x86_reg::BX(11, "bx");  x86_reg x86_reg::eBX(19, "ebx");
x86_reg x86_reg::AH(4, "ah");  x86_reg x86_reg::SP(12, "sp");  x86_reg x86_reg::eSP(20, "esp");
x86_reg x86_reg::CH(5, "ch");  x86_reg x86_reg::BP(13, "bp");  x86_reg x86_reg::eBP(21, "ebp");
x86_reg x86_reg::DH(6, "dh");  x86_reg x86_reg::SI(14, "si");  x86_reg x86_reg::eSI(22, "esi");
x86_reg x86_reg::BH(7, "bh");  x86_reg x86_reg::DI(15, "di");  x86_reg x86_reg::eDI(23, "edi");
const unsigned x86_reg::max_int_reg = 23;

// Segment registers in same order as the bit assignments (V2,p3-4).
const unsigned x86_reg::min_seg_reg = 24;
x86_reg x86_reg::ES(24, "es");
x86_reg x86_reg::CS(25, "cs");
x86_reg x86_reg::SS(26, "ss");
x86_reg x86_reg::DS(27, "ds");
x86_reg x86_reg::FS(28, "fs");
x86_reg x86_reg::GS(29, "gs");
const unsigned x86_reg::max_seg_reg = 29;

// Misc registers
const unsigned x86_reg::min_misc_reg = 30;
x86_reg x86_reg::EFLAGS(30, "eflags");
x86_reg x86_reg::PC(31, "eip");
x86_reg x86_reg::FP(32, "fp?");  /* Catch all for fp regs. */
const unsigned x86_reg::max_misc_reg = 32;

// 32-bit control registers
const unsigned x86_reg::min_ctl_reg = 33;
x86_reg x86_reg::CR0(33, "cr0");
x86_reg x86_reg::CR1(34, "cr1");
x86_reg x86_reg::CR2(35, "cr2");
x86_reg x86_reg::CR3(36, "cr3");
x86_reg x86_reg::CR4(37, "cr4");
const unsigned x86_reg::max_ctl_reg = 37;

// 32-bit debug registers
const unsigned x86_reg::min_dbg_reg = 38;
x86_reg x86_reg::DR0(38, "dr0");
x86_reg x86_reg::DR1(39, "dr1");
x86_reg x86_reg::DR2(40, "dr2");
x86_reg x86_reg::DR3(41, "dr3");
x86_reg x86_reg::DR4(42, "dr4");
x86_reg x86_reg::DR5(43, "dr5");
x86_reg x86_reg::DR6(44, "dr6");
x86_reg x86_reg::DR7(45, "dr7");
const unsigned x86_reg::max_dbg_reg = 45;

// Occasionally we need a null register
x86_reg x86_reg::nullreg(46, "null");

/* If you change max_reg_num, also change highest_reg_num in x86_reg_set.h */
const unsigned x86_reg::max_reg_num = 46;

/* BE SURE TO ADJUST max_reg_num above if you add a register */

static const pdstring preDisassembledRegs[] = {
   "%al",  "%cl", "%dl", "%bl", "%ah", "%ch", "%dh", "%bh",
   "%ax",  "%cx", "%dx", "%bx", "%sp", "%bp", "%si", "%di",
   "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi",
   "%es",  "%cs", "%ss", "%ds", "%fs", "%gs",
   "%EFLAGS", "%eip", "%fp?", "%cr0", "%cr1", "%cr2", "%cr3", "%cr4",
   "%dr0", "%dr1", "%dr2", "%dr3", "%dr4", "%dr5", "%dr6", "%dr7",
   "NULLreg"
};

static x86_reg regArray[] = {
   x86_reg::AL, x86_reg::CL, x86_reg::DL, x86_reg::BL, 
   x86_reg::AH, x86_reg::CH, x86_reg::DH, x86_reg::BH,
   x86_reg::AX, x86_reg::CX, x86_reg::DX, x86_reg::BX, 
   x86_reg::SP, x86_reg::BP, x86_reg::SI, x86_reg::DI,
   x86_reg::eAX, x86_reg::eCX, x86_reg::eDX, x86_reg::eBX, 
   x86_reg::eSP, x86_reg::eBP, x86_reg::eSI, x86_reg::eDI,
   x86_reg::ES, x86_reg::CS, x86_reg::SS, 
   x86_reg::DS, x86_reg::FS, x86_reg::GS,
   x86_reg::EFLAGS, x86_reg::PC, x86_reg::FP, 
   x86_reg::CR0, x86_reg::CR1, x86_reg::CR2, x86_reg::CR4,
   x86_reg::DR0, x86_reg::DR1, x86_reg::DR2, x86_reg::DR3, 
   x86_reg::DR4, x86_reg::DR5, x86_reg::DR6, x86_reg::DR7,
   x86_reg::nullreg
};

reg_t& x86_reg::getRegByBit(unsigned bit) {
   assert(bit <= max_reg_num);
   return regArray[bit];
}

unsigned char x86_reg::getRegRMVal() const {
   // returns appropriate ModRM 'reg' or 'rm' field value
   if(regnum <= max_int_reg) {
      unsigned char result = regnum % 8;
      return result;
   }
   else {
      assert(!"x86_reg::getRegRMVal() only supported for int registers\n");
      return 0;
   }
}

x86_reg::x86_reg(unsigned i, const char * /*IGNORE*/) : reg_t(i) {
   assert(regnum <= max_reg_num);
}

x86_reg::x86_reg(NullReg) : reg_t(0) {}

x86_reg::x86_reg(RawIntReg, unsigned i) : reg_t(i) {}

x86_reg::x86_reg(GeneralModRMReg, unsigned u, unsigned width) : reg_t(0) {
   assert(width == 1 || width == 2 || width == 4);
   assert(u < 8);
   regnum = u;
   if(width > 1)
      regnum += (width/2)*8;
}

x86_reg::x86_reg(SegmentReg, unsigned u) : reg_t(0) {
   if (u >= 6)
      throw x86_reg::malformedreg();
   regnum = u + min_seg_reg;
}

x86_reg::x86_reg(ControlReg, unsigned u) : reg_t(0) {
   // We really ought to throw something about malformed registers,
   // and have the instruction parser rethrow the unknowninsn.
   if (u >= 5)
      throw x86_reg::malformedreg();
   regnum = u + min_ctl_reg;
}

x86_reg::x86_reg(DebugReg, unsigned u) : reg_t(0) {
   // We really ought to throw something about malformed registers,
   // and have the instruction parser rethrow the unknowninsn.
   if (u > 7)
      throw x86_reg::malformedreg();
   regnum = u + min_dbg_reg;
}

x86_reg::~x86_reg() {}

bool x86_reg::operator==(const x86_reg &src) const { 
   return (regnum == src.regnum); 
}

bool x86_reg::operator!=(const x86_reg &src) const { 
   return (regnum != src.regnum); 
}

bool x86_reg::isIntReg() const { 
   return (regnum <= max_int_reg); 
}

bool x86_reg::is32BitIntReg() const { 
   return ((regnum >= 16) && (regnum <= 23)); 
}

bool x86_reg::isSegReg() const { 
   return (regnum >= min_seg_reg && regnum <= max_seg_reg); 
}
   
bool x86_reg::isEFLAGS() const { 
   return (regnum == EFLAGS.regnum); 
}

bool x86_reg::isPC() const { 
   return (regnum == PC.regnum); 
}

bool x86_reg::isFP() const { 
   return (regnum == FP.regnum); 
}
   
bool x86_reg::isCtlReg() const { 
   return (regnum >= min_ctl_reg && regnum <= max_ctl_reg); 
}
 
bool x86_reg::isNullReg() const { 
   return (regnum == nullreg.regnum); 
}

bool x86_reg::isFloatReg() const {  
   return (regnum == FP.regnum); 
}

bool x86_reg::isIntCondCode() const { 
   assert(0); 
   return true; 
}

bool x86_reg::isFloatCondCode() const { 
   assert(0); 
   return true; 
}

unsigned x86_reg::getIntNum() const {
   // asserts this is an int reg, then returns regnum as appropriate
   assert(isIntReg());
   return regnum;
}

unsigned x86_reg::getFloatNum() const {
   // asserts this is a float reg, then returns regnum as appropriate
   assert(0 && "x86_reg::getFloatNum nyi");
   assert(isFloatReg());
   return 0; // FIXME: when float registers get defined
}

pdstring x86_reg::disass() const {
   assert(regnum <= max_reg_num);
   return preDisassembledRegs[regnum];
}

pdstring x86_reg::disassx(unsigned reg) {
   assert(reg <= max_reg_num);
   return preDisassembledRegs[reg];
}

void rollinFpReg(unsigned &result, x86_reg r, unsigned precision) {

   assert(0);      // FP undefined for now on x86.

   unsigned bits = 0;
   const unsigned floatNum = r.getFloatNum();
   assert(floatNum % precision == 0);
   
   switch (precision) {
      case 1:
         bits = floatNum;
         break;
      case 2:
         bits = (floatNum & 0x1f) | (floatNum >> 5);
         break;
      case 4:
         bits = (floatNum & 0x1f) | (floatNum >> 5);
         // coincidentally (or perhaps not) the same algorithm as for doubles
         break;
      default:
         assert(false);
   }

   rollin(result, uimmediate<5>(bits));
}
