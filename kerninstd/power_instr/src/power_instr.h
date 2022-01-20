// power_instr.h
// Igor Grobman
// (based on sparc_instr by Ariel Tamsches and on dyninst power parser)

#ifndef _POWER_INSTR_H_
#define _POWER_INSTR_H_

#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"
#include "uimm.h" // unsigned immediate bits
#include "simm.h" // signed immediate bits
#include "power_reg.h"
#include "power_reg_set.h"
#include "bitUtils.h"
#include "insnVec.h"
#include "common/h/String.h"
#include "instr.h"
#include "xdr_send_recv_common.h"

struct  XDR;

class power_instr : public instr_t {
 public:

   //private:
   
   /*
    * Define power instruction information.
    *
    */
   
   struct genericform {
     unsigned op : 6;
     unsigned XX : 26;
   } __attribute__((packed)) __attribute__((aligned));

   struct registergenericform { //useful for extracting registers
     unsigned op : 6;
     unsigned r1 : 5;
     unsigned r2 : 5;
     unsigned r3 : 5;
     unsigned r4 : 5;
     unsigned XX : 6; 
   } __attribute__((packed)) __attribute__((aligned));
     
     
   struct iform {            // unconditional branch + 
     unsigned op : 6;
     signed   li : 24;
     unsigned aa : 1;
     unsigned lk : 1;
     
   } __attribute__((packed)) __attribute__((aligned));
   
   struct bform {            // conditional branch + (sc is a special case)
     unsigned op : 6;
     unsigned bo : 5;
     unsigned bi : 5;
     signed   bd : 14;
     unsigned aa : 1;
     unsigned lk : 1;
   } __attribute__((packed)) __attribute__((aligned));
   
  
   struct dform {
     unsigned op : 6;
     unsigned rt : 5;        // rt, rs, frt, frs, to, bf_l
     unsigned ra : 5;
     signed   d_or_si : 16;  // d, si, ui
   } __attribute__((packed)) __attribute__((aligned));

   struct dsform {
     unsigned op : 6;
     unsigned rt : 5;        // rt, rs
     unsigned ra : 5;
     signed   ds  : 14;
     unsigned xo : 2;
   } __attribute__((packed)) __attribute__((aligned));

   struct xform {
     unsigned op : 6;
     unsigned rt : 5;   // rt, frt, bf_l, rs, frs, to, bt
     unsigned ra : 5;   // ra, fra, bfa_, sr, spr
     unsigned rb : 5;   // rb, frb, sh, nb, u_
     unsigned xo : 10;  // xo, eo
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));

   struct xlform {
     unsigned op : 6;
     unsigned bt : 5;   // rt, bo, bf_
     unsigned ba : 5;   // ba, bi, bfa_
     unsigned bb : 5; 
     unsigned xo : 10;  // xo, eo
     unsigned lk : 1;
   } __attribute__((packed)) __attribute__((aligned));

   struct xfxform {
     unsigned op : 6;
     unsigned rt : 5;   // rs
     unsigned spr: 10;  // spr, tbr, fxm
     unsigned xo : 10;
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));

   struct xflform {
     unsigned op : 6;
     unsigned u1 : 1;
     unsigned flm: 8;
     unsigned u2 : 1;
     unsigned frb: 5;
     unsigned xo : 10;
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));

   struct xsform {        //64-bit only: sradi (shift right dblwrd immediate)
     unsigned op : 6;
     unsigned rs : 5;   
     unsigned ra : 5;   
     unsigned sh1 : 5;   
     unsigned xo : 9;  
     unsigned sh2 : 1;
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));

   struct xoform {
     unsigned op : 6;
     unsigned rt : 5;
     unsigned ra : 5;
     unsigned rb : 5;
     unsigned oe : 1;
     unsigned xo : 9; // xo, eo'
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));

   struct aform {           //float opcodes
     unsigned op : 6;
     unsigned frt : 5;
     unsigned fra : 5;
     unsigned frb : 5;
     unsigned frc : 5;
     unsigned xo : 5; // xo, eo'
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));

   struct mform {
     unsigned op : 6;
     unsigned rs : 5;
     unsigned ra : 5;
     unsigned sh : 5;
     unsigned mb : 5; // mb, sh
     unsigned me : 5;
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));

   struct mdform {
     unsigned op : 6;
     unsigned rs : 5;
     unsigned ra : 5;
     unsigned sh1 : 5;
     unsigned mb_or_me : 6;
     unsigned xo : 3;
     unsigned sh2 : 1;
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));

   
   struct mdsform {
     unsigned op : 6;
     unsigned rs : 5;
     unsigned ra : 5;
     unsigned rb : 5;
     unsigned mb_or_me : 6;
     unsigned xo : 4;
     unsigned rc : 1;
   } __attribute__((packed)) __attribute__((aligned));


   union instrForms {
     struct iform  iform;  // branch;
     struct bform  bform;  // cbranch;
     struct dform  dform;
     struct dsform dsform;
     struct xform  xform;
     struct xoform xoform;
     struct aform  aform;
     struct xlform xlform;
     struct xfxform xfxform;
     struct xflform xflform;
     struct xsform xsform;
     struct mform  mform;
     struct mdform  mdform;
     struct mdsform mdsform;
     struct genericform generic;
     struct registergenericform reggeneric;
     uint32_t rawbits; 
   } __attribute__((packed)) __attribute__((aligned));


/* 
 * A bit of history.  I started coding this class before the 
 * architecture-generic interface was defined and thought having
 * an anonymous union of bit-packed structs to access instruction
 * fields was a good idea.  Now that "raw" is a member of the super
 * class, the ugly macros below are required in order to
 * get the functionality equivalent to having an anonymous union 
 * around "raw". -Igor
 */

#define iform (const_cast<instrForms *>((const instrForms *)&raw))->iform 
#define bform (const_cast<instrForms *>((const instrForms *)&raw))->bform
#define dform (const_cast<instrForms *>((const instrForms *)&raw))->dform
#define dsform (const_cast<instrForms *>((const instrForms *)&raw))->dsform
#define xform (const_cast<instrForms *>((const instrForms *)&raw))->xform
#define xoform (const_cast<instrForms *>((const instrForms *)&raw))->xoform
#define aform (const_cast<instrForms *>((const instrForms *)&raw))->aform
#define xlform (const_cast<instrForms *>((const instrForms *)&raw))->xlform
#define xfxform (const_cast<instrForms *>((const instrForms *)&raw))->xfxform
#define xflform (const_cast<instrForms *>((const instrForms *)&raw))->xflform
#define xsform (const_cast<instrForms *>((const instrForms *)&raw))->xsform
#define mform (const_cast<instrForms *>((const instrForms *)&raw))->mform
#define mdform (const_cast<instrForms *>((const instrForms *)&raw))->mdform
#define mdsform (const_cast<instrForms *>((const instrForms *)&raw))->mdsform
#define generic (const_cast<instrForms *>((const instrForms *)&raw))->generic
#define reggeneric (const_cast <instrForms *>((const instrForms *)&raw))->reggeneric


/*
 * Define the operation codes as classes 
 */

   class Trap {}; 
   class Sync {};
   class Nop {};
   
   class Mul {};
   class Div {};        
   class Abs {};
   class DifferenceOrZero {};
   class AddSub {};
   class Cmp {};
   class Neg {};
//The following is prefaced with "Logic" in order to avoid conflict with 
//predefined C++ keywords
   class LogicAnd {};
   class LogicOr {};
   class LogicXor {};
   class Nand {};
   class Nor {};
   class Eqv {};
   class MaskGen {};
   class MaskInsertFromReg {};

   class Exts {};
   class Cntlz {};
   
 
   class Load {};
   class Store {};

   class Shift {};
   class Rotate {};
   
   //special register moves
   class MoveToSPR {};
   class MoveFromSPR {};
   class MoveToCondRegFlds {};
   class MoveToCondRegFromXER {};
   class MoveFromCondReg{};
   class MoveToMachineStateReg {};
   class MoveFromMachineStateReg {};
   //   class SegmentRegMove {};

  
   

   //Branches
   class Branch {};
   class BranchCond {};
   class BranchCondLinkReg {};
   class BranchCondCountReg {};
   class Sc {};

   class CondReg {};  //condition register modifications
   class CondRegField {};


   //Floating point
   class FPLoad {};
   class FPStore {};
   class FPMove {};
   class FPArithmetic {};
   class FPConversion {};  
   class FPCompareUnordered {};
   class FPCompareOrdered {};
   class MoveFromFPSCR {};
   class MoveToCondRegFromFPSCR{};
   class MoveToFPSCRFieldImm {};
   class MoveToFPSCRFields {};
   class MoveToFPSCRBit0 {};
   class MoveToFPSCRBit1 {};

   class Rfi {};
   
  
   //Segments are only in 32-bit implementations
   //class SLBInvalidateEntry {};
   //class SLBInvalidateAll {};
  
   //TLB ops
   class TLBInvalidateEntry {};
   class TLBInvalidateAll {};
   class TLBSync {};

   //Cache Management
   class ICacheBlockInvalidate {};
   class InstructionSync {};
   class DCacheBlockInvalidate {};
   class DCacheBlockTouch {};
   class DCacheBlockTouchStore {};
   class DCacheBlockSetZero {};
   class DCacheBlockStore {};
   class DCacheBlockFlush {};
   class CacheLineComputeSize {};
   class CacheLineFlush {};
   class CacheLineInvalidate {};

   class EnforceInOrderExecutionIO {};
   class MoveFromTimeBase {};

  
   
 public:

  

   enum ArgSize { word, doubleWord };
   enum MulHiLo {high, low};  //high or low part of the result
   enum ExtsOps { extSB=954, extSH=922, extSW=986};
   enum CondCode { lessThan=0, greaterThan=1, equal=2, summaryOverflow=3,
		   greaterThanEqual, lessThanEqual, notEqual, 
		   notSummaryOverflow, always };
   

   //For all of the following enumerated types, the values 
   //correspond to the opcode or XO fields of instruction
   //(whichever differs).  This makes instruction generation
   //convenient and easy. Instruction groups that come
   // in multiple forms (e.g. D and X) are split accordingly,
   //with the form being a suffix on the name.  This is to 
   //differentiate between instructions with the same name,
   //e.g ldByteZeroD vs. ldByteZeroX and prevent stupid typos.

   //Add and Subtract ops come in either D(immediate) or XO
   //(register) form. XO-form has the ever-popular opcode=31,
   //and differing XOs, whereas D-form instructions have differing
   //opcodes.
   enum AddSubOpsD { addD=14, addShiftedD=15, addCarryD=12, 
		     addCarryRecordD=13, subCarryD=8 };
   
   enum AddSubOpsXO { addXO=266, subXO=40, addCarryXO=10, subCarryXO=8, 
		      addExtXO=138, subExtXO=136, addMinusOneExtXO=234, 
		      subMinusOneExtXO=232, addZeroExtXO=202, 
		      subZeroExtXO=200 };



   //Load and store operations come in two forms: X (register-based)
   //and D(immediate).  X-form has same opcode(31), and differing XOs,
   // whereas D-form instructions differ in their opcode.
   //There is also a couple DS form instructions (sligthly shorter immediate)
   //with last 2 bits acting as XO 
   enum LoadOpsD { ldByteZeroD=34, ldByteZeroUpdateD=35, ldHalfZeroD=40,  
		   ldHalfZeroUpdateD=41, ldHalfAlgebraicD=42, 
		   ldHalfAlgebraicUpdateD=43, ldWordZeroD=32,
		   ldWordZeroUpdateD=33, ldMultipleWordD=46 };

   enum LoadOpsDS { ldWordAlgebraicDS=2,ldDoubleDS=0, ldDoubleUpdateDS=1 };

   enum LoadOpsX { ldByteZeroX=87,  ldByteZeroUpdateX=119, ldHalfZeroX=279,  
		   ldHalfZeroUpdateX=311, ldHalfAlgebraicX=343, 
		   ldHalfAlgebraicUpdateX=375, ldWordZeroX=23,

		   ldWordZeroUpdateX=55, ldWordAlgebraicX=341,
		   ldWordAlgebraicUpdateX=373, ldDoubleX=21,
		   ldDoubleUpdateX=53, ldHalfReverseX=790,
		   ldWordReverseX=534, ldStringWordImmX=597,
		   ldStringWordX=533, ldWordReserveX=20, 
		   ldDoubleReserveX=84, ldStringCompareByteX=277 };


   enum StoreOpsD { stByteD=38, stByteUpdateD=39, stHalfD=44, 
		    stHalfUpdateD=39, stWordD=36, stWordUpdateD=37, 
		    stMultipleWordD=47 };

   enum StoreOpsDS { stDoubleDS=0, stDoubleUpdateDS=1 };


  

   enum StoreOpsX { stByteX=215,  stByteUpdateX=247, stHalfX=407,
		    stHalfUpdateX=439,  stWordX=151,  stWordUpdateX=183, 
		    stDoubleX=149, stDoubleUpdateX=181, stHalfReverseX=918,
		    stWordReverseX=662, stStringWordImmX=725, stStringWordX=661,
		    stWordCondX=150, stDoubleCondX=214 };


   //Floating Load and store operations come in two forms: X (register-based)
   //and D(immediate).  X-form has same opcode(31), and differing XOs,
   // whereas D-form instructions differ in their opcode.
   enum FPLoadOpsD { fpldSingleD=48, fpldSingleUpdateD=49,
		     fpldDoubleD=50, fpldDoubleUpdateD=51
   };

   enum FPLoadOpsX { fpldSingleX=535, fpldSingleUpdateX=567,
		     fpldDoubleX=599, fpldDoubleUpdateX=631   
   };

  
   enum FPStoreOpsD { fpstSingleD=52, fpstSingleUpdateD=53,
		      fpstDoubleD=54, fpstDoubleUpdateD=55
   };
 
   enum FPStoreOpsX { fpstSingleX=663, fpstSingleUpdateX=695,
		     fpstDoubleX=727, fpstDoubleUpdateX=759   
   };
   
  


   //Rotate ops come in M, MD, or MDS form.  Their identifying bits of
   //opcode are thus located in different places (XO, OPCODE, etc.)
//    enum RotateOps { rotateLDoubleImmClearL=0,  rotateLDoubleImmClearR=1,
// 		    rotateLDoubleImmClear=2, rotateLWordImmANDWithMask=21,
// 		    rotateLDoubleClearL=8, rotateLDoubleClearR=9,
// 		    rotateLWordANDWithMask=23, rotateLDoubleImmMaskInsert=3,
// 		    rotateLWordImmMaskInsert=20 };
   
   enum RotateOpsM {  rotateLeftWordImmANDWithMaskM=21, 
		      rotateLeftWordANDWithMaskM=23,
		      rotateLeftWordImmMaskInsertM=20 };
   
   enum RotateOpsMD { rotateLeftDoubleImmClearLeftMD=0, 
		      rotateLeftDoubleImmClearRightMD=1, 
		      rotateLeftDoubleImmClearMD=2,
		      rotateLDoubleImmMaskInsertMD=3 };

   enum RotateOpsMDS {  rotateLeftDoubleClearLeftMDS=8,
			rotateLeftDoubleClearRightMDS=9 };

   //All shift instructions are X-form with opcode=31
   //sradi is an exception, and has XS form (last bit of XO used as sh-bit).
   //Both sradi and srawi (also uses immediate sh field) are not included 
   //here and handled separately.
   enum ShiftOps { shiftLDouble=27, shiftLWord=24, shiftRDouble=539,
		   shiftRWord=536, ShiftRAlgebraicDouble=794,
		   shiftRightAlgebraicWord=792 };


   //All Condition register instructions are XL-form with Opcode=19
   enum CondRegOps { CRAnd=257, CROr=449, CRXor=193, CRNand=225, 
		     CRNor=33, CREqv=289, CRAndComplement=129,
		     CROrComplement=417 };

   //All floating-point move instructions are X-form with opcode=63
   enum FPMoveOps { FPMoveop=72, FPNegate=40, FPAbs=264, FPNegAbs=136 };

   //Floating-point arithmetic instructions are A-form with opcode=63
   //for operations on doubles and opcode=59 for single-precision.
   //XOs are the same for equivalent (double vs. single) ops.
   enum FPArithmeticOps { FPAdd=21, FPSub=20, FPMul=25, FPDiv=18,
                          FPMulAdd=29, FPMulSub=28, FPNegMulAdd=31, 
                          FPNegMulSub=30 };
   enum FPPrecision { singlePrecision, doublePrecision };


   //Floating-point rounding and conversion instructions are
   //X-form with opcode=63
   enum FPConversionOps { FPRoundSingle=12, FPConvFromIntDouble=814,
			  FPConvIntDoubleRoundZero=815, FPConvIntWord=14,
			  FPConvIntWordRoundZero=15, FPConvToIntDouble=846 };   
   
   //NOTE: segment registers are 32-bit implementation only.
   //Segment Register moves are X-form, opcode=31
   //   enum SegmentRegMoveOps { MoveToSegmentReg=210, MoveToSegmentRegIndirect=242,
   //		      MoveFromSegmentReg=595, 
   //		    MoveFromSegmentRegIndirect=659};
 
   struct power_cfi: public controlFlowInfo {
      // note: we need a ctor for this structure since the_register has 
      // no default ctor
      // 1) What is being tested: analyze BO field
      union {
         unsigned power_bit_stuff;
         struct {
            unsigned decrementCTR:1;
            unsigned CTRzero:1;
            unsigned condTrue:1;  //jump on bit == 1 ?
            unsigned predict:1; //the y prediction bit
            unsigned link:1;   // are we linking?
            unsigned isBug:1; //is this a fake instruction produced by BUG() macro
         } power_fields;
      };
      
      power_cfi();
   
      
      
      // 2) The condition that the above is being tested against, i.e. which 
      // bit of the CR are we testing
      //unsigned char condRegBitNum; //handled by the superclass' condition var
      

      // 3) Destination information (i.e. AA bit) (all of below is actually
      // in the superclass)
      // enum desttype {pcrelative, pcabsolute, registerrelative, 
      //                  registerabsolute};
      
      //  desttype destination;
      //power_reg destreg; 
      //int offset_nbytes;
      // offset from pc if destination==pcrelative or absolute address
      //if destination == absolute
      
   };
 private:
     
   void set_iform(uimmediate<6> opcd, simmediate<24> li, 
		  uimmediate<1> aa, uimmediate<1> lk);
   void set_bform(uimmediate<6> opcd, uimmediate<5> bo, 
		  uimmediate<5> bi, simmediate<14> bd,
		  uimmediate<1> aa, uimmediate<1> lk);
   void set_dform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
		  uimmediate<16> d);
    void set_dform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
		   simmediate<16> si);
   void set_dform(uimmediate<6> opcd, uimmediate<5> to, power_reg ra, 
		  simmediate<16> si);
   void set_dform(uimmediate<6> opcd, uimmediate<3> bf, uimmediate<1> l, 
		  power_reg ra, uimmediate<16> d);
   void set_dform(uimmediate<6> opcd, uimmediate<3> bf, uimmediate<1> l, 
		  power_reg ra, simmediate<16> si);
   void set_dsform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
		  simmediate<14> ds, uimmediate<2> xo );
   void set_xform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
		  power_reg rb, uimmediate<10> xo, uimmediate<1> rc);
   //segment registers are only used in 32-bit mode -Igor
   //  void set_xform(uimmediate<6> opcd, power_reg rt, power_reg sr, 
   //	  uimmediate<10> xo, uimmediate<1> rc);
   void set_xform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
		  uimmediate<5> nb_or_sh, uimmediate<10> xo, uimmediate<1> rc);
   void set_xform(uimmediate<6> opcd, uimmediate<3> bf, uimmediate<1> l, 
		  power_reg ra, power_reg rb, uimmediate<10> xo, 
		  uimmediate<1> rc);
   void set_xform(uimmediate<6> opcd, uimmediate<3> bf, 
		  uimmediate<3> bfa, uimmediate<10> xo, uimmediate<1> rc);
   void set_xform(uimmediate<6> opcd, uimmediate<3> bf, uimmediate<4> u, 
		  uimmediate<10> xo, uimmediate<1> rc);
   void set_xform(uimmediate<6> opcd, uimmediate<5> to, power_reg ra, 
		  power_reg rb, uimmediate<10> xo, uimmediate<1> rc);
   void set_xlform(uimmediate<6> opcd, uimmediate<5> bt_or_bo, 
		   uimmediate<5> ba_or_bi, uimmediate<5> bb, 
		   uimmediate<10> xo, uimmediate<1> lk);
   void set_xlform(uimmediate<6> opcd, uimmediate<3> bf, 
		   uimmediate<3> bfa, uimmediate<10> xo);
   void set_xfxform(bool tbr, uimmediate<6> opcd, power_reg rt, power_reg spr, 
		uimmediate<10> xo);
   void set_xfxform(uimmediate<6> opcd, power_reg rt, uimmediate<8> fxm, 
		     uimmediate<10> xo);
   void set_xflform(uimmediate<6> opcd, uimmediate<8> flm, power_reg frb, 
		    uimmediate<10> xo, uimmediate<1> rc);
   void set_xsform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
		   uimmediate<6> sh, uimmediate<9> xo, 
		   uimmediate<1> rc);
   void set_xoform(uimmediate<6> opcd, power_reg rt, power_reg ra, 
		   power_reg rb, uimmediate<1> oe,  uimmediate<9> xo, 
		   uimmediate<1> rc);
   void set_aform(uimmediate<6> opcd, power_reg frt, power_reg fra, 
		  power_reg frb, power_reg frc, uimmediate<5> xo, 
		  uimmediate<1> rc);
   void set_mform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
		  power_reg rb, uimmediate<5> mb, uimmediate<5> me,
		  uimmediate<1> rc);
   void set_mform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
		  uimmediate<5> sh, uimmediate<5> mb, uimmediate<5> me,
		  uimmediate<1> rc);
   void set_mdform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
		  uimmediate<6> sh, uimmediate<6> mb_or_me, uimmediate<3> xo,
		   uimmediate<1> rc);
   void set_mdsform(uimmediate<6> opcd, power_reg rs, power_reg ra, 
		   power_reg rb, uimmediate<6> mb_or_me, uimmediate<4> xo,
		    uimmediate<1> rc);

  

   pdstring disass_simm(int simm) const; // any # of bits
   pdstring disass_uimm(unsigned uimm) const; // any # of bits

   pdstring disass_li24() const { return disass_simm(getLi24()); }
   pdstring disass_bd14() const { return disass_simm(getBd14()); }


   //power manuals have counterintuitive bit numbering scheme:
   //MSB is numbered 0.  All of the parsing code is also 
   //written with this assumption in mind.  This is where it's "fixed"
   uint32_t getBits(unsigned lobit, unsigned hibit) const {
      return ::getBits(this->raw, 31-hibit,31-lobit );
   }
   
   uint32_t getBits0(unsigned hibit) const {
      // same as prev routine except lobit assumed 0
      return ::getBitsFrom0(this->raw, hibit);
   }

   bool getPredictBit() const { return getBits(10,10); }
   unsigned getBFField() const { return getBits(6,8); }
   unsigned getBFAField() const { return getBits(11,13); }
   unsigned getLField() const { return getBits(10,10); }
   unsigned getUField() const { return getBits(16, 19); }
   unsigned  reverse5BitHalves (unsigned int orig) const;
   

   template <unsigned numbits>
   void replaceBits(unsigned loBit, 
		    const uimmediate<numbits> &replacementValue) {
      this->raw = ::replaceBits(this->raw, loBit, replacementValue);
         // uimm.h
   }
   template <unsigned numbits>
   void replaceBits(unsigned loBit, 
		    const simmediate<numbits> &replacementValue) {
      this->raw = ::replaceBits(this->raw, loBit, replacementValue);
         // simm.h
   }
   

   int32_t getBitsSignExtend(unsigned lobit, unsigned hibit) const;
  

   int getSimm7() const { return getBitsSignExtend(0, 6); }
   int getSimm10() const { return getBitsSignExtend(0, 9); }
   int getSimm11() const { return getBitsSignExtend(0, 10); }
   int getSimm13() const { return getBitsSignExtend(0, 12); }
   
   unsigned  getUimm11() const { return getBits0(10); }
   unsigned  getUimm13() const { return getUimm13Bits(raw); }
   
   int getLi24() const { return getBitsSignExtend(6,29)<<2; }
   //Note the shift left.  This is because 0b00 gets appended to LI.
   int getBd14() const { return getBitsSignExtend(16,29)<<2; }
   //Note the shift left.  This is because 0b00 gets appended to BD.
   


   bool getBit(unsigned bit) const {
      const unsigned temp = raw >> bit;
      return ((temp & 0x1) != 0);
   }

   /* 
   //May need to have similar optimization for Power...
   unsigned getOp3() const {
      return getOp3Bits(raw);
   }
   unsigned getHi2Bits() const {
      return (raw >> 30) & 0x3;
      } 
   */

   

   //On power, registers are encoded in 5-bit blocks immediately following
   //the opcd field.  The number of registers varies from 0-4, depending on
   //the instruction.  These are generic functions that convert bits into
   //registers.  Here, reg1 is the first register after opcd, reg2 is the
   //second etc.  Their name (rs, rt, ...) depends on context.
   
   power_reg_set getReg1AsInt() const {
     return power_reg_set(power_reg_set::singleIntReg, reggeneric.r1);
   }
   
   power_reg_set getReg2AsInt() const {
     return power_reg_set(power_reg_set::singleIntReg, reggeneric.r2);
   }

   power_reg_set getReg3AsInt() const {
     return power_reg_set(power_reg_set::singleIntReg, reggeneric.r3);
   }
   
   power_reg_set getReg4AsInt() const {
     return power_reg_set(power_reg_set::singleIntReg, reggeneric.r4);
   }
   
   power_reg_set getReg1AsFloat() const {
     return power_reg_set(power_reg_set::singleFloatReg, reggeneric.r1);
   }

   power_reg_set getReg2AsFloat() const {
     return power_reg_set(power_reg_set::singleFloatReg, reggeneric.r2);
   }
   
    power_reg_set getReg3AsFloat() const {
     return power_reg_set(power_reg_set::singleFloatReg, reggeneric.r3);
   }

   power_reg_set getReg4AsFloat() const {
     return power_reg_set(power_reg_set::singleFloatReg, reggeneric.r4);
   }
   
    //unconditional branch
   void getInformation_iform(registerUsage *, memUsage *, 
                             disassemblyFlags *,  
                             power_cfi *, 
                             relocateInfo *) const; 
   //conditional branch
   void getInformation_bform(registerUsage *, memUsage *, disassemblyFlags *, 
			     power_cfi *, relocateInfo *) const;        
   void getInformation_dform(registerUsage *, memUsage *, disassemblyFlags *, 
                         power_cfi *, relocateInfo *) const;
   void getInformation_dsform(registerUsage *, memUsage *, disassemblyFlags *,
                         power_cfi *, relocateInfo *) const;
   void getInformation_xform31(registerUsage *, memUsage *, disassemblyFlags *,
			 power_cfi *, relocateInfo *) const;
   void getInformation_xform31n(registerUsage *, memUsage *, disassemblyFlags *,
			 power_cfi *, relocateInfo *) const;
   
   void processIntegerLoads(registerUsage *ru, memUsage *mu, 
                            disassemblyFlags *disassFlags, unsigned xop )  const;
   void processIntegerStores(registerUsage *ru, memUsage *mu, 
                             disassemblyFlags *disassFlags, unsigned xop ) const;
   void processLoadStoreStringWordImmediate(registerUsage *ru, memUsage *mu, 
                                            disassemblyFlags *disassFlags, unsigned xop ) const;
   void processLoadStoreStringWordIndexed(registerUsage *ru, memUsage *mu, 
                                          disassemblyFlags *disassFlags, unsigned xop ) const;
   void processLoadStoreFloatingQuad(registerUsage *ru, memUsage *mu, 
                                     disassemblyFlags *disassFlags, unsigned xop ) const;
   void processCompare (registerUsage *ru, 
                        disassemblyFlags *disassFlags, unsigned xop ) const;

   void getInformation_xform63(registerUsage *, memUsage *, disassemblyFlags *,
			 power_cfi *, relocateInfo *) const;
   void getInformation_xlform(registerUsage *, memUsage *, disassemblyFlags *,
			  power_cfi *, relocateInfo *) const;
   void getInformation_xoform(registerUsage *, memUsage *, disassemblyFlags *,
                         power_cfi *, relocateInfo *) const;
   void getInformation_aform(registerUsage *, memUsage *, disassemblyFlags *,
                         power_cfi *, relocateInfo *) const;
   void getInformation_mform(registerUsage *, memUsage *, disassemblyFlags *,
                             power_cfi *, relocateInfo *) const;
   void getInformation_mdform(registerUsage *, memUsage *, disassemblyFlags *,
                         power_cfi *, relocateInfo *) const;
   void getInformation_mdsform(registerUsage *, memUsage *, disassemblyFlags *,
                         power_cfi *, relocateInfo *) const;
   void processTrap (registerUsage *ru, 
                     disassemblyFlags *disassFlags, unsigned xop )  const ;
   void processLogicAndMiscArithmetic(registerUsage *ru, 
                                       disassemblyFlags *disassFlags, unsigned xop )  const;
   void processSPRMove(registerUsage *ru, 
                       disassemblyFlags *disassFlags, unsigned xop )  const;
   void processCRMove (registerUsage *ru, 
                       disassemblyFlags *disassFlags, unsigned xop )  const;
   void processXERToCRMove (registerUsage *ru, 
                            disassemblyFlags *disassFlags )   const;
   void processFloatingLoads(registerUsage *ru, memUsage *mu, 
                             disassemblyFlags *disassFlags, unsigned xop )  const;
   void processFloatingStores (registerUsage *ru, memUsage *mu, 
                               disassemblyFlags *disassFlags, unsigned xop )  const;
   void processCacheInstr(registerUsage *ru, 
                          disassemblyFlags *disassFlags, unsigned xop )   const;
   void processMSRMove(registerUsage *ru, 
                       disassemblyFlags *disassFlags, unsigned xop )  const;
   void processLBInstr (registerUsage *ru, 
                        disassemblyFlags *disassFlags, unsigned xop )  const;
   void processSLBMove (registerUsage *ru, 
			disassemblyFlags *disassFlags, unsigned xop )  const; 
   void processTBMove(registerUsage *ru, 
                      disassemblyFlags *disassFlags )  const;
   void  processCacheLineInstr (registerUsage *ru, 
                                disassemblyFlags *disassFlags, unsigned xop ) const;
   void processSegRegMove(registerUsage *ru, 
                          disassemblyFlags *disassFlags, unsigned xop )  const;
   
   pdstring disass_prediction_bit(bool bit, bool reg_or_nonneg) const;
   //default predict bit logic is as follows: 
   //for branches with immediate offset, it's predicted taken if offset < 0
   //for branches to a register (LR or CTR), branches are predicted not taken
   //Setting the bit on reverses the above assumptions.  
   //
   //The above function produces a "+" if the predict bit is on AND the 
   //reversed assumptions would lead to the branch being taken.
   //It produces a "-" if the predict bit is on AND the branch is predicted
   //not taken by the reversed assumptions.
   //No output is produces if prediction bit is off.

   //produce cr# if bi > 3 (cr0 is assumed)
   pdstring disass_crfield (unsigned bi) const;

   //This function does all the disassembly common to all conditional branches
   //(i.e. by far the majority of the disassembly and analysis)
   bool getInformation_condbranch (registerUsage *ru,
				 disassemblyFlags *disassFlags, 
				 power_cfi *cfi,
				 pdstring suffix //"" (b-form) or "lr" or "ctr"
				 ) const;
   
   void getRegisterUsage_CRLogical ( registerUsage *ru,
                                unsigned bt, unsigned ba, unsigned bb) const;

   void disass_dform_loads_and_stores (disassemblyFlags *disassFlags,
                                       const unsigned op, 
                                       power_reg_set rt,
                                       power_reg_set ra) const;
   void disass_xform_loads_and_stores (disassemblyFlags *disassFlags,
                                       const unsigned xop) const;
   void disass_dform_arithmetic (disassemblyFlags *disassFlags,
                                 const unsigned op, 
                                 power_reg_set rt,
                                 power_reg_set ra) const;
   pdstring disass_to_field (const unsigned to) const;
   void disass_dform_logical (disassemblyFlags *disassFlags,
                              const unsigned op, 
                              const unsigned ui,
                              power_reg_set ra,
                              power_reg_set rt) const;


   pdstring getMnemonic31(const unsigned xop) const;
   pdstring getMnemonic63(const unsigned xop) const;

   static bool existsTrueDependence(const registerUsage &ru,
                                    const registerUsage &nextRu,
                                    const memUsage &mu,
                                    const memUsage &nextMu);
   static bool existsAntiDependence(const registerUsage &ru,
                                    const registerUsage &nextRu,
                                    const memUsage &mu,
                                    const memUsage &nextMu);
   static bool existsOutputDependence(const registerUsage &ru,
                                      const registerUsage &nextRu,
                                      const memUsage &mu,
                                      const memUsage &nextMu);

   static bool inRangeOf(kptr_t from, kptr_t to,
                         unsigned numBitsOfSignedDisplacement);

  
   
   void changeBranchOffsetIform(const power_cfi &oldcfi,
                                       int new_insnbytes_offset);
   void changeBranchOffsetBform(const power_cfi &oldcfi,
                                       int new_insnbytes_offset);
  
 public:
   //these two are used for identifying the instruction to overwrite in
   //binary patching bad ea bits check in 2.4 kernel (see gatherMod-linux.C)
   unsigned getShFieldMd() const { 
      unsigned sh1 = getBits(16,20); 
      unsigned sh2 = getBits(30, 30);
      return ( (sh1) | (sh2<<5) );
   }
   unsigned getMeFieldMd() const { 
      unsigned origme = getBits(21,26); 
      return ( (origme>>1 ) | ( (origme & 0x1) << 5 ) );
   } 
   bool isRotateLeftDoubleWordImmediateClearRight() const {
      return ( (mdform.op == 30) && (mdform.xo == 1) ); 
   }

   bool send(XDR *xdr) const {
      return P_xdr_send(xdr, raw);
   }

   // The following static member vrbles are meant to be passed as the first 
   //param to the constructors of this class...making them each of a different 
   // type (instead of an enumerated type) really beefs up type checking nicely.
  
  
   static Trap trap; 
   static Sync sync;
   static Nop nop;
  
   static Mul mul;
   static Abs abs;
   static DifferenceOrZero differenceorzero;
   static Div div;   
   static AddSub addsub;
   static Cmp cmp;
   static Neg neg;
   static LogicAnd logicand;
   static LogicOr logicor;
   static LogicXor logicxor;
   static Nand nand;
   static Nor nor;
   static Eqv eqv;
   static MaskGen maskgen;
   static MaskInsertFromReg maskinsertfromreg;

   static Exts exts;
   static Cntlz cntlz;
   
 
   static Load load;
   static Store store;

   static Shift shift;
   static Rotate rotate;
   
   //special register moves
   static MoveToSPR movetospr;
   static MoveFromSPR movefromspr;
   static MoveToCondRegFlds movetocondregflds;
   static MoveToCondRegFromXER movetocondregfromxer;
   static MoveFromCondReg movefromcondreg;
   static MoveToMachineStateReg movetomachinestatereg;
   static MoveFromMachineStateReg movefrommachinestatereg;
   //  static SegmentRegMove segmentregmove;

   //Branches
   static Branch branch;
   static BranchCond branchcond;
   static BranchCondLinkReg branchcondlinkreg;
   static BranchCondCountReg branchcondcountreg;
   static Sc sc;

   static CondReg condreg;  //condition register modifications
   static CondRegField condregfield;

   //Floating point
   static FPLoad fpload;
   static FPStore fpstore;
   static FPMove fpmove;
   static FPArithmetic fparithmetic;
   static FPConversion fpconversion;  
   static FPCompareUnordered fpcompareunordered;
   static FPCompareOrdered fpcompareordered;
   static MoveFromFPSCR movefromfpscr;
   static MoveToCondRegFromFPSCR movetocondregfromfpscr;
   static MoveToFPSCRFieldImm movetofpscrfieldimm;
   static MoveToFPSCRFields movetofpscrfields;
   static MoveToFPSCRBit0 movetofpscrbit0;
   static MoveToFPSCRBit1 movetofpscrbit1;

   static Rfi rfi;
   
   
   //NOTE: segments are in 32-bit implementation only
   //  static SLBInvalidateEntry slbinvalidateentry;
   //static SLBInvalidateAll slbinvalidateall;

   //TLB ops
   static TLBInvalidateEntry tlbinvalidateentry;
   static TLBInvalidateAll tlbinvalidateall;
   static TLBSync tlbsync;

   //Cache Management
   static ICacheBlockInvalidate icacheblockinvalidate;
   static InstructionSync instructionsync;
   static DCacheBlockInvalidate dcacheblockinvalidate;
   static DCacheBlockTouch dcacheblocktouch;
   static DCacheBlockTouchStore dcacheblocktouchstore;
   static DCacheBlockSetZero dcacheblocksetzero;
   static DCacheBlockStore dcacheblockstore;
   static DCacheBlockFlush dcacheblockflush;
   static CacheLineComputeSize cachelinecomputesize;
   static CacheLineFlush cachelineflush;
   static CacheLineInvalidate cachelineinvalidate;

   static EnforceInOrderExecutionIO enforceinorderexecutionio;
   static MoveFromTimeBase movefromtimebase;

   power_instr(const power_instr &src) : instr_t(src) {}
   power_instr(uint32_t iraw): instr_t(iraw)  {}

   power_instr(XDR *xdr) : instr_t((unsigned)0) {
      if(!P_xdr_recv(xdr, raw))
	 assert(0 && "recv of power raw bits failed");
   }

   power_instr &operator=(const power_instr &src) {raw=src.raw; return *this;}
   
   bool operator==(const power_instr &other) const {
      return raw == other.raw;
   }

  
   bool isBranch() const; 
   bool isCall() const;
   bool isUnanalyzableCall() const;
   bool isCmp() const;
   bool isNop() const;
   bool isRestore() const { 
      assert(false);
      return false; 
   }
   bool isSave() const { 
      assert(false);
      return false; 
   }
   bool isLoad() const;
   bool isStore() const;
   
   bool isIntegerLoadWithOffset(long *offset) const;
   //offset filled in
   bool isRotateLeftWordImmediate() const;

   static bool instructionHasValidAlignment(kptr_t addr) {
      // used for assertions.  The x86 version would just always return true.
      return addr % 4 == 0;
   }
   
   

   static bool hasUniformInstructionLength() {
      // true for SPARC and power, not so for x86 of course.
      return true;
   }

   static bool condBranchesUseDelaySlot() {
      //no delay slots on power
      return false;
   }


   delaySlotInfo getDelaySlotInfo() const {
      return dsNone;
   }

   // ----------------------------------------------------------------
   // Making instructions
   // ----------------------------------------------------------------
   // 
   // Note the style of the arguments: they are meant to read like
   // an assembly language printout (well, not exactly of course, but you
   // get the idea). 
   // ---------------------------------------------------------------


   power_instr(Trap, ArgSize, uimmediate<5> to, power_reg ra, simmediate<16> si);
   power_instr(Trap, ArgSize, uimmediate<5> to, power_reg ra, power_reg rb);
   
   power_instr(Sync);
   power_instr(Nop);
   
   power_instr(Mul, MulHiLo, power_reg rt, 
	       power_reg ra, simmediate<16> si);
   power_instr(Mul, ArgSize, MulHiLo, bool isSigned, power_reg rt, 
	       power_reg ra, power_reg rb, 
	       uimmediate<1> oe, uimmediate<1> rc);
   power_instr(Abs, power_reg rt, power_reg ra, 
               uimmediate<1> oe, uimmediate<1> rc);
   power_instr(DifferenceOrZero,  power_reg rt, power_reg ra, power_reg rb,
	      uimmediate<1> oe,  uimmediate<1> rc);
   power_instr(DifferenceOrZero,  power_reg rt, power_reg ra, 
               simmediate<16> si);
   
   power_instr(Div, ArgSize, bool isSigned,  power_reg rt, power_reg ra, 
	       power_reg rb, uimmediate<1> oe, uimmediate<1> rc);

   power_instr(AddSub, AddSubOpsD, power_reg rt, 
	       power_reg ra, simmediate<16> si);
   power_instr(AddSub, AddSubOpsXO, power_reg rt,
	       power_reg ra, power_reg rb, uimmediate<1> oe, uimmediate<1> rc);
  
   power_instr(Neg, power_reg rt, power_reg ra,  uimmediate<1> oe, 
	       uimmediate<1> rc); 
   
   power_instr(Cmp, bool logical, uimmediate<3> bf, uimmediate<1> l, 
	       power_reg ra, power_reg rb);
   power_instr(Cmp, bool logical, uimmediate<3> bf, uimmediate<1> l, 
	       power_reg ra, simmediate<16> si );
          
   power_instr(LogicAnd, bool shifted, power_reg ra, power_reg rs, uimmediate<16> ui);
   power_instr(LogicOr, bool shifted, power_reg ra, power_reg rs, uimmediate<16> ui);
   power_instr(LogicXor, bool shifted, power_reg ra, power_reg rs, uimmediate<16> ui);
   power_instr(LogicAnd, bool complement,  power_reg ra, power_reg rs, power_reg rb,
	       uimmediate<1> rc);
   power_instr(LogicOr, bool complement,  power_reg ra, power_reg rs, power_reg rb,
		uimmediate<1> rc);
   power_instr(LogicXor,  power_reg ra, power_reg rs, power_reg rb,
	       uimmediate<1> rc);
   power_instr(Nand,  power_reg ra, power_reg rs, power_reg rb,
	       uimmediate<1> rc);
   power_instr(Nor,  power_reg ra, power_reg rs, power_reg rb,
	       uimmediate<1> rc);
   power_instr(Eqv,  power_reg ra, power_reg rs, power_reg rb,
	       uimmediate<1> rc);
   power_instr(MaskGen,  power_reg ra, power_reg rs, power_reg rb,
	       uimmediate<1> rc);
   power_instr(MaskInsertFromReg,  power_reg ra, power_reg rs, power_reg rb,
	       uimmediate<1> rc);

   power_instr(Exts, ExtsOps, power_reg ra, power_reg rs, uimmediate<1> rc);
   power_instr(Cntlz, ArgSize, power_reg ra, power_reg rs, uimmediate<1> rc);


   power_instr(Load, LoadOpsD, power_reg rt, power_reg ra, simmediate<16> d);
   power_instr(Load, LoadOpsDS, power_reg rt, power_reg ra, signed d);
   power_instr(Load, LoadOpsX, power_reg rt, power_reg ra, power_reg rb);
   power_instr(Store, StoreOpsD, power_reg rt, power_reg ra, simmediate<16> d);
   power_instr(Store, StoreOpsDS, power_reg rt, power_reg ra, signed d);
   power_instr(Store, StoreOpsX, power_reg rt, power_reg ra, power_reg rb);

   power_instr(Shift, ShiftOps, power_reg rs, power_reg ra, power_reg rb, 
	       uimmediate<1> rc);
   power_instr(Shift, power_reg rs, power_reg ra, uimmediate<6> sh, 
	       uimmediate<1> rc);
   power_instr(Shift, power_reg rs, power_reg ra, uimmediate<5> sh, 
		 uimmediate<1> rc);
   
   power_instr(Rotate, RotateOpsMD, power_reg ra, power_reg rs, 
	       uimmediate<6> sh, uimmediate<6> mb_or_me, uimmediate<1> rc);
   power_instr(Rotate, RotateOpsM, power_reg ra, power_reg rs, 
	       uimmediate<5> sh, uimmediate<5> mb, uimmediate<5> me, 
	       uimmediate<1> rc);
   power_instr(Rotate, RotateOpsM op, power_reg ra, power_reg rs, 
	       power_reg rb, uimmediate<5> mb, uimmediate<5> me, 
	       uimmediate<1> rc);
   power_instr(Rotate, RotateOpsMDS, power_reg ra, power_reg rs, power_reg rb, 
	       uimmediate<6> mb_or_me, uimmediate<1> rc);

   power_instr(MoveToSPR, power_reg spr, power_reg rs);
   power_instr(MoveFromSPR, power_reg spr, power_reg rt);
   power_instr(MoveToCondRegFlds, uimmediate<8> fxm, power_reg rs);
   power_instr(MoveToCondRegFromXER, uimmediate<3> bf);
   power_instr(MoveFromCondReg, power_reg rt);
   power_instr(MoveToMachineStateReg, power_reg rs);
   power_instr(MoveFromMachineStateReg, power_reg rt);
   //  power_instr(SegmentRegMove, SegmentRegMoveOps, power_reg sr_or_rb, 
   //       power_reg rs_or_rt);
   
   void convertCondCode(CondCode cond, unsigned crfield, uimmediate<5> &bo, 
			uimmediate<5> &bi);
       
   power_instr(Branch, signed li, uimmediate<1> aa, uimmediate<1> lk);
   power_instr(BranchCond, uimmediate<5> bo, uimmediate<5> bi, 
	       signed bd, uimmediate<1> aa, uimmediate<1> lk);
   power_instr(BranchCond, CondCode cond, unsigned crfield, 
			    signed bd, uimmediate<1> aa, 
			    uimmediate<1> lk); 
   power_instr(BranchCondLinkReg, uimmediate<5> bo, uimmediate<5> bi,  
	       uimmediate<1> lk);
   power_instr(BranchCondLinkReg,  CondCode cond, unsigned crfield, 
	       uimmediate<1> lk);
   power_instr(BranchCondCountReg, uimmediate<5> bo, uimmediate<5> bi,  
	       uimmediate<1> lk);
   power_instr(BranchCondCountReg, CondCode cond, unsigned crfield,
	       uimmediate<1> lk);
   power_instr(Sc);
   
   power_instr(CondReg, CondRegOps, uimmediate<5> bt, 
	       uimmediate<5> ba, uimmediate<5> bb);
   power_instr(CondRegField, uimmediate<3> bf, uimmediate<3> bfa);
   
   
   power_instr(FPLoad, FPLoadOpsD, power_reg frt, 
	       power_reg ra, simmediate<16> d);
   power_instr(FPLoad, FPLoadOpsX, power_reg frt, power_reg ra, power_reg rb);
   power_instr(FPStore, FPStoreOpsD, power_reg frt, 
	       power_reg ra, simmediate<16> d);
   power_instr(FPStore, FPStoreOpsX, power_reg frs, power_reg ra, 
	       power_reg rb);
   
   power_instr(FPArithmetic, FPArithmeticOps, FPPrecision p,
	       power_reg frt, power_reg fra, power_reg frb, power_reg frc, 
	       uimmediate<1> rc);
   power_instr(FPConversion, FPConversionOps, power_reg frt, power_reg frb,
	       uimmediate<1> rc);
   
   power_instr(FPCompareUnordered, uimmediate<3> bf, 
	       power_reg fra, power_reg frb);
   power_instr(FPCompareOrdered, uimmediate<3> bf, power_reg fra, power_reg frb);
   
   power_instr(MoveFromFPSCR, power_reg frt, uimmediate<1> rc);
   power_instr(MoveToCondRegFromFPSCR, uimmediate<3> bf, uimmediate<3> bfa);
   power_instr(MoveToFPSCRFieldImm, uimmediate<3> bf, 
	       uimmediate<4> u, uimmediate<1> rc);
   power_instr(MoveToFPSCRFields, uimmediate<8> flm, 
	       power_reg frb, uimmediate<1> rc);
   power_instr(MoveToFPSCRBit0, uimmediate<5> bt, uimmediate<1> rc);
   power_instr(MoveToFPSCRBit1, uimmediate<5> bt, uimmediate<1> rc);

   power_instr(Rfi);
   
   // power_instr(SLBInvalidateEntry, power_reg rb);
   //power_instr(SLBInvalidateAll);
   
   power_instr(TLBInvalidateEntry, power_reg rb);
   power_instr(TLBInvalidateAll);
   power_instr(TLBSync);
   
   power_instr(ICacheBlockInvalidate, power_reg ra, power_reg rb);
   power_instr(InstructionSync);
   power_instr(DCacheBlockInvalidate, power_reg ra, power_reg rb);
   power_instr(DCacheBlockTouch, power_reg ra, power_reg rb);
   power_instr(DCacheBlockTouchStore, power_reg ra, power_reg rb);
   power_instr(DCacheBlockSetZero, power_reg ra, power_reg rb);
   power_instr(DCacheBlockStore, power_reg ra, power_reg rb);
   power_instr(DCacheBlockFlush, power_reg ra, power_reg rb);
   power_instr(CacheLineComputeSize, power_reg rt, power_reg ra);
   power_instr(CacheLineFlush, power_reg ra, power_reg rb);
   power_instr(CacheLineInvalidate, power_reg ra, power_reg rb);
   

   power_instr(EnforceInOrderExecutionIO);
   power_instr(MoveFromTimeBase, power_reg rt, power_reg tbr);

   
   void getInformation(registerUsage *,    // NULL if you don't want reg usage
                       memUsage *,         // NULL if you don't want mem usage
		       disassemblyFlags *, // NULL if you don't want disassembly
                       controlFlowInfo *,  // NULL if you don't want control flow
                       relocateInfo *      // NULL if you don't want to relocate
		       ) const;

 
   
   void getDisassembly(disassemblyFlags &) const;
   pdstring disassemble(kptr_t insnaddr) const; // calls above with usual params

   void getRelocationInfo(relocateInfo &) const;


   bool isCallInstr(kaddrdiff_t &offset) const;
      // NOTE: This will only return true for "call <const-addr>", not
      // for "jmpl <addr>, %o7"
   bool isCallToFixedAddress(kptr_t insnAddr, kptr_t &calleeAddr) const;
   bool  isUnconditionalJumpToFixedAddress() const;

   bool isBranchToFixedAddress(kptr_t insnAddr, kptr_t &brancheeAddr) const;
   bool isBranchToCounterAndLink() const { 
      return (  (xlform.op == 19) && (xlform.xo == 528) && xlform.lk);
   }
   bool isBranchToLinkRegAndLink() const { 
      return (  (xlform.op == 19) && (xlform.xo == 16) && xlform.lk);
   }
   /**** Restore instructions apparently have no equivalent on power -- Igor ***
   bool isRestoreInstr() const;
      // same apologies as above, but we need to check for a restore instr after
      // each call instr, to check for a particularly bizarre optimized sequence.
   bool isRestoreInstr(power_reg &rd) const;
   bool isRestoreUsingSimm13Instr(sparc_reg &rs1, // filled in
                                  int &simm13, // filled in
                                  sparc_reg &rd // filled in
                                  ) const;
   bool isRestoreUsing2RegInstr(sparc_reg &rs1, // filled in
                                sparc_reg &rs2, // filled in
                                sparc_reg &rd // filled in
                                ) const;
   */

   
   bool isLoadDForm(power_reg &rt, // filled in
			power_reg &ra, // filled in
			int &d // filled in
			);
   bool isLoadXForm(const power_reg &rt,
		    power_reg &ra, // filled in
		    power_reg &rb //filled in
		    );

   bool isORXForm(const power_reg &rt, // must match
			  power_reg &rs, // returned for you
			  power_reg &ra,  // returned for you
	        	  bool &complement // returned for you
                         ) const;
   bool isORDForm(const power_reg &ra, // must match
                         power_reg &rs, // returned for you
                         unsigned &val, // returned for you
		         bool &shifted //returned to you
                         ) const;
  
   

   bool isCmp(power_reg ra, int &val, unsigned &bf) const;
      // ra must match; we fill in 'val' for you

   bool isCmpWithValue(power_reg ra, int val, unsigned &bf) const;

   bool isAndDForm(power_reg ra, power_reg &rs, int &amt) const;

   bool isBranchBFormCondition(unsigned &bi, unsigned &bo, int &bd) const; 
   //condition must match
   
   bool isBranchBForm(unsigned &bi, unsigned &bo, int &bd) const;

   bool isBranchLink (unsigned &bi, unsigned &bo);
   bool isBranchLinkCondition (const unsigned &bi, const unsigned &bo); 
   //condition must match
   

   bool isBranchCounter (unsigned &bi, unsigned &bo);
   bool isBranchCounterCondition (const unsigned &bi, const unsigned &bo); 
   //condition must match
   

   //These are actually rotate instructions on power
   bool isShiftLeftLogicalToDestReg(const power_reg &ra, // must match
                                    power_reg &rs, // returned for you
                                    unsigned &shiftamt // returned for you
                                    ) const;
   bool isShiftRightLogicalToDestReg(power_reg ra, // must match
                                     power_reg &rs, // returned for you
                                     unsigned &shiftamt // returned for you
                                     ) const;

   bool isAdd2RegToDestReg(const power_reg &rt, // must match
			   power_reg &ra, // filled in
			   power_reg &rb // filled in
			   ) const;
   bool isAddImmToDestReg(const power_reg &rt, // you provide; must match
                          power_reg &ra, // we set this for you
                          unsigned &val // we ADD to this for you
                             // val must already be initialized with some value
                          ) const;
      // NOTE: if true, this routine *adds* to the parameter "val"!

   bool isAddImm(power_reg &ra, // filled in for you
                 unsigned &val, // NOTE: we ADD to this for you!!!!!
                 power_reg &rt // filled in for you
                 ) const;

   bool isAddImm(int &val) const;
      // sets val iff true

   // Should we have same scheme as above with shifted adds?? -Igor

   bool isSub2RegToDestReg(power_reg rt, // must match
                           power_reg &ra, // filled in for you
                           power_reg &rb // filled in for you
                           );  

   dependence calcDependence(const instr_t *nextInstr) const;
      // assuming 'this' is executed, followed by 'nextInstr', calculate the
      // dependency.  If there is more than one dependence, the stronger one
      // is returned.  I.e, if a true dependence exists, then return true;
      // else if an anti-dependence exists then return anti;
      // else if an output-dependence exists then return output;
      // else return none;

   
   // --------------------------------------------------

   instr_t *relocate(kptr_t old_insnaddr, kptr_t new_insnaddr) const;
   instr_t *tryRelocation(kptr_t old_insnaddr, kptr_t new_insnaddr, bool *relSuccess) const;

   uimmediate<5> getBOField() { return bform.bo; }
   uimmediate<5> getBIField() { return bform.bi; }

   static bool inRangeOfBranch(kptr_t from, kptr_t to);
   static bool inRangeOfBranchCond(kptr_t from, kptr_t to);

   //This calculates registers affected by move to/from spr instructions
   //Note that this could be a set, since we pretend CR and XER fields
   //are separate registers.  
   power_reg_set getSPRUsage(unsigned sprnum) const;
   
   static void getRangeOfBranch(kptr_t from, kptr_t &min_reachable_result,
                              kptr_t &max_reachable_result);


   static void getRangeOfBranchCond(kptr_t from, kptr_t &min_reachable_result,
                               kptr_t &max_reachable_result);
   
   static void getRangeOf(kptr_t from, unsigned numBitsOfSignedDisplacement,
			  kptr_t &min_reachable_result,
                          kptr_t &max_reachable_result);


   // --------------------------------------------------------------

   void changeBranchOffset(int new_insnbytes_offset);
      // must be a conditional branch instruction.  Changes the offset.
   void changeBranchOffset(const power_cfi &,
                                      int new_offset);
  
   void reverseBranchPolarity(const power_cfi &old_cfi);
   void reverseBranchPolarity();

   void changeBranchPredictBitIfApplicable(const power_cfi &,
                                           bool newPredict);
   void changeBranchPredictBitIfApplicable(bool newPredict);

   static power_instr
   cfi2ConditionalMove(const power_cfi &, // describes orig insn
                       power_reg srcReg, // if true, move this...
                       power_reg destReg // ...onto this
                       );
     // returns a conditional move instruction that moves srcReg onto destReg
     // iff the branch would be taken.  Works for any conditional branch 
     // (bicc, bpcc, bpr, fbfcc, fbpfcc).  Probably works for unconditional 
     // branches (ba, bn)  too.

   void changeSimm16(int new_val);

   // --------------------------------------------------------------

   static void gen32imm(insnVec_t  * /* piv  */, uint32_t /* val */, power_reg /*rd */ ); 
   
   static void gen64imm(insnVec_t *piv, uint64_t val, power_reg rd, 
                        power_reg rtemp); 

   // Set the register to the given 32-bit address value
   // Try to produce the shortest code, depending on the address value
   static void genImmAddr(insnVec_t  * /* piv */, uint32_t /* addr */, 
                          power_reg /* rt */) {
      assert(false && "no need to generate immediate address on power");
   }
   

   // Set the register to the given 64-bit address value
   // For now, we do not optimize based on the address value, though
   // we certainly could save a few instructions here and there.
   static void genImmAddr(insnVec_t *piv, uint64_t addr, 
                          power_reg rt) {
       genImmAddrGeneric(piv, addr, rt, true /* setLow */);
   }

   // The next two functions set the register to the given address
   // value, but do not attempt to produce smaller code, since we may patch
   // it with a different address later. Do not set the lowest 12 bits of
   // the address if the caller requests so.
   static void genImmAddrGeneric(insnVec_t *piv, uint32_t addr, 
				 const power_reg& rt, bool setLow = true); 
   static void genImmAddrGeneric(insnVec_t *piv, uint64_t addr, 
				 const power_reg& rd, bool setLow = true);

   // Generate "genImmAddr(hi(to)) -> link; jmpl [link + lo(to)], link"
   // Note: we require link != sparc_reg::g0, since we need a scratch 
   // register for sure
   static void genBranchAndLink(insnVec_t *piv, kptr_t to);

   // Generate either "call disp" or
   // "genImmAddr(hi(to)) -> o7, jmpl o7+lo(to),o7" if the displacement
   // is not in the range for the call instruction
   static void genCallAndLink(insnVec_t  * /* piv */, kptr_t /* from */, 
                              kptr_t  /* to */); 

   // Same goal, but do not optimize produced code for size. However,
   // we still want the 32-bit version to emit one call instruction -- hence
   // two overloaded functions
   static void genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, 
                                                     uint32_t to);
   static void genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, 
                                                     uint64_t to);

   static void genCallAndLinkGeneric_unknownFromAndToAddr(
					insnVec_t *piv);

   //for debugging
   void printOpcodeFields() const;
};

#endif


