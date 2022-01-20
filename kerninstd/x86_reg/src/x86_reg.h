// x86_reg.h

#ifndef _X86_REG_H_
#define _X86_REG_H_

#include "uimm.h"
#include "common/h/String.h"

#include "reg.h"
      
class x86_reg : public reg_t {
 private:
   class NullReg         {};
   class RawIntReg       {};
   class GeneralModRMReg {};
   class SegmentReg      {};
   class ControlReg      {};
   class DebugReg        {};

   // These are probably obsolete:
   //void disass_int(char *) const {}
   //void disass_float(char *) const {}
   //void disass_misc(char *) const {}

 public:

   class malformedreg {}; // An exception
   static const unsigned max_reg_num;

   // We used to have an enumerated type for the registers, but it opened up
   // doors to ambiguity w.r.t. integers.  So instead we use static member 
   // variables for each register, which provides for strong type checking:

   //               GENERAL INTEGER REGISTERS
   //    8-bits              16-bits               32-bits
   static x86_reg AL;   static x86_reg AX;   static x86_reg eAX;
   static x86_reg CL;	static x86_reg CX;   static x86_reg eCX;
   static x86_reg DL;	static x86_reg DX;   static x86_reg eDX;
   static x86_reg BL;	static x86_reg BX;   static x86_reg eBX;
   static x86_reg AH;	static x86_reg SP;   static x86_reg eSP;
   static x86_reg CH;	static x86_reg BP;   static x86_reg eBP;
   static x86_reg DH;	static x86_reg SI;   static x86_reg eSI;
   static x86_reg BH;	static x86_reg DI;   static x86_reg eDI;
   static const unsigned min_int_reg;
   static const unsigned max_int_reg;

   // SEGMENT REGISTERS
   //     16-bits 
   static x86_reg ES;
   static x86_reg CS;
   static x86_reg SS;
   static x86_reg DS;
   static x86_reg FS;
   static x86_reg GS;
   static const unsigned min_seg_reg;
   static const unsigned max_seg_reg;

   // Misc regs
   static x86_reg EFLAGS;
   static x86_reg PC; 
   static x86_reg FP;
   static const unsigned min_misc_reg;
   static const unsigned max_misc_reg;

   static x86_reg CR0;
   static x86_reg CR1;
   static x86_reg CR2;
   static x86_reg CR3;
   static x86_reg CR4;
   static const unsigned min_ctl_reg;
   static const unsigned max_ctl_reg;

   static x86_reg DR0;
   static x86_reg DR1;
   static x86_reg DR2;
   static x86_reg DR3;
   static x86_reg DR4;
   static x86_reg DR5;
   static x86_reg DR6;
   static x86_reg DR7;
   static const unsigned min_dbg_reg;
   static const unsigned max_dbg_reg;

   static x86_reg nullreg;

   static NullReg            nullReg;
   static RawIntReg          rawIntReg;
   static GeneralModRMReg    generalModRMReg;
   static SegmentReg         segmentReg;
   static ControlReg         controlReg;
   static DebugReg           debugReg;

   x86_reg(const x86_reg &src) : reg_t(src) {}

   x86_reg &operator=(const x86_reg &src) {
      regnum = src.regnum;
      return *this;
   }

   x86_reg(unsigned i, const char *dis);
   x86_reg(RawIntReg, unsigned i);
   x86_reg(NullReg);
   x86_reg(GeneralModRMReg, unsigned u, unsigned width);
   x86_reg(SegmentReg, unsigned u);
   x86_reg(ControlReg, unsigned u);
   x86_reg(DebugReg, unsigned u);

   ~x86_reg();

   bool operator==(const x86_reg &src) const;
   bool operator!=(const x86_reg &src) const;

   bool isIntReg() const;
   bool is32BitIntReg() const;
   bool isSegReg() const;
   
   bool isEFLAGS() const;
   bool isPC() const;
   bool isFP() const;
   
   bool isCtlReg() const;
   bool isNullReg() const;

   // FIXME:
   bool isFloatReg() const;
   bool isIntCondCode() const;
   bool isFloatCondCode() const;

   unsigned getIntNum() const;
   unsigned getFloatNum() const;
   static reg_t& getRegByBit(unsigned);
   unsigned char getRegRMVal() const;

   pdstring disass() const;
   static pdstring disassx(unsigned);
};

inline void rollinIntReg(unsigned &result, x86_reg r) {
   rollin(result, uimmediate<5>(r.getIntNum()));
}

void rollinFpReg(unsigned &, x86_reg, unsigned precision);

#endif
