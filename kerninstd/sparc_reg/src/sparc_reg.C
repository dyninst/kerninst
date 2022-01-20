// sparc_reg.C
// Ariel Tamches

#include <assert.h>
#include "sparc_reg.h"
#include "common/h/String.h"

// Define static members:

const unsigned sparc_reg::max_reg_num = 105;

sparc_reg::RAWIntReg sparc_reg::rawIntReg;
sparc_reg::GLOBAL sparc_reg::global;
sparc_reg::IN     sparc_reg::in;
sparc_reg::LOCAL  sparc_reg::local;
sparc_reg::OUT    sparc_reg::out;

sparc_reg::FP     sparc_reg::f;

sparc_reg::Icc_type    sparc_reg::icc_type;
sparc_reg::Xcc_type    sparc_reg::xcc_type;
sparc_reg::Fcc_type    sparc_reg::fcc_type;
sparc_reg::PC_type     sparc_reg::pc_type;
sparc_reg::GSR_type    sparc_reg::gsr_type;
sparc_reg::ASI_type    sparc_reg::asi_type;
sparc_reg::PIL_type    sparc_reg::pil_type;

sparc_reg sparc_reg::g0(global, 0);   sparc_reg sparc_reg::g1(global, 1);
sparc_reg sparc_reg::g2(global, 2);   sparc_reg sparc_reg::g3(global, 3);
sparc_reg sparc_reg::g4(global, 4);   sparc_reg sparc_reg::g5(global, 5);
sparc_reg sparc_reg::g6(global, 6);   sparc_reg sparc_reg::g7(global, 7);
  
sparc_reg sparc_reg::o0(out, 0);   sparc_reg sparc_reg::o1(out, 1);
sparc_reg sparc_reg::o2(out, 2);   sparc_reg sparc_reg::o3(out, 3);
sparc_reg sparc_reg::o4(out, 4);   sparc_reg sparc_reg::o5(out, 5);
sparc_reg sparc_reg::o6(out, 6);   sparc_reg sparc_reg::sp(out, 6);
sparc_reg sparc_reg::o7(out, 7);

sparc_reg sparc_reg::l0(local, 0);   sparc_reg sparc_reg::l1(local, 1);
sparc_reg sparc_reg::l2(local, 2);   sparc_reg sparc_reg::l3(local, 3);
sparc_reg sparc_reg::l4(local, 4);   sparc_reg sparc_reg::l5(local, 5);
sparc_reg sparc_reg::l6(local, 6);   sparc_reg sparc_reg::l7(local, 7);

sparc_reg sparc_reg::i0(in, 0);   sparc_reg sparc_reg::i1(in, 1);
sparc_reg sparc_reg::i2(in, 2);   sparc_reg sparc_reg::i3(in, 3);
sparc_reg sparc_reg::i4(in, 4);   sparc_reg sparc_reg::i5(in, 5);
sparc_reg sparc_reg::i6(in, 6);   sparc_reg sparc_reg::fp(in, 6);
sparc_reg sparc_reg::i7(in, 7);

sparc_reg sparc_reg::f0(f, 0);   sparc_reg sparc_reg::f1(f, 1);
sparc_reg sparc_reg::f2(f, 2);   sparc_reg sparc_reg::f3(f, 3);
sparc_reg sparc_reg::f4(f, 4);   sparc_reg sparc_reg::f5(f, 5);
sparc_reg sparc_reg::f6(f, 6);   sparc_reg sparc_reg::f7(f, 7);
sparc_reg sparc_reg::f8(f, 8);   sparc_reg sparc_reg::f9(f, 9);
sparc_reg sparc_reg::f10(f, 10);   sparc_reg sparc_reg::f11(f, 11);
sparc_reg sparc_reg::f12(f, 12);   sparc_reg sparc_reg::f13(f, 13);
sparc_reg sparc_reg::f14(f, 14);   sparc_reg sparc_reg::f15(f, 15);
sparc_reg sparc_reg::f16(f, 16);   sparc_reg sparc_reg::f17(f, 17);
sparc_reg sparc_reg::f18(f, 18);   sparc_reg sparc_reg::f19(f, 19);
sparc_reg sparc_reg::f20(f, 20);   sparc_reg sparc_reg::f21(f, 21);
sparc_reg sparc_reg::f22(f, 22);   sparc_reg sparc_reg::f23(f, 23);
sparc_reg sparc_reg::f24(f, 24);   sparc_reg sparc_reg::f25(f, 25);
sparc_reg sparc_reg::f26(f, 26);   sparc_reg sparc_reg::f27(f, 27);
sparc_reg sparc_reg::f28(f, 28);   sparc_reg sparc_reg::f29(f, 29);
sparc_reg sparc_reg::f30(f, 30);   sparc_reg sparc_reg::f31(f, 31);
sparc_reg sparc_reg::f32(f, 32);   sparc_reg sparc_reg::f33(f, 33);
sparc_reg sparc_reg::f34(f, 34);   sparc_reg sparc_reg::f35(f, 35);
sparc_reg sparc_reg::f36(f, 36);   sparc_reg sparc_reg::f37(f, 37);
sparc_reg sparc_reg::f38(f, 38);   sparc_reg sparc_reg::f39(f, 39);
sparc_reg sparc_reg::f40(f, 40);   sparc_reg sparc_reg::f41(f, 41);
sparc_reg sparc_reg::f42(f, 42);   sparc_reg sparc_reg::f43(f, 43);
sparc_reg sparc_reg::f44(f, 44);   sparc_reg sparc_reg::f45(f, 45);
sparc_reg sparc_reg::f46(f, 46);   sparc_reg sparc_reg::f47(f, 47);
sparc_reg sparc_reg::f48(f, 48);   sparc_reg sparc_reg::f49(f, 49);
sparc_reg sparc_reg::f50(f, 50);   sparc_reg sparc_reg::f51(f, 51);
sparc_reg sparc_reg::f52(f, 52);   sparc_reg sparc_reg::f53(f, 53);
sparc_reg sparc_reg::f54(f, 54);   sparc_reg sparc_reg::f55(f, 55);
sparc_reg sparc_reg::f56(f, 56);   sparc_reg sparc_reg::f57(f, 57);
sparc_reg sparc_reg::f58(f, 58);   sparc_reg sparc_reg::f59(f, 59);
sparc_reg sparc_reg::f60(f, 60);   sparc_reg sparc_reg::f61(f, 61);
sparc_reg sparc_reg::f62(f, 62);   sparc_reg sparc_reg::f63(f, 63);


sparc_reg sparc_reg::reg_icc(icc_type);
sparc_reg sparc_reg::reg_xcc(xcc_type);
sparc_reg sparc_reg::reg_fcc0(fcc_type, 0);
sparc_reg sparc_reg::reg_fcc1(fcc_type, 1);
sparc_reg sparc_reg::reg_fcc2(fcc_type, 2);
sparc_reg sparc_reg::reg_fcc3(fcc_type, 3);
sparc_reg sparc_reg::reg_pc(pc_type);
sparc_reg sparc_reg::reg_gsr(gsr_type);
sparc_reg sparc_reg::reg_asi(asi_type);
sparc_reg sparc_reg::reg_pil(pil_type);

sparc_reg::sparc_reg(const sparc_reg &src) : reg_t(src) {}

sparc_reg::~sparc_reg() {}

sparc_reg::sparc_reg(RAWIntReg, unsigned u) : reg_t(u) {
   assert(u < 32);
}

sparc_reg::sparc_reg(GLOBAL, unsigned n) : reg_t(n) {
   assert(n < 8);
}

sparc_reg::sparc_reg(OUT, unsigned n) : reg_t(8+n) {
   assert(n < 8);
}

sparc_reg::sparc_reg(LOCAL, unsigned n) : reg_t(16+n) {
   assert(n < 8);
}

sparc_reg::sparc_reg(IN, unsigned n) : reg_t(24+n) {
   assert(n < 8);
}

sparc_reg::sparc_reg(FP, unsigned n) : reg_t(32+n) {
   assert(n < 64);
}

sparc_reg::sparc_reg(Icc_type) : reg_t(96) {}
sparc_reg::sparc_reg(Xcc_type) : reg_t(97) {}
sparc_reg::sparc_reg(Fcc_type, unsigned num) : reg_t(98 + num) {
   assert(num < 4);
}
sparc_reg::sparc_reg(PC_type)  : reg_t(102) {}
sparc_reg::sparc_reg(GSR_type) : reg_t(103) {}
sparc_reg::sparc_reg(ASI_type) : reg_t(104) {}
sparc_reg::sparc_reg(PIL_type) : reg_t(105) {}

sparc_reg& sparc_reg::operator=(const sparc_reg &src) {
   regnum = src.regnum;
   return *this;
}

bool sparc_reg::isIntReg() const { return (regnum < 32); }
   
bool sparc_reg::is_gReg(unsigned &num) const {
   if (regnum < 8) {
      num = regnum;
      return true;
   }
   else
      return false;
}

bool sparc_reg::is_oReg(unsigned &num) const {
   if (regnum >= 8 && regnum < 16) {
      num = regnum-8;
      return true;
   }
   else
      return false;
}

bool sparc_reg::is_lReg(unsigned &num) const {
   if (regnum >= 16 && regnum < 24) {
      num = regnum-16;
      return true;
   }
   else
      return false;
}

bool sparc_reg::is_iReg(unsigned &num) const {
   if (regnum >= 24 && regnum < 32) {
      num = regnum-24;
      return true;
   }
   else
      return false;
}

bool sparc_reg::isFloatReg() const { 
   return (regnum >= 32 && regnum < 96); 
}

bool sparc_reg::isIntCondCode() const { 
   return (regnum >= 96 && regnum <= 97); 
}

bool sparc_reg::isIcc() const { 
   return (regnum==96); 
}

bool sparc_reg::isXcc() const { 
   return (regnum==97); 
}

bool sparc_reg::isFloatCondCode() const { 
   return (regnum >= 98 && regnum <= 101);
}

bool sparc_reg::isFcc0() const { 
   return regnum == 98; 
}

bool sparc_reg::isFcc1() const { 
   return regnum == 99; 
}

bool sparc_reg::isFcc2() const { 
   return regnum == 100; 
}

bool sparc_reg::isFcc3() const { 
   return regnum == 101; 
}

unsigned sparc_reg::getFloatCondCodeNum() const {
   assert(isFloatCondCode());
   return (regnum - 98);
}

bool sparc_reg::isPC() const { 
   return (regnum == 102); 
}

bool sparc_reg::isGSR() const { 
   return (regnum == 103); 
}

bool sparc_reg::isASI() const { 
   return (regnum == 104); 
}

bool sparc_reg::isPIL() const { 
   return (regnum == 105); 
}

sparc_reg sparc_reg::regLocationAfterASave() const {
   if (!isIntReg())
      // unchanged after a save
      return *this;

   unsigned num;
   if (is_gReg(num))
      // unchanged after a save
      return *this;

   if (is_oReg(num))
      // e.g. %o5 --> %i5 after the save
      return sparc_reg(sparc_reg::in, num);
      
   if (is_lReg(num) || is_iReg(num))
      throw UnreachableAfterSave();
      
   assert(false && "unknown reg type");
}

sparc_reg sparc_reg::regLocationAfterARestore() const {
   if (!isIntReg())
      // unchanged after a save
      return *this;

   unsigned num;
   if (is_gReg(num))
      // unchanged after a save
      return *this;

   if (is_iReg(num))
      // e.g. %i5 --> %o5 after the save
      return sparc_reg(sparc_reg::out, num);
      
   if (is_lReg(num) || is_oReg(num))
      throw UnreachableAfterRestore();
      
   assert(false && "unknown reg type");
}

unsigned sparc_reg::getIntNum() const {
   // asserts this is an int reg, then returns 0-31, as appropriate
   assert(isIntReg());
   return regnum;
}
   
unsigned sparc_reg::getFloatNum() const {
   // asserts this is an fp reg, then returns 0-63, as appropriate
   assert(isFloatReg());
   return regnum - 32;
}

reg_t& sparc_reg::getRegByBit(unsigned bit) {
   switch (bit) {
   case 0:
      return sparc_reg::g0;
   case 1:
      return sparc_reg::g1;
   case 2:
      return sparc_reg::g2;
   case 3:
      return sparc_reg::g3;
   case 4:
      return sparc_reg::g4;
   case 5:
      return sparc_reg::g5;
   case 6:
      return sparc_reg::g6;
   case 7:
      return sparc_reg::g7;
   case 8:
      return sparc_reg::o0;
   case 9: 
      return sparc_reg::o1;
   case 10:
      return sparc_reg::o2;
   case 11: 
      return sparc_reg::o3;
   case 12:
      return sparc_reg::o4;
   case 13: 
      return sparc_reg::o5;
   case 14:
      return sparc_reg::o6;
   case 15: 
      return sparc_reg::o7;
   case 16:
      return sparc_reg::l0;
   case 17: 
      return sparc_reg::l1;
   case 18:
      return sparc_reg::l2;
   case 19: 
      return sparc_reg::l3;
   case 20:
      return sparc_reg::l4;
   case 21: 
      return sparc_reg::l5;
   case 22:
      return sparc_reg::l6;
   case 23: 
      return sparc_reg::l7;
   case 24:
      return sparc_reg::i0;
   case 25: 
      return sparc_reg::i1;
   case 26:
      return sparc_reg::i2;
   case 27: 
      return sparc_reg::i3;
   case 28:
      return sparc_reg::i4;
   case 29: 
      return sparc_reg::i5;
   case 30:
      return sparc_reg::i6;
   case 31: 
      return sparc_reg::i7;
   case 32:
      return sparc_reg::f0;
   case 33: 
      return sparc_reg::f1;
   case 34:
      return sparc_reg::f2;
   case 35: 
      return sparc_reg::f3;
   case 36:
      return sparc_reg::f4;
   case 37: 
      return sparc_reg::f5;
   case 38:
      return sparc_reg::f6;
   case 39: 
      return sparc_reg::f7;
   case 40:
      return sparc_reg::f8;
   case 41: 
      return sparc_reg::f9;
   case 42:
      return sparc_reg::f10;
   case 43: 
      return sparc_reg::f11;
   case 44:
      return sparc_reg::f12;
   case 45: 
      return sparc_reg::f13;
   case 46:
      return sparc_reg::f14;
   case 47: 
      return sparc_reg::f15;
   case 48:
      return sparc_reg::f16;
   case 49: 
      return sparc_reg::f17;
   case 50:
      return sparc_reg::f18;
   case 51: 
      return sparc_reg::f19;
   case 52:
      return sparc_reg::f20;
   case 53: 
      return sparc_reg::f21;
   case 54:
      return sparc_reg::f22;
   case 55: 
      return sparc_reg::f23;
   case 56:
      return sparc_reg::f24;
   case 57: 
      return sparc_reg::f25;
   case 58:
      return sparc_reg::f26;
   case 59: 
      return sparc_reg::f27;
   case 60:
      return sparc_reg::f28;
   case 61: 
      return sparc_reg::f29;
   case 62:
      return sparc_reg::f30;
   case 63: 
      return sparc_reg::f31;
   case 64:
      return sparc_reg::f32;
   case 65: 
      return sparc_reg::f33;
   case 66:
      return sparc_reg::f34;
   case 67: 
      return sparc_reg::f35;
   case 68:
      return sparc_reg::f36;
   case 69: 
      return sparc_reg::f37;
   case 70:
      return sparc_reg::f38;
   case 71: 
      return sparc_reg::f39;
   case 72:
      return sparc_reg::f40;
   case 73: 
      return sparc_reg::f41;
   case 74:
      return sparc_reg::f42;
   case 75: 
      return sparc_reg::f43;
   case 76:
      return sparc_reg::f44;
   case 77: 
      return sparc_reg::f45;
   case 78:
      return sparc_reg::f46;
   case 79: 
      return sparc_reg::f47;
   case 80:
      return sparc_reg::f48;
   case 81: 
      return sparc_reg::f49;
   case 82:
      return sparc_reg::f50;
   case 83: 
      return sparc_reg::f51;
   case 84:
      return sparc_reg::f52;
   case 85: 
      return sparc_reg::f53;
   case 86:
      return sparc_reg::f54;
   case 87: 
      return sparc_reg::f55;
   case 88:
      return sparc_reg::f56;
   case 89: 
      return sparc_reg::f57;
   case 90:
      return sparc_reg::f58;
   case 91: 
      return sparc_reg::f59;
   case 92:
      return sparc_reg::f60;
   case 93: 
      return sparc_reg::f61;
   case 94:
      return sparc_reg::f62;
   case 95: 
      return sparc_reg::f63;
   case 96:
      return sparc_reg::reg_icc;
   case 97:
      return sparc_reg::reg_xcc;
   case 98:
      return sparc_reg::reg_fcc0;
   case 99:
      return sparc_reg::reg_fcc1;
   case 100:
      return sparc_reg::reg_fcc2;
   case 101:
      return sparc_reg::reg_fcc3;
   case 102:
      return sparc_reg::reg_pc;
   case 103:
      return sparc_reg::reg_gsr;
   case 104:
      return sparc_reg::reg_asi;
   case 105:
      return sparc_reg::reg_pil;
   default:
      assert(false);
      abort(); // placate compiler when compiling with NDEBUG
   }
}

static const pdstring preDisassembledRegs[] = {
   "%g0", "%g1", "%g2", "%g3", "%g4", "%g5", "%g6", "%g7",
   "%o0", "%o1", "%o2", "%o3", "%o4", "%o5", "%sp", "%o7",
   "%l0", "%l1", "%l2", "%l3", "%l4", "%l5", "%l6", "%l7",
   "%i0", "%i1", "%i2", "%i3", "%i4", "%i5", "%fp", "%i7",
   
   "%f0", "%f1", "%f2", "%f3", "%f4", "%f5", "%f6", "%f7", "%f8", "%f9",
   "%f10", "%f11", "%f12", "%f13", "%f14", "%f15", "%f16", "%f17", "%f18", "%f19",
   "%f20", "%f21", "%f22", "%f23", "%f24", "%f25", "%f26", "%f27", "%f28", "%f29",
   "%f30", "%f31", "%f32", "%f33", "%f34", "%f35", "%f36", "%f37", "%f38", "%f39",
   "%f40", "%f41", "%f42", "%f43", "%f44", "%f45", "%f46", "%f47", "%f48", "%f49",
   "%f50", "%f51", "%f52", "%f53", "%f54", "%f55", "%f56", "%f57", "%f58", "%f59",
   "%f60", "%f61", "%f62", "%f63",

   "%icc", "%xcc", "%fcc0", "%fcc1", "%fcc2", "%fcc3",
   "%pc", "%gsr", "%asi", "%pil"
};

pdstring sparc_reg::disass() const {
   return preDisassembledRegs[regnum];
}

void rollinIntReg(unsigned &result, sparc_reg r) {
   rollin(result, uimmediate<5>(r.getIntNum()));
}

void rollinFpReg(unsigned &result, sparc_reg r, unsigned precision) {
   // Encoding is weird, due to v8 compatibility issues
   // See 5.1.4.1 in v9 architecture manual

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
