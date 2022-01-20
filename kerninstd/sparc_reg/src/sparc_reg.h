// sparc_reg.h
// Ariel Tamches

#ifndef _SPARC_REG_H_
#define _SPARC_REG_H_

#include "common/h/Vector.h"
#include "uimm.h"
#include "common/h/String.h"

#include "reg.h"

// Quick refresher course on SPARC integer registers:
// 0-7 map to %g0-%g7
// 8-15 map to %o0-%o7   %o6 (r14) is %sp    
// 16-23 map to %l0-%l7
// 24-31 map to %i0-%i7  %i6 (r30) is %fp  (so a save sets %fp to prev frame's %sp)
//
// regs o0-o7 except o6 (aka %sp) are assumed volatile across a procedure call,
//    so don't put anything so important that it must survive a proc call in one
//    of these regs, unless you want to take the time to save them to stack before
//    the call, and restore them afterwards.
// regs i0-i7 and l0-l7 are assumed preserved across a procedure call.  Why?
//    simple.  Because the save instruction creates a new window.  But what about
//    calling an optimized leaf routine, which has no save?  Well then the leaf
//    routine is responsible for maintaining the invariant that the caller's %i
//    and %l regs are preserved.
// all condition codes, all fp regs, and the y reg are assumed volatile
//    across a procedure call, like the %o registers (except %o6 aka %sp).
// call writes to %o7 (r15), so a subsequent save in the callee puts the
//    return address (well actually return address-8) into the callee's %i7.
//    Of course an optimized leaf procedure has its return addr-8 in its
//    %o7.
// trap writes to l1 and l2 (r17 and r18) of the new register window
      
class sparc_reg : public reg_t {
 public:
   class UnreachableAfterSave {};
   class UnreachableAfterRestore {};
   
 private:
   /* unsigned regnum; - defined in parent class */
      // 0 thru 31 for conventional int regs
      // 32 thru 95 for conventional fp regs
      // 96 for icc.  97 for xcc.
      // 98 thru 101 for fcc0-3, respectively.
      // 102 for PC.
      // 103 for GSR (graphics status register of UltraSPARC).
      // 104 for %asi
      // 105 for %pil
      // I've thought about it, and %icc and %xcc should *not* be combined
      // into a single register, either here or in sparc_reg_set.

   class GLOBAL {};
   class OUT {};
   class LOCAL {};
   class IN {};

   class FP {};

   class RAWIntReg {};

   class Icc_type {};
   class Xcc_type {};
   class Fcc_type {};
   class PC_type {};
   class GSR_type {}; // graphix status register
   class ASI_type {}; // %asi
   class PIL_type {}; // %pil

   static Icc_type icc_type;
   static Xcc_type xcc_type;
   static PC_type  pc_type;
   static GSR_type gsr_type;
   static ASI_type asi_type;
   static PIL_type pil_type;

   sparc_reg(Icc_type);
   sparc_reg(Xcc_type);
   sparc_reg(PC_type);
   sparc_reg(GSR_type);
   sparc_reg(ASI_type);
   sparc_reg(PIL_type);

   //void disass_int(char *buffer) const;
   //void disass_float(char *buffer) const;
   //void disass_misc(char *buffer) const;

 public:

   static const unsigned max_reg_num;

   // We used to have an enumerated type for the registers, but it opened up
   // doors to ambiguity w.r.t. integers.  So instead we use static member 
   // variables for each register, which provides for strong type checking:

   static sparc_reg g0;    static sparc_reg g1;    static sparc_reg g2;
   static sparc_reg g3;    static sparc_reg g4;    static sparc_reg g5;
   static sparc_reg g6;    static sparc_reg g7;
   static sparc_reg i0;    static sparc_reg i1;    static sparc_reg i2;
   static sparc_reg i3;    static sparc_reg i4;    static sparc_reg i5;
   static sparc_reg i6;    static sparc_reg fp; // same as i6
   static sparc_reg i7;
   static sparc_reg l0;    static sparc_reg l1;    static sparc_reg l2;
   static sparc_reg l3;    static sparc_reg l4;    static sparc_reg l5;
   static sparc_reg l6;    static sparc_reg l7;
   static sparc_reg o0;    static sparc_reg o1;    static sparc_reg o2;
   static sparc_reg o3;    static sparc_reg o4;    static sparc_reg o5;
   static sparc_reg o6;    static sparc_reg sp; // same as o6
   static sparc_reg o7;
   
   static sparc_reg f0;    static sparc_reg f1;    static sparc_reg f2;
   static sparc_reg f3;    static sparc_reg f4;    static sparc_reg f5;
   static sparc_reg f6;    static sparc_reg f7;    static sparc_reg f8;
   static sparc_reg f9;    
   static sparc_reg f10;    static sparc_reg f11;    static sparc_reg f12;
   static sparc_reg f13;    static sparc_reg f14;    static sparc_reg f15;
   static sparc_reg f16;    static sparc_reg f17;    static sparc_reg f18;
   static sparc_reg f19;    
   static sparc_reg f20;    static sparc_reg f21;    static sparc_reg f22;
   static sparc_reg f23;    static sparc_reg f24;    static sparc_reg f25;
   static sparc_reg f26;    static sparc_reg f27;    static sparc_reg f28;
   static sparc_reg f29;    
   static sparc_reg f30;    static sparc_reg f31;    static sparc_reg f32;
   static sparc_reg f33;    static sparc_reg f34;    static sparc_reg f35;
   static sparc_reg f36;    static sparc_reg f37;    static sparc_reg f38;
   static sparc_reg f39;    
   static sparc_reg f40;    static sparc_reg f41;    static sparc_reg f42;
   static sparc_reg f43;    static sparc_reg f44;    static sparc_reg f45;
   static sparc_reg f46;    static sparc_reg f47;    static sparc_reg f48;
   static sparc_reg f49;    
   static sparc_reg f50;    static sparc_reg f51;    static sparc_reg f52;
   static sparc_reg f53;    static sparc_reg f54;    static sparc_reg f55;
   static sparc_reg f56;    static sparc_reg f57;    static sparc_reg f58;
   static sparc_reg f59;    static sparc_reg f60;    static sparc_reg f61;
   static sparc_reg f62;    static sparc_reg f63;    


   static sparc_reg reg_icc;
   static sparc_reg reg_xcc;
   static sparc_reg reg_fcc0;
   static sparc_reg reg_fcc1;
   static sparc_reg reg_fcc2;
   static sparc_reg reg_fcc3;
   static sparc_reg reg_pc;
   static sparc_reg reg_gsr;
   static sparc_reg reg_asi;
   static sparc_reg reg_pil;

   static RAWIntReg rawIntReg;
   static GLOBAL global;
   static OUT out;
   static LOCAL local;
   static IN in;
   static FP f;
   static Fcc_type fcc_type;

   sparc_reg(const sparc_reg &src);
      
   sparc_reg(RAWIntReg, unsigned u);
   sparc_reg(GLOBAL, unsigned);
   sparc_reg(OUT, unsigned);
   sparc_reg(LOCAL, unsigned);
   sparc_reg(IN, unsigned);

   sparc_reg(FP, unsigned);
   sparc_reg(Fcc_type, unsigned fccnum);

   ~sparc_reg();

   sparc_reg& operator=(const sparc_reg &src);
   
   bool isIntReg() const;
   bool is_gReg(unsigned &num) const;
   bool is_oReg(unsigned &num) const;
   bool is_lReg(unsigned &num) const;
   bool is_iReg(unsigned &num) const;
      
   bool isFloatReg() const;
   bool isIntCondCode() const;
   bool isIcc() const;
   bool isXcc() const;
   bool isFloatCondCode() const;
   bool isFcc0() const;
   bool isFcc1() const;
   bool isFcc2() const;
   bool isFcc3() const;
   unsigned getFloatCondCodeNum() const;
   
   bool isPC() const;
   bool isGSR() const;
   bool isASI() const;
   bool isPIL() const;

   sparc_reg regLocationAfterASave() const;
   sparc_reg regLocationAfterARestore() const;
   
   unsigned getIntNum() const;
   unsigned getFloatNum() const;
   static reg_t& getRegByBit(unsigned);

   pdstring disass() const;
};

void rollinIntReg(unsigned &result, sparc_reg r);

void rollinFpReg(unsigned &, sparc_reg, unsigned precision);

#endif
