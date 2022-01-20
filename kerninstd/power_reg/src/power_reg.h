// power_reg.h
// Igor Grobman (based on sparc_reg by Ariel Tamches )

#ifndef _POWER_REG_H_
#define _POWER_REG_H_

#include "common/h/Vector.h"
#include "uimm.h"
#include "common/h/String.h"
#include "reg.h"

//Some notes on Power register conventions:
//There are 32 General-Purpose integer registers (GPRs).  Also, there are 32 
//Floating point registers.  Note that all FPRs store double precision values 
// only (can be converted to/from single precision on load/store using 
// appropriate instructions).
//
//GPR0-12 and FPR0-13 are volatile across function calls, GPR13-31 and FPR14-31 
//are assumed preserved.  Note that it is the procedure's responsibility to 
// preserve the contents of these registers, unlike SPARC, there is no magic 
// save instruction. Conventions:
//  
// GPR1 -- stack pointer
// GPR2 -- TOC pointer
//
// GPR3-10 --argument passing 
//
// FPR0 -- scratch register
// FPR1-FPR13 -- argument passing
//
// Special registers are assumed volatile across calls (except CR2-5 fields).
// LR -- link register
// CTR -- counter register
// XER -- integer exception register
//   * SO -- Summary overflow field (gets set once)
//   * OV -- Overflow field (set/reset on every instr. that affects it)
//   * CA -- carry
//   * Reserved (3:24)
//   * # Bytes for load/store string instructions (25:31)
// FPSCR -- floating point exception register
// CR -- condition register, consists of 8 4-bit fields CR0-CR7
//
// System registers
// ASR -- address space register (64-bit), used for segmentation
// DAR -- data access register, stores interrupting instructions' 
//        address (64-bit)
// EAR -- external access register
// DSISR  -- data storage interrupt status register--cause of interrupt (32-bit)
// SPRG0 -- real address for storage used by first-level interrupt handler
// SPRG1 -- scratch register for first-level interrupt handler
// SPRG2-3 -- used by OS
// MSR -- machine state register
// DEC -- decrementer (used to generate decrementer interrupt)
// TB, TBU -- time base registers
// PVR -- processor version register (read-only using mfspr)
// SDR1 -- Storage description register (used for paging)
// 16 Segment registers (only used in 32-bit implementations, 
//  of no concern here)
// SRR0-1 -- Machine status save/restore rgisters (used on interrupts)
// IBAT0U-DBAT3L -- Block address translation registers (16 total)
// CR0-CR7 -- condition register fields



class power_reg : public reg_t {
 public:
 ~power_reg() { } 
   
 private:
   // unsigned regnum;
      // 0 thru 31 for GPRs
      // 32 thru 63 for  fp regs
      // 64 is LR, 65 is CTR, 66 is XER, 67 is FPSCR, 68 is CR, 
      // 69-70 for TB and TBU
      // 71 is ASR, 72 is DAR, 73 is DSISR, 74-77 for SPRGs, 78 is MSR
      // 79 is DEC, 80 is PVR, 81 is SDR1,
      // 82-83 for SRRs, 84 is EAR
      //Performance regs: 85 MMCRA,  86-93 PMC1-PMC8 94 MMCR0 
      //                  95 SIAR 96 SDAR 97 MMCR1
      //98 DABR
      //99 PIR 
      //100 HDEC 
      // 101-108 for CR fields
      // 109 for SO, 110 for OV, 111 for CA, 112 for XER reserved, 
      //113 for XER load/store string bytes
      //114 ACCR
      //115 CTRL
      //116 is null
      //HACK: 85-100 used to be BAT, but since we run out of 
      //the 64 bits of miscregset bitfield, we ignore BATs for now.


   class FP {};
   class RAWIntReg {};

   class LINKREG {};
   class COUNTERREG {};
   class XEREG {};  //Use XER fields instead
   //XER fields
   class SO {};
   class OV {};
   class XERRESV {};
   class XERSTRBYTES {};
   class CA {};

   //note we don't split FPSCR into fields, like we do with CR and XER.
   //This is because floating point instructions are likely to modify
   //the majority of FPSCR fields, and keeping track of every field
   //becomes a useless task. -Igor
   class FPSCREG {};
   class CONDREG {};  //Use CR fields.  We don't keep track of the whole reg.

   //We treat CR fields as registers
   class CRFIELD {};

   class TBREG {};
   class ASREG {};
   class DAREG {};
   class DSISREG {};
   class SPRGREG {};
   class MSREG {};
   class DECREG {};
   class PVREG {};
   class SDR1REG {};
   class SRREG {};
   class EAREG {};
   class BATREG {};
   class SPREG {};
   class ACCREG {};
   class CTRLREG {};
   class HDECREG {};
   class MMCRAREG {};
   class PMCREG {};
   class MMCR0REG {};
   class SIAREG {};
   class SDAREG {};
   class MMCR1REG {};
   class DABREG {};
   class PIREG {}; 
   class Null {}; //Non-existent in the actual architecture, but
                  //useful as default value and error-check.
   
 public:
   static LINKREG lr_type;
   static COUNTERREG ctr_type;
   static XEREG xer_type;
   static SO so_type;
   static OV ov_type;
   static CA ca_type;
   static XERRESV xerresv_type;
   static XERSTRBYTES xerstrbytes_type;
   static FPSCREG fpscr_type;
   static CONDREG cr_type;
   static CRFIELD crfield_type;
   static TBREG tbr_type;
   static ASREG asr_type;
   static DAREG dar_type;
   static DSISREG dsisr_type;
   static SPRGREG sprg_type;
   static MSREG msr_type;
   static DECREG dec_type;
   static PVREG pvr_type;
   static SDR1REG sdr1_type;
   static SRREG srr_type;
   static EAREG ear_type;
   static BATREG bat_type;
   static SPREG spr_type;
   static ACCREG accr_type;
   static CTRLREG ctrl_type;
   static HDECREG hdec_type;
   static MMCRAREG mmcra_type;
   static PMCREG pmc_type;
   static MMCR0REG mmcr0_type;
   static SIAREG siar_type;
   static SDAREG sdar_type;
   static MMCR1REG mmcr1_type;
   static DABREG dabr_type;
   static PIREG pir_type;
   static Null null_type;
   
   power_reg(LINKREG);
   power_reg(COUNTERREG);
   power_reg(XEREG);
   power_reg(SO);
   power_reg(OV);
   power_reg(CA);
   power_reg(XERRESV);
   power_reg(XERSTRBYTES);
   power_reg(FPSCREG);
   power_reg(CONDREG);
   power_reg(CRFIELD, unsigned);
   
   power_reg(TBREG, unsigned); //TB = 0, TBU=1
   power_reg(ASREG);
   power_reg(DAREG);
   power_reg(DSISREG);
   power_reg(SPRGREG, unsigned);
   power_reg(MSREG);
   power_reg(DECREG);
   power_reg(PVREG);
   power_reg(SDR1REG);
   power_reg(SRREG, unsigned);
   power_reg(EAREG);
   power_reg(BATREG, unsigned); //IBT0U=0 ... DBAT3L=15
   power_reg(ACCREG);
   power_reg(CTRLREG);
   power_reg(HDECREG);
   power_reg(MMCRAREG);
   power_reg(PMCREG, unsigned);
   power_reg(MMCR0REG);
   power_reg(SIAREG);
   power_reg(SDAREG);
   power_reg(MMCR1REG);
   power_reg(DABREG);
   power_reg(PIREG);
   power_reg(Null);
  
   pdstring disass() const;

   //These appear to be unimplemented, even for sparc -Igor
   void disass_int(char * /* buffer */) const { assert(false && "nyi"); }
   void disass_float(char * /* buffer */) const { assert(false && "nyi"); }
   void disass_misc(char * /* buffer */) const { assert(false && "nyi"); }

   static const unsigned max_reg_num = 116;
   static const unsigned total_num_regs = max_reg_num+1;

   //general purpose register declarations
   static power_reg gpr0;  static power_reg gpr1;   static power_reg gpr2;
   static power_reg gpr3;  static power_reg gpr4;   static power_reg gpr5;
   static power_reg gpr6;  static power_reg gpr7;   static power_reg gpr8;
   static power_reg gpr9;  static power_reg gpr10;  static power_reg gpr11;
   static power_reg gpr12; static power_reg gpr13;  static power_reg gpr14;
   static power_reg gpr15; static power_reg gpr16;  static power_reg gpr17;
   static power_reg gpr18; static power_reg gpr19;  static power_reg gpr20;
   static power_reg gpr21; static power_reg gpr22;  static power_reg gpr23;
   static power_reg gpr24; static power_reg gpr25;  static power_reg gpr26;
   static power_reg gpr27; static power_reg gpr28;  static power_reg gpr29;
   static power_reg gpr30; static power_reg gpr31; 
   
   //floating point registers
   static power_reg fpr0;  static power_reg fpr1;   static power_reg fpr2;
   static power_reg fpr3;  static power_reg fpr4;   static power_reg fpr5;
   static power_reg fpr6;  static power_reg fpr7;   static power_reg fpr8;
   static power_reg fpr9;  static power_reg fpr10;  static power_reg fpr11;
   static power_reg fpr12; static power_reg fpr13;  static power_reg fpr14;
   static power_reg fpr15; static power_reg fpr16;  static power_reg fpr17;
   static power_reg fpr18; static power_reg fpr19;  static power_reg fpr20;
   static power_reg fpr21; static power_reg fpr22;  static power_reg fpr23;
   static power_reg fpr24; static power_reg fpr25;  static power_reg fpr26;
   static power_reg fpr27; static power_reg fpr28;  static power_reg fpr29;
   static power_reg fpr30; static power_reg fpr31; 
   

   //special registers
   
   static power_reg lr;   static power_reg ctr;    static power_reg xer;
   static power_reg so;   static power_reg ov;     static power_reg ca;
   static power_reg xerresv; static power_reg xerstrbytes;
   static power_reg fpscr; static power_reg cr;     static power_reg asr;
   static power_reg dar;  static power_reg dsisr;  static power_reg msr;
   static power_reg dec;  static power_reg pvr;    static power_reg sdr1;
   static power_reg srr0; static power_reg srr1;   static power_reg ear;
   static power_reg accr; static power_reg ctrl;   static power_reg hdec;
   static power_reg mmcra; static power_reg mmcr0; static power_reg siar;
   static power_reg sdar; static power_reg mmcr1; static power_reg dabr;
   static power_reg pir;


   //Condition register fields
   static power_reg cr0;         static power_reg cr4;
   static power_reg cr1;         static power_reg cr5;
   static power_reg cr2;         static power_reg cr6;
   static power_reg cr3;         static power_reg cr7;

   static power_reg tb;   static power_reg tbu;

   static power_reg sprg0; static power_reg sprg1; 
   static power_reg sprg2; static power_reg sprg3;
   
   static power_reg ibat0u; static power_reg ibat0l;
   static power_reg ibat1u; static power_reg ibat1l;
   static power_reg ibat2u; static power_reg ibat2l;
   static power_reg ibat3u; static power_reg ibat3l;
   static power_reg dbat0u; static power_reg dbat0l;
   static power_reg dbat1u; static power_reg dbat1l;
   static power_reg dbat2u; static power_reg dbat2l;
   static power_reg dbat3u; static power_reg dbat3l;

   static power_reg pmc1;  static power_reg pmc2; 
   static power_reg pmc3;  static power_reg pmc4; 
   static power_reg pmc5;  static power_reg pmc6; 
   static power_reg pmc7;  static power_reg pmc8; 
   
   static power_reg nullreg;
   

   // We used to have an enumerated type for the registers, but it opened up
   // doors to ambiguity w.r.t. integers.  So instead we use static member 
   // vrbles for each register, which provides for strong type checking:

   static RAWIntReg rawIntReg;
   static FP f;

   
   //   power_reg(const power_reg &src) : regnum(src.regnum) {
   //     assert(regnum <= max_reg_num);
   // }
      
   power_reg(RAWIntReg, unsigned u) : reg_t(u) {
      assert(u < 32);
   }

   power_reg(FP, unsigned);
  
   bool isIntReg() const { return (regnum < 32); }
   bool isFloatReg() const { return (regnum >= 32 && regnum < 64); }
   
   bool isLinkReg() const { return (regnum == 64); }
   bool isCtrReg() const { return (regnum == 65); }
   bool isXerReg() const { return (regnum == 66); }
   bool isSO() const {return (regnum == 109); }
   bool isOV() const { return (regnum == 110); }
   bool isCA() const { return (regnum == 111); }
   bool isXerResv() const { return (regnum == 112); }
   bool isXerStrBytes() const {return (regnum == 113); }
   bool isFpscReg() const { return (regnum == 67); }
   bool isCondReg() const { return (regnum == 68); }
   bool isCrField() const {return (regnum >=101 && regnum < 109); }
   bool isTbReg() const { return ( (regnum == 69) || (regnum == 70) ); }
   bool isTb() const { return (regnum == 69); }
   bool isTbu() const { return (regnum == 70); }

   bool isAsReg() const { return (regnum == 71); }
   bool isDaReg() const { return (regnum == 72); }
   bool isDsisReg() const { return (regnum == 73); }
   bool isSprgReg() const { return (regnum >= 74 && regnum < 78); }
   bool isMsReg() const { return (regnum == 78); }
   bool isDecReg() const { return (regnum == 79); }
   bool isPvReg() const { return (regnum == 80); }
   bool isSdr1Reg() const { return (regnum == 81); }
   bool isSrReg() const { return ( (regnum == 82) || (regnum == 83) );  }
   bool isEaReg() const { return (regnum == 84); }
   bool isBatReg() const { return false; }
   bool isAccReg() const { return (regnum == 114); }
   bool isCtrlReg() const { return (regnum == 115); }
   bool isMmcraReg() const { return (regnum == 85); }
   bool isPmcReg() const { return (regnum >= 86 && regnum < 94); }
   bool isMmcr0Reg() const {return (regnum == 94); }
   bool isSiaReg() const {return (regnum == 95); }
   bool isSdaReg() const { return (regnum == 96); }
   bool isMmcr1Reg() const { return (regnum == 97); }
   bool isDabReg() const { return (regnum == 98); }
   bool isPiReg() const {return (regnum == 99); } 
   bool isHdecReg() const { return (regnum == 100); }
   bool isNullReg() const { return (regnum == 116); }
   bool isSprReg() const { return (regnum > 63); }
   bool isParamReg(unsigned &num) const;   
 
   //The following don't make sense on power
   bool isIntCondCode() const { 
      assert(false); 
      return false; 
   }
   bool isFloatCondCode() const {
      assert(false); 
      return false;
   }
   bool isPC() const {
      assert(false); 
      return false; //placate compiler
   }
   

   

   unsigned getIntNum() const {
      // asserts this is an int reg, then returns 0-31, as appropriate
      assert(isIntReg());
      return regnum;
   }
   
   unsigned getFloatNum() const {
      // asserts this is an fp reg, then returns 0-31, as appropriate
      assert(isFloatReg());
      return regnum - 32;
   }

   unsigned getCrFieldNum() const {
     assert(isCrField());
     return regnum - 101;
   }
   

   //This function is needed because we want set_form instructions to
   //be able to transparently deal with any register type.
   unsigned getIntOrFloatNum() const {
     assert(isIntReg() || isFloatReg() );
     if (isIntReg())
       return regnum;
     else
       return regnum - 32;
   }
   
   unsigned getSrrNum() const {
      assert(isSrReg() );
      return regnum - 82;
   }
   unsigned getTbrNum() const {
      assert(isTbReg() );
      return regnum - 69;
   }

   unsigned getSprgNum() const {
      assert(isSprgReg() );
      return regnum - 74;
   }
   unsigned getBatNum() const {
      assert(isBatReg() );
      return regnum - 85;
   }
   unsigned getPmcNum() const {
      assert(isPmcReg());
      return regnum - 117;
   }
   
   //Note that TBR is a funny register.  Its encoding in mftb instruction
   //is distinctly different from its encoding in mtspr instruction.
   //In the former, it can be TB (the whole register) or TBU (upper),
   //whereas in the latter (mtspr, see below), it is strictly divided into
   //TBL and TBU.
   unsigned getTbrEncoding() {
     assert(isTbReg());
     if (regnum == 69)
       return 268;
     else
       return 269;
   }
   
 


   //This is ugly, but gets the job done.  Note that SPR bits are encoded
   //with 5:9 coming before 0:4 in the actual instruction. 
   unsigned getSprEncoding() {
     switch (regnum) {
     case 64:  //LR
       return 8;
     case 65: //CTR
       return 9;
     case 66:  //XER
       return 1;
     case 73: //DSISR
       return 18;
     case 72: //DAR
       return 19;
     case 79: //DEC
       return 22;
     case 81: //SDR1
       return 25;
     case 82: //SRR0
       return 26;
     case 83: //SRR1
       return 27;
     case 74: //SPRG0
       return 272;
     case 75: //SPRG1
       return 273;
     case 76: //SPRG2
       return 274;
     case 77: //SPRG3
       return 275;
     case 71: //ASR
       return 280;
     case 84: //EAR
       return 282;
     case 69: //TBR
       return 284;
     case 70: //TBU
       return 285;
     case 80: //PVR
       return 287;
     case 85: //MMCRA
       return 786;
     case 86: //PMC1
       return 787;
     case 87: //PMC2
       return 788;
     case 88: //PMC3
       return 789;
     case 89: //PMC4
       return 790;
     case 90: //PMC5
       return 791;
     case 91: //PMC6
       return 792;
     case 92: //PMC7
       return 793;
     case 93: //PMC8
       return 794;
     case 94: //MMCR0
       return 795;
     case 95: //SIAR
       return 796;
     case 96: //SDAR
       return 797;
     case 97: //MMCR1
       return 798;
     case 98: //DABR
       return 1013;
     case 99: //PIR
       return 1023;
     case 100: //HDEC
       return 310;
     default:
       assert(false); //not an spr
     }
     
     //get rid of gcc warning
     return 0;
   }
     
static   power_reg& getRegByBit(int bit) {  
   switch (bit) {
       case 0:
	   return power_reg::gpr0;
       case 1:
	   return power_reg::gpr1;
       case 2:
	   return power_reg::gpr2;
       case 3:
	   return power_reg::gpr3;
       case 4:
	   return power_reg::gpr4;
       case 5:
	   return power_reg::gpr5;
       case 6:
	   return power_reg::gpr6;
       case 7:
	   return power_reg::gpr7;
       case 8:
	   return power_reg::gpr8;
       case 9:
	   return power_reg::gpr9;
       case 10:
	   return power_reg::gpr10;
       case 11:
	   return power_reg::gpr11;
       case 12:
	   return power_reg::gpr12;
       case 13:
	   return power_reg::gpr13;
       case 14:
	   return power_reg::gpr14;
       case 15:
	   return power_reg::gpr15;
       case 16:
	   return power_reg::gpr16;
       case 17:
	   return power_reg::gpr17;
       case 18:
	   return power_reg::gpr18;
       case 19:
	   return power_reg::gpr19;
       case 20:
	   return power_reg::gpr20;
       case 21:
	   return power_reg::gpr21;
       case 22:
	   return power_reg::gpr22;
       case 23:
	   return power_reg::gpr23;
       case 24:
	   return power_reg::gpr24;
       case 25:
	   return power_reg::gpr25;
       case 26:
	   return power_reg::gpr26;
       case 27:
	   return power_reg::gpr27;
       case 28:
	   return power_reg::gpr28;
       case 29:
	   return power_reg::gpr29;
       case 30:
	   return power_reg::gpr30;
       case 31:
	   return power_reg::gpr31;
       case 32:
	   return power_reg::fpr0;
       case 33:
	   return power_reg::fpr1;
       case 34:
	   return power_reg::fpr2;
       case 35:
	   return power_reg::fpr3;
       case 36:
	   return power_reg::fpr4;
       case 37:
	   return power_reg::fpr5;
       case 38:
	   return power_reg::fpr6;
       case 39:
	   return power_reg::fpr7;
       case 40:
	   return power_reg::fpr8;
       case 41:
	   return power_reg::fpr9;
       case 42:
	   return power_reg::fpr10;
       case 43:
	   return power_reg::fpr11;
       case 44:
	   return power_reg::fpr12;
       case 45:
	   return power_reg::fpr13;
       case 46:
	   return power_reg::fpr14;
       case 47:
	   return power_reg::fpr15;
       case 48:
	   return power_reg::fpr16;
       case 49:
	   return power_reg::fpr17;
       case 50:
	   return power_reg::fpr18;
       case 51:
	   return power_reg::fpr19;
       case 52:
	   return power_reg::fpr20;
       case 53:
	   return power_reg::fpr21;
       case 54:
	   return power_reg::fpr22;
       case 55:
	   return power_reg::fpr23;
       case 56:
	   return power_reg::fpr24;
       case 57:
	   return power_reg::fpr25;
       case 58:
	   return power_reg::fpr26;
       case 59:
	   return power_reg::fpr27;
       case 60:
	   return power_reg::fpr28;
       case 61:
	   return power_reg::fpr29;
       case 62:
	   return power_reg::fpr30;
       case 63:
	   return power_reg::fpr31;
       case 64:
	   return power_reg::lr;
       case 65: 
	   return power_reg::ctr;
       case 67: 
	   return power_reg::fpscr;
       case 69: 
	   return power_reg::tb;
       case 70: 
	   return power_reg::tbu;
       case 71: 
	   return power_reg::asr;
       case 72: 
	   return power_reg::dar;
       case 73: 
	   return power_reg::dsisr;
       case 74:
           return power_reg::sprg0; 
       case 75: 
           return power_reg::sprg1;
       case 76: 
           return power_reg::sprg2;
       case 77:
           return power_reg::sprg3;
       case 78:
	   return power_reg::msr;
       case 79: 
	   return power_reg::dec;
       case 80: 
	   return power_reg::pvr;
       case 81: 
	   return power_reg::sdr1;
       case 82:
           return power_reg::srr0; 
       case 83: 
           return power_reg::srr1;
       case 84:
	   return power_reg::ear;
       case 85:
           return power_reg::mmcra;
       case 86: 
           return power_reg::pmc1;
       case 87: 
           return power_reg::pmc2;
       case 88: 
           return power_reg::pmc3;
       case 89: 
           return power_reg::pmc4;
       case 90: 
           return power_reg::pmc5;
       case 91: 
           return power_reg::pmc6;
       case 92: 
           return power_reg::pmc7;
       case 93: 
           return power_reg::pmc8;
       case 94:
           return power_reg::mmcr0;
       case 95:
           return power_reg::siar;
       case 96:
           return power_reg::sdar;
       case 97:
           return power_reg::mmcr1;
       case 98:
           return power_reg::dabr;
       case 99:
           return power_reg::pir;
       case 100:
           return power_reg::hdec;
       case 101: 
           return power_reg::cr0;
       case 102: 
           return power_reg::cr1;
       case 103: 
           return power_reg::cr2;
       case 104: 
           return power_reg::cr3;
       case 105: 
           return power_reg::cr4;
       case 106: 
           return power_reg::cr5;
       case 107: 
           return power_reg::cr6;
       case 108: 
           return power_reg::cr7;
       case 109:
	   return power_reg::so;
       case 110:
	   return power_reg::ov;
       case 111: 
	   return power_reg::ca;
       case 112:
	   return power_reg::xerresv;
       case 113:
	   return power_reg::xerstrbytes;
       case 114: 
           return power_reg::accr;
       case 115:
           return power_reg::ctrl;
       case 116:
	   return power_reg::nullreg;
       default:
	   assert(false);
	   abort(); // placate compiler when compiling with NDEBUG
   }
   return power_reg::nullreg;
}
   
};


#endif
