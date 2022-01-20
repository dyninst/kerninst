// sparc_instr.h
// Ariel Tamches

#ifndef _SPARC_INSTR_H_
#define _SPARC_INSTR_H_

#include "common/h/String.h"
#include "common/h/Vector.h"
#include "pdutil/h/xdr_send_recv.h"
#include "util/h/kdrvtypes.h"
#include "uimm.h" // unsigned immediate bits
#include "simm.h" // signed immediate bits
#include "sparc_reg.h"
#include "sparc_reg_set.h"
#include "bitUtils.h"
#include "instr.h"

class insnVec_t;

class sparc_instr : public instr_t {
 public:
   class Cas {};
   class Load {};
   class LoadAltSpace {};
   class LoadFP {};
   class Store {};
   class StoreAltSpace {};
   class StoreFP {};
   class LoadStoreUB {};
   class Swap {};
   class SetHi {}; class HiOf {}; class HiOf64 {}; class UHiOf64 {};
   class PopC {};
   class Prefetch {};
   class PrefetchAltSpace {};
   
   class Nop {};
   class Logic {};
   class BSet {}; // pseudo-instr; actually or
   class Cmp {};
   class Tst {};
   class Mov {}; // pseudo-instr; actually or
   class Movcc {};
   class MovR {};
   class FMovcc {};
   class FMovR {};

   class Clear {}; // pseudo-instr; actually or (if clearing a reg) or st (mem loc)
   class Shift {};
   class SignX {};
   class Not {};
   class Neg {};

   class Add {};
   class Sub {};
   class MulX {};
   class SDivX {};
   class UDivX {};
   class Save {};
   class Restore {};
   class FlushW {};

   class Bicc {};
   class BPcc {};
   class BPr {};
   class FBPfcc {};
   class FBfcc {};
   
   class CallAndLink {}; // writes to o7  (r15)
   class JumpAndLink {};
   class Jump {};
   class Ret {};  // pseudo-instr; actually jmpl %i7+8, %g0
   class Retl {}; // pseudo-instr; actually jmpl %o7+8, %g0
   class V9Return {};
   class Trap {};
   class StoreBarrier {};
   class MemBarrier {};
   class UnImp {};
   class Flush {};

   class FAdd {};
   class FSub {};
   class FCmp {};
   class Float2Int {};
   class Float2Float {};
   class Int2Float {};
   class FMov {};
   class FNeg {};
   class FAbs {};
   class FMul {};
   class FDiv {};
   class FSqrt {};

   class ReadStateReg {}; // e.g. for rd %tick, <dest>
   class WriteStateReg {}; // e.g. for wr <rs1>, <rs2>, %ccr

   class ReadPrivReg {}; // e.g. for rdpr %pil, <dest>
   class WritePrivReg {}; // e.g. for wrpr <rs>, %pil

   enum LoadOps {ldSignedByte=0x09, ldSignedHalfword=0x0a, ldSignedWord=0x8,
		 ldUnsignedByte=0x01, ldUnsignedHalfword=0x02, ldUnsignedWord=0x00,
		 ldExtendedWord=0x0b, ldDoubleWord=0x03};
   enum StoreOps {stByte=0x05, stHalfword=0x06, stWord=0x04,
		  stExtended=0x0e, stDoubleWord=0x07};

   enum FPLoadOps {ldSingleReg=0x20, ldDoubleReg=0x23, ldQuadReg=0x22};
   enum FPStoreOps {stSingleReg=0x24, stDoubleReg=0x27, stQuadReg=0x26};

   enum logical_types {logic_and=1, logic_andn=5, logic_or=2,
                       logic_orn=6, logic_xor=3, logic_xnor=7};
      // note: this enum type has its numbers chosen to match the op3.
      // andn and orn negate the 2d op and then perform a normal and/or;
      // i.e. they are (reg1 and reg2') and (reg1 or reg2'), respectively.
      // xnor is different; it is: (reg1 xor reg2)'

   enum ShiftKind {leftLogical=0x25, rightLogical=0x26, rightArithmetic=0x27};

   // note that the values chosen for the following icc enumerated types correspond
   // exactly to the condition bits themselves...let's keep it that way.
   enum IntCondCodes {iccAlways=8, iccNever=0, iccNotEq=9, iccEq=1,
                        iccGr=10, iccLessOrEq=2, iccGrOrEq=11, iccLess=3,
                        iccGrUnsigned=12, iccLessOrEqUnsigned=4,
                        /* iccCarryClear=13, */ iccGrOrEqUnsigned=13,
                        /* iccCarrySet=5, */ iccLessUnsigned=5,
                        iccPositive=14,
			iccNegative=6, iccOverflowClear=15, iccOverflowSet=7};
   static const IntCondCodes reversedIntCondCodes[16];
   
   enum FloatCondCodes {fccAlways=8, fccNever=0, fccUnord=7, fccGr=6,
                        fccUnordOrGr=5, fccLess=4, fccUnordOrLess=3,
                        fccLessOrGr=2, fccNotEq=1, fccEq=9,
                        fccUnordOrEq=0xa, fccGrOrEq=0xb, fccUnordOrGrOrEq=0xc,
                        fccLessOrEq=0xd, fccUnordOrLessOrEq=0xe, fccOrd=0xf};
   static const FloatCondCodes reversedFloatCondCodes[16];

   enum RegCondCodes {regReserved1=0, regZero=1, regLessOrEqZero=2,
                        regLessZero=3, regReserved2=4, regNotZero=5,
                        regGrZero=6, regGrOrEqZero=7};
   static const RegCondCodes reversedRegCondCodes[8];

   enum MembarMmaskCodes {StoreStore=8, LoadStore=4, StoreLoad=2, LoadLoad=1};
   enum MembarCmaskCodes {Sync=4, MemIssue=2, Lookaside=1};

   struct sparc_ru : public registerUsage {
      union {
         unsigned save_restore_return_stuff;
         struct {
            unsigned isSave : 1;
            unsigned isRestore : 1;
            unsigned isV9Return : 1;
         } sr; // sr stands for 'save/restore flags'
      };
      sparc_ru() {
         save_restore_return_stuff = 0;
	 // faster than setting the bit fields, plus avoids purify UMCs due to
	 // the read-modify-write nature of bit field operations

	 assert(!sr.isSave && !sr.isRestore && !sr.isV9Return);
	 //isSave = isRestore = isV9Return = false;
      }
   };

   struct sparc_cfi : public controlFlowInfo {
      union {
         bool xcc; // for BPcc; if true, xcc; else, icc.  Bicc is implicitly icc only.
         unsigned fcc_num; // for FBPfcc; 0-3 for fcc0 thru fcc3.
      };

      // The specific kind of control flow instruction, 
      // along with predict & annul bits.
      union {
         unsigned sparc_bit_stuff;
         struct {
	    unsigned isRetl : 1; // return from leaf procedure
            unsigned isV9Return : 1; // new in v9 (restore & a jump w/o link)
            unsigned isDoneOrRetry : 1; // new in v9 (return from trap handler)

            // Conditional Branches:
            unsigned isBicc : 1;
            unsigned isBPcc : 1; // new in v9
            unsigned isBPr : 1;  // new in v9
            unsigned isFBfcc : 1;
            unsigned isFBPfcc : 1; // new in v9
            unsigned predict : 1; // only defined for BPcc, FBPfcc, and BPr
            unsigned annul : 1; // defined for all conditional branches
         } sparc_fields;
      };
      sparc_cfi() : controlFlowInfo(), fcc_num(0), sparc_bit_stuff(0) {}
      ~sparc_cfi() {}
   };

 private:
   void getInformation10_unimpl_0x36(registerUsage *,
                                     disassemblyFlags *,
                                     const sparc_reg_set rdAsIntReg,
                                     const sparc_reg_set rs1AsIntReg,
                                     const sparc_reg_set rs2AsIntReg) const;

   static pdstring disass_asi(unsigned);

   void disass_11_intloads(disassemblyFlags *,
                           unsigned op3, const sparc_reg_set &rd,
                           bool i, bool alternateSpace) const;

   void disass_11_floatloads(disassemblyFlags *disassFlags,
                             unsigned op3, const sparc_reg_set &rd) const;

   void disass_11_loadfpstatereg(disassemblyFlags *disassFlags,
                                 bool extended) const;

   void disass_11_load_fp_from_alternate_space(disassemblyFlags *disassFlags,
                                               unsigned op3, bool i,
                                               const sparc_reg_set &rd) const;
   
   void disass_11_intstores(disassemblyFlags *disassFlags,
                            unsigned op3, bool i,
                            const sparc_reg_set &rd,
                            bool alternateSpace) const;

   void disass_11_floatstores(disassemblyFlags *disassFlags,
                              unsigned op3,
                              const sparc_reg_set &rd, // a float reg
                              bool i, bool alternateSpace) const;

   void disass_11_store_fp_statereg(disassemblyFlags *disassFlags,
                                    bool expanded) const;

   void disass_11_cas(disassemblyFlags *disassFlags,
                      bool i, bool extended,
                      const sparc_reg_set &rs1,
                      const sparc_reg_set &rs2,
                      const sparc_reg_set &rsd) const;

   void disass_11_prefetch(disassemblyFlags *disassFlags,
                           unsigned fcn, bool i,
                           bool alternateSpace) const;

   void disass_10_bitlogical(disassemblyFlags *disassFlags,
                             const unsigned op3, bool i, bool cc,
                             const sparc_reg_set &rs1,
                             const sparc_reg_set &rs2,
                             const sparc_reg_set &rd) const;

   void disass_10_shiftlogical(disassemblyFlags *disassFlags,
                               unsigned op3,
                               const sparc_reg_set &rs1,
                               const sparc_reg_set &rd) const;

   void disass_10_addsubtract(disassemblyFlags *disassFlags,
                              const unsigned op3, bool i, bool cc,
                              const sparc_reg_set &rs1,
                              const sparc_reg_set &rd) const;

   void disass_10_mulscc(disassemblyFlags *disassFlags,
                         const sparc_reg_set &rs1,
                         const sparc_reg_set &rd) const;

   void disass_10_muldiv32(disassemblyFlags *disassFlags,
                           unsigned op3,
                           bool cc,
                           const sparc_reg_set &rs1,
                           const sparc_reg_set &rd) const;

   void disass_10_muldiv64(disassemblyFlags *disassFlags,
                           unsigned op3,
                           const sparc_reg_set &rs1,
                           const sparc_reg_set &rd) const;

   void disass_10_saverestore(disassemblyFlags *disassFlags,
                              bool isSave,
                              const sparc_reg_set &rs1,
                              const sparc_reg_set &rs2,
                              const sparc_reg_set &rd) const;

   void disass_10_doneretry(disassemblyFlags *disassFlags,
                            unsigned fcn) const;

   void disass_10_v9return(disassemblyFlags *disassFlags) const;

   void disass_10_ticc(disassemblyFlags *disassFlags,
                       bool i,
                       bool cc0, bool cc1,
                       const sparc_reg_set &rs2) const;

   void disass_10_wrasr(disassemblyFlags *disassFlags,
                        const sparc_reg_set &rs1,
                        unsigned rdBits) const;

   void disass_10_saved_restored(disassemblyFlags *disassFlags,
                                 bool saved) const;

   void disass_10_wrpr(disassemblyFlags *disassFlags,
                       const sparc_reg_set &rs1) const;

   void disass_10_flush(disassemblyFlags *disassFlags) const;

   void disass_10_movcc(disassemblyFlags *disassFlags,
                        sparc_reg condition_reg, // fcc0-3 or icc or xcc
                        const sparc_reg_set &rd) const;

   void disass_10_movr(disassemblyFlags *disassFlags,
                       unsigned cond,
                       const sparc_reg_set &rs1,
                       const sparc_reg_set &rs2,
                       const sparc_reg_set &rd) const;

   void disass_10_rdpr(disassemblyFlags *disassFlags,
                       unsigned whichreg,
                       const sparc_reg_set &rd) const;

   void disass_10_popc(disassemblyFlags *disassFlags,
                       const sparc_reg_set &rd) const;
   
   void disass_10_jmpl(disassemblyFlags *disassFlags,
                       const sparc_reg_set &rs1,
                       const sparc_reg_set &rd) const;
   
   void disass_10_rd_state_reg(disassemblyFlags *disassFlags,
                               const sparc_reg_set &rd,
                               unsigned rs1bits, bool i) const;

   void getInformation_10_0x34_floats(registerUsage *ru,
                                      disassemblyFlags *disassFlags) const;

   void getInformation_10_0x35_floats(registerUsage *ru,
                                      disassemblyFlags *disassFlags) const;

   void disass_01_call(disassemblyFlags *, kaddrdiff_t displacement) const;

   void disass_00_sethi(disassemblyFlags *, const sparc_reg_set &rd) const;

   void disass_00_bpr(disassemblyFlags *disassFlags,
                      unsigned cond, bool annulled, bool predict,
                      const sparc_reg_set &rs1,
                      int offset) const;

   void disass_00_bpcc_fbpcc(disassemblyFlags *disassFlags,
                             bool floatOp, bool predict, bool annulled,
                             bool cc0, bool cc1, int displacement) const;

   void disass_00_bicc_fbfcc(disassemblyFlags *disassFlags,
                             bool floatOp, bool annulled,
                             int displacement) const;
   
   void setImm13Common(uimmediate<2> hi2bits, sparc_reg destReg,
		       uimmediate<6> op3, sparc_reg srcReg);
   void setImm13Common(uimmediate<2> hi2bits, uimmediate<5> rdBits,
		       uimmediate<6> op3, sparc_reg srcReg);
   void setSimm13Style(uimmediate<2> hi2bits, sparc_reg destReg,
		       uimmediate<6> op3, sparc_reg srcReg,
		       simmediate<13> offset);
   void setSimm13Style(uimmediate<2> hi2bits, uimmediate<5> rdBits,
		       uimmediate<6> op3, sparc_reg srcReg,
		       simmediate<13> offset);
   void setUimm13Style(uimmediate<2> hi2bits, sparc_reg destReg,
		       uimmediate<6> op3, sparc_reg srcReg,
		       uimmediate<13> offset);

   void set2RegStyle(uimmediate<2> hi2bits, sparc_reg destReg,
		     uimmediate<6> op3, sparc_reg srcReg1,
		     uimmediate<8> asi, sparc_reg srcReg2);

   void set2RegStyle(uimmediate<2> hi2bits, uimmediate<5> rdBits,
		     uimmediate<6> op3, sparc_reg srcReg1,
		     uimmediate<8> asi, sparc_reg srcReg2);

   void sethicommon(unsigned val, sparc_reg dest);

   uimmediate<6> calcLogicalOp3(logical_types, bool modifycc) const;
   uimmediate<6> calcAddOp3(bool modifycc, bool carry);
   uimmediate<6> calcSubOp3(bool modifycc, bool carry);

   uimmediate<5> calcFloatRegEncoding1(sparc_reg floatReg);
   uimmediate<5> calcFloatRegEncoding2(sparc_reg floatReg);
   uimmediate<5> calcFloatRegEncoding4(sparc_reg floatReg);
   uimmediate<5> calcFloatRegEncoding(sparc_reg floatReg, unsigned precision);
      // remember that sparc's floating point register have a goofy encoding,
      // due to backwards compatibility reasons.  This utility routine helps.

   pdstring disass_reg_or_imm13(bool disConstAsSigned) const;
   pdstring disass_reg_or_imm11(bool disConstAsSigned) const;
   pdstring disass_reg_or_shcnt() const;
   pdstring disass_software_trap_number() const;
   pdstring disass_addr(bool displayBracesAroundAddr) const;
   pdstring disass_addr_simm13() const;
   pdstring disass_addr_simm7() const;
   pdstring disass_addr_2reg() const;
   pdstring disass_simm7() const;
   pdstring disass_simm11() const;
   pdstring disass_simm13() const;
   pdstring disass_uimm11() const;
   pdstring disass_uimm13() const;

   pdstring disass_simm(int simm) const; // any # of bits
   pdstring disass_uimm(unsigned uimm) const; // any # of bits

   template <unsigned numbits>
   void replaceBits(unsigned loBit, const uimmediate<numbits> &replacementValue) {
      this->raw = ::replaceBits(this->raw, loBit, replacementValue);
         // uimm.h
   }
   template <unsigned numbits>
   void replaceBits(unsigned loBit, const simmediate<numbits> &replacementValue) {
      this->raw = ::replaceBits(this->raw, loBit, replacementValue);
         // simm.h
   }
   

   int32_t getBitsSignExtend(unsigned lobit, unsigned hibit) const;

   unsigned getOp3() const {
      return getOp3Bits(raw);
   }
   unsigned getHi2Bits() const {
      return (raw >> 30) & 0x3;
   }

   sparc_reg_set getRs1AsInt() const {
      return sparc_reg_set(sparc_reg_set::singleIntReg, getRs1Bits(raw));
   }
   sparc_reg_set getRs2AsInt() const {
      return sparc_reg_set(sparc_reg_set::singleIntReg, getRs2Bits(raw));
   }
   sparc_reg_set getRdAsInt() const {
      return sparc_reg_set(sparc_reg_set::singleIntReg, getRdBits(raw));
   }

   sparc_reg getRs1As1IntReg() const {
      return sparc_reg(sparc_reg::rawIntReg, getRs1Bits(raw));
   }
   sparc_reg getRs2As1IntReg() const {
      return sparc_reg (sparc_reg::rawIntReg, getRs2Bits(raw));
   }
   sparc_reg getRdAs1IntReg() const {
      return sparc_reg(sparc_reg::rawIntReg, getRdBits(raw));
   }
   

   sparc_reg_set bits2FloatReg(unsigned bits, unsigned reg_precision) const;
      // reg_precision: 1 (single), 2 (double), or 4 (quad)

   sparc_reg_set getRs1AsFloat(unsigned floatsize) const;
   sparc_reg_set getRs2AsFloat(unsigned floatsize) const;
   sparc_reg_set getRdAsFloat(unsigned floatsize) const;

   int getSimm7() const { return getBitsSignExtend(0, 6); }
   int getSimm10() const { return getBitsSignExtend(0, 9); }
   int getSimm11() const { return getBitsSignExtend(0, 10); }
   int getSimm13() const { return getBitsSignExtend(0, 12); }

   unsigned  getUimm11() const { return getRawBits0(10); }
   unsigned  getUimm13() const { return getUimm13Bits(raw); }
   
   IntCondCodes getIntCondCode_25_28() const; // gets it from bits 25 thru 28
   IntCondCodes getIntCondCode_14_17() const; // gets it from bits 14 thru 17
   FloatCondCodes getFloatCondCode_25_28() const; // bits 25 thru 28
   FloatCondCodes getFloatCondCode_14_17() const; // bits 14 thru 17
   bool geti() const { return getIBit(raw); }
   
   bool getcc1() const {return getRawBit(21);}
   bool getcc0() const {return getRawBit(20);}
   bool getPredict() const {return getRawBit(19);}

   void getInformation11(registerUsage *, memUsage *, disassemblyFlags *,
                         sparc_cfi *, relocateInfo *) const;
   void getInformation10(registerUsage *, memUsage *, disassemblyFlags *,
                         sparc_cfi *, relocateInfo *) const;
   void getInformation01(registerUsage *, memUsage *, disassemblyFlags *,
                         sparc_cfi *, relocateInfo *) const;
   void getInformation00(registerUsage *, memUsage *, disassemblyFlags *,
                         sparc_cfi *, relocateInfo *) const;
		       
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

   void setto_logic(logical_types theop, bool cc,
                    sparc_reg dest, sparc_reg src1, sparc_reg src2);
   void setto_logic(logical_types theop, bool cc,
                    sparc_reg dest, sparc_reg src, simmediate<13> constant);
      // imho, it's weird for logical operations like this one to sign extend
      // the constant operand...but sparc does just that, so we must take in
      // an simmediate<13>, not a uimmediate<13>
   
   void changeBranchOffsetAndAnnulBicc(const sparc_cfi &oldcfi,
                                       int new_insnbytes_offset, bool);
   void changeBranchOffsetAndAnnulBPcc(const sparc_cfi &oldcfi,
                                       int new_insnbytes_offset, bool);
   void changeBranchOffsetAndAnnulBPr(const sparc_cfi &oldcfi,
                                      int new_insnbytes_offset, bool);
   void changeBranchOffsetAndAnnulFBfcc(const sparc_cfi &oldcfi,
                                        int new_insnbytes_offset, bool);
   void changeBranchOffsetAndAnnulFBPfcc(const sparc_cfi &oldcfi,
                                         int new_insnbytes_offset, bool);
   
 public:
   // The following static member vrbles are meant to be passed as the first param
   // to the constructors of this class...making them each of a different type (instead
   // of an enumerated type) really beefs up type checking nicely.
   static Cas cas;
   static Load load;
   static LoadAltSpace loadaltspace;
   static LoadFP loadfp;
   static Store store;
   static StoreAltSpace storealtspace;
   static StoreFP storefp;
   static LoadStoreUB loadstoreub;
   static Swap swap;
   static SetHi sethi;
   static PopC popc;
   static Prefetch prefetch;
   static PrefetchAltSpace prefetchAltSpace;
   static Nop nop;
   static Logic logic;
   static BSet bset;
   static Cmp cmp;
   static Tst tst;
   static Mov mov;
   static Movcc movcc;
   static MovR movr;
   static FMovcc fmovcc;
   static FMovR fmovr;
   static Clear clear;
   static Shift shift;
   static SignX signx;
   static Not _not; // "not" is now a reserved word in c++
   static Neg neg;

   static Add add;
   static Sub sub;
   static MulX mulx;
   static SDivX sdivx;
   static UDivX udivx;

   static Save save;
   static Restore restore;
   static FlushW flushw;

//     static Done done;
//     static Retry retry;

   static Bicc bicc;
   static BPcc bpcc;
   static BPr bpr;
   static FBPfcc fbpfcc;
   static FBfcc fbfcc;

   static CallAndLink callandlink;
   static JumpAndLink jumpandlink;
   static Jump jump;
   static Ret ret;
   static Retl retl;
   static V9Return v9return;
   static Trap trap;
   static StoreBarrier storebarrier;
   static MemBarrier membarrier;
   static UnImp unimp;
   static Flush flush;
   static FAdd fadd;
   static FSub fsub;
   static FCmp fcmp;
   static Float2Int float2int;
   static Float2Float float2float;
   static Int2Float int2float;
   static FMov fmov;
   static FNeg fneg;
   static FAbs fabs;
   static FMul fmul;
   static FDiv fdiv;
   static FSqrt fsqrt;
   static ReadStateReg readstatereg;
   static WriteStateReg writestatereg;
   static ReadPrivReg readprivreg;
   static WritePrivReg writeprivreg;

   void* operator new(size_t);
   void* operator new(size_t, void *place) {return place;}
   void operator delete(void*);

   sparc_instr(const sparc_instr &src) : instr_t(src.raw) {}
   sparc_instr(uint32_t iraw) : instr_t(iraw) {}

   sparc_instr(XDR *xdr) : instr_t() {
      if(!P_xdr_recv(xdr, raw))
	 assert(0 && "recv of sparc raw bits failed");
   }

   ~sparc_instr() {}

   sparc_instr& operator=(const sparc_instr &src) { 
      raw = src.raw; 
      return *this;
   }

   bool operator==(const sparc_instr &other) const {
      return raw == other.raw;
   }

   bool send(XDR *xdr) const {
      return P_xdr_send(xdr, raw);
   }

   static bool instructionHasValidAlignment(kptr_t addr);
   static bool hasUniformInstructionLength();
   static bool condBranchesUseDelaySlot();

   // ----------------------------------------------------------------
   // Making instructions
   // ----------------------------------------------------------------
   // 
   // Note the style of the arguments: they are meant to read like
   // an assembly language printout (well, not exactly of course, but you
   // get the idea).  Thus, as according to sparc conventions for
   // asm instructions, rd (destination register) is usually last.
   //
   // (Note that I haven't finished translating all of these ctors into
   // the "nice" format according to the above rules)
   // ---------------------------------------------------------------

   sparc_instr(Cas, bool extended, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);

   sparc_instr(Load, LoadOps, sparc_reg srcAddrReg,
	       simmediate<13> offset, sparc_reg rd);
   sparc_instr(Load, LoadOps, sparc_reg srcAddrReg1,
	       sparc_reg srcAddrReg2, sparc_reg rd);

   sparc_instr(bool, LoadAltSpace, LoadOps, sparc_reg srcAddrReg,
	       simmediate<13> offset, sparc_reg rd);
      // load from alternate space.  I considered just "LoadA" for the 1st param type,
      // but it's too susceptible to accidental spelling errors.

   sparc_instr(bool, LoadFP, FPLoadOps,
	       sparc_reg srcAddrReg, simmediate<13> offset,
               sparc_reg rd);
   sparc_instr(bool, LoadFP, FPLoadOps,
	       sparc_reg srcAddrReg1, sparc_reg srcAddrReg2,
               sparc_reg rd);

   sparc_instr(Store, StoreOps, sparc_reg srcValueReg,
	       sparc_reg addressReg, simmediate<13> offset);
   sparc_instr(Store, StoreOps, sparc_reg srcValueReg,
	       sparc_reg addressReg1, sparc_reg addressReg2);

   sparc_instr(StoreAltSpace, StoreOps, sparc_reg srcValueReg,
	       sparc_reg addressReg, simmediate<13> offset);
//     sparc_instr(StoreAltSpace, StoreOps, sparc_reg srcValueReg,
//  	       sparc_reg addressReg1, sparc_reg addressReg2);

   sparc_instr(StoreFP, FPStoreOps, sparc_reg srcValueReg,
	       sparc_reg addressReg, simmediate<13> offset);
   sparc_instr(StoreFP, FPStoreOps, sparc_reg srcValueReg,
	       sparc_reg addressReg1, sparc_reg addressReg2);
   
   sparc_instr(LoadStoreUB, sparc_reg srcAndDestReg,
	       sparc_reg addressReg, simmediate<13> offset);
   sparc_instr(LoadStoreUB, sparc_reg srcAndDestReg,
	       sparc_reg addressReg1, sparc_reg addressReg2);

   sparc_instr(Swap, sparc_reg destReg,
	       sparc_reg addressReg, simmediate<13> offset);
   sparc_instr(Swap, sparc_reg destReg,
	       sparc_reg addressReg1, sparc_reg addressReg2);

   // Note: the following routines do *not* sign-extend anything:
   static unsigned hi_of_32(uint32_t val); // high 22 bits
   static unsigned lo_of_32(uint32_t val); // low 10 bits
   static unsigned hi_of_64(uint64_t val64); // high 22 bits of the low 32 bit half
   static unsigned lo_of_64(uint64_t val64); // low 10 bits
   static unsigned uhi_of_64(uint64_t val64); // high 22 bits of the hi 32 bit half
   static unsigned ulo_of_64(uint64_t val64); // low 10 bits of the hi 32 bit half
   
   sparc_instr(SetHi, HiOf, uint32_t val, sparc_reg dest);
   sparc_instr(SetHi, HiOf64, uint64_t val, sparc_reg dest);
   sparc_instr(SetHi, UHiOf64, uint64_t val, sparc_reg dest);
   
   sparc_instr(Nop);

   sparc_instr(PopC, sparc_reg rs2, sparc_reg rd);

   enum PrefetchFcn {prefetchForSeveralReads=0, prefetchForOneRead=1,
                     prefetchForSeveralWrites=2, prefetchForOneWrite=3,
                     prefetchPage=4,
                     prefetchImpDep16=16, prefetchImpDep17=17, prefetchImpDep18=18,
                     prefetchImpDep19=19, prefetchImpDep20=20, prefetchImpDep21=21,
                     prefetchImpDep22=22, prefetchImpDep23=23, prefetchImpDep24=24,
                     prefetchImpDep25=25, prefetchImpDep26=26, prefetchImpDep27=27,
                     prefetchImpDep28=28, prefetchImpDep29=29, prefetchImpDep30=30,
                     prefetchImpDep31=31};
   
   sparc_instr(Prefetch, PrefetchFcn, sparc_reg rs1, sparc_reg rs2);
   sparc_instr(Prefetch, PrefetchFcn, sparc_reg rs1, simmediate<13> offset);
   
   sparc_instr(PrefetchAltSpace, PrefetchFcn, sparc_reg rs1, sparc_reg rs2);
   sparc_instr(PrefetchAltSpace, PrefetchFcn, sparc_reg rs1, simmediate<13> offset);

   sparc_instr(Logic, logical_types theop, bool cc,
	       sparc_reg dest, sparc_reg src1, sparc_reg src2);
   sparc_instr(Logic, logical_types theop, bool cc,
	       sparc_reg dest, sparc_reg src, simmediate<13> constant);

   sparc_instr(bool, BSet, simmediate<13> constant, sparc_reg reg);

   sparc_instr(Cmp, sparc_reg rs1, sparc_reg rs2);
   sparc_instr(Cmp, sparc_reg rs1, simmediate<13>);

   sparc_instr(Tst, sparc_reg rs1);

   sparc_instr(Mov, sparc_reg rs1, sparc_reg rd);
   
   sparc_instr(Mov, simmediate<13> constant, sparc_reg rd);
      // mov constant, rd (a pseudo-instr; actually or %g0, constant, destReg)

   sparc_instr(Movcc, unsigned cond, bool xcc, sparc_reg srcval, sparc_reg destreg);
   sparc_instr(Movcc, unsigned cond, unsigned fcc_num, // thru 3
               sparc_reg srcval, sparc_reg destreg);

   sparc_instr(MovR, unsigned cond, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
      // if rs1 matches cond then rd <- rs2
   sparc_instr(MovR, unsigned cond, sparc_reg rs1, int val, sparc_reg rd);
      // if rs1 matches cond then rd <- val

   sparc_instr(FMovR, unsigned precision, unsigned, sparc_reg rs1, // int reg
               sparc_reg rs2, sparc_reg rd
               );

   sparc_instr(FMovcc, unsigned precision,
               unsigned cond, bool xcc,
               sparc_reg rs2, sparc_reg rd // float regs
               );
   sparc_instr(FMovcc, unsigned precision,
               unsigned, unsigned fcc_num,
               sparc_reg rs2, sparc_reg rd // float regs
               );

   sparc_instr(Clear, sparc_reg destReg);
   
   sparc_instr(Clear, StoreOps, sparc_reg addrReg1, sparc_reg addrReg2);
      // clr [rs1 + rs2] actually st %g0, [rs1 + rs2] (or stb, sth, or stx)...but don't
      // use std
   sparc_instr(Clear, StoreOps, sparc_reg addrReg1, simmediate<13> offset);
      // clr [rs1 + offset] actually st %g0, [rs1 + offset] (or stb, sth, or stx)...but
      // don't use std

   sparc_instr(Shift, ShiftKind kind, bool extended,
	       sparc_reg destReg, sparc_reg srcReg1, sparc_reg srcReg2);
      // reg[destReg] <- (srcReg1 << srcReg2)  [i.e. srcReg2 contains the # of bits
      //					to shift by]
   sparc_instr(Shift, ShiftKind, bool extended,
	       sparc_reg destReg, sparc_reg srcReg, uimmediate<6> constant);
      // reg[destReg] <- (srcReg << constant) [i.e. constant hardcodes the # of bits
      //				       to shift by]

   sparc_instr(SignX, sparc_reg r);
      // sra r, 0, r
   sparc_instr(SignX, sparc_reg src, sparc_reg dest);
      // sra src, 0, dest

   sparc_instr(Not, sparc_reg r);
      // xnor r, %g0, r
   sparc_instr(Not, sparc_reg src, sparc_reg dest);
      // xnor src, %g0, dest

   sparc_instr(Neg, sparc_reg r);
      // xnor r, %g0, r
   sparc_instr(Neg, sparc_reg src, sparc_reg dest);
      // xnor src, %g0, dest

   sparc_instr(Add, bool modifyicc, bool carry, sparc_reg destReg,
	       sparc_reg srcReg1, sparc_reg srcReg2);
   sparc_instr(Add, bool modifyicc, bool carry, sparc_reg destReg,
	       sparc_reg srcReg, simmediate<13> constant);
   sparc_instr(Add, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
   sparc_instr(Add, sparc_reg rs1, simmediate<13> constant, sparc_reg rd);
	       
   sparc_instr(Sub, bool modifyicc, bool carry, sparc_reg destReg,
	       sparc_reg srcReg1, sparc_reg srcReg2);
   sparc_instr(Sub, bool modifyicc, bool carry, sparc_reg destReg,
	       sparc_reg srcReg, simmediate<13> constant);
   sparc_instr(Sub, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
   sparc_instr(Sub, sparc_reg rs1, simmediate<13> constant, sparc_reg rd);

   sparc_instr(MulX, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
   sparc_instr(MulX, sparc_reg rs1, simmediate<13> val, sparc_reg rd);
   sparc_instr(SDivX, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
   sparc_instr(SDivX, sparc_reg rs1, simmediate<13> val, sparc_reg rd);
   sparc_instr(UDivX, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
   sparc_instr(UDivX, sparc_reg rs1, simmediate<13> val, sparc_reg rd);

   sparc_instr(Save, simmediate<13> constant);
   sparc_instr(Save, sparc_reg destReg, sparc_reg srcReg1,
	       sparc_reg srcReg2);
   sparc_instr(Save, sparc_reg destReg, sparc_reg srcReg,
	       simmediate<13> constant);

   sparc_instr(Restore); // restore %g0, %g0, %g0
   sparc_instr(bool, Restore, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
   sparc_instr(Restore, sparc_reg rs1, simmediate<13> constant, sparc_reg rd);
      // rs1 in old frame, rd in new (post-restore) frame
   sparc_instr(FlushW);

   sparc_instr(Bicc, unsigned cond, bool annulled,
               kaddrdiff_t displacement_nbytes);
   sparc_instr(BPcc, unsigned cond, bool annulled, bool predictTaken, bool xcc,
               kaddrdiff_t displacement_nbytes);
   sparc_instr(BPr, unsigned cond, bool annulled, bool predictTaken,
               sparc_reg rs1, kaddrdiff_t displacement_nbytes);
   sparc_instr(FBfcc, unsigned, bool annulled,
               kaddrdiff_t displacement_nbytes);
   sparc_instr(FBPfcc, unsigned,
               bool annulled, bool predictTaken,
               unsigned fcc_num, /* 0 thru 3...for fcc0 thru fcc3 */
               kaddrdiff_t displacement_nbytes);

   sparc_instr(CallAndLink, kaddrdiff_t displacement); 
   // pc-relative (we divide by 4 for you)

   sparc_instr(CallAndLink, kptr_t to, kptr_t currPC);
   // absolute (we xlate into pc-relative for you)

   sparc_instr(CallAndLink, sparc_reg callee); 
   // actually jmpl [callee+0], %o7

   sparc_instr(JumpAndLink, sparc_reg linkReg,
	       sparc_reg addrReg1, sparc_reg addrReg2);
   sparc_instr(JumpAndLink, sparc_reg linkReg,
	       sparc_reg addrReg, int offset_nbytes);

   sparc_instr(Jump, sparc_reg rs1);
   sparc_instr(Jump, sparc_reg rs1, sparc_reg rs2);
   sparc_instr(Jump, sparc_reg rs1, int offset_nbytes);
 
   sparc_instr(Ret);  // jmpl %i7+8, %g0
   sparc_instr(Retl); // jmpl %o7+8, %g0

   sparc_instr(V9Return, sparc_reg rs1, sparc_reg rs2);
   sparc_instr(V9Return, sparc_reg rs1, simmediate<13> offset);
   
   sparc_instr(Trap, unsigned cond, sparc_reg srcReg1,
	       sparc_reg srcReg2);
   sparc_instr(Trap, unsigned cond, bool xcc,
               sparc_reg srcReg,
	       simmediate<7> constant);
   sparc_instr(Trap, unsigned cond, bool xcc, simmediate<7> constant);
   sparc_instr(Trap, simmediate<7> constant);

   sparc_instr(StoreBarrier);
   sparc_instr(MemBarrier, uimmediate<3> cmask, uimmediate<4> mmask);
   sparc_instr(UnImp, uimmediate<22> constant);

   sparc_instr(Flush, sparc_reg srcReg1, sparc_reg srcReg2);
   sparc_instr(Flush, sparc_reg srcReg, simmediate<13> offset);

   sparc_instr(FAdd, unsigned precision,
               sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
   sparc_instr(FSub, unsigned precision,
               sparc_reg rs1, sparc_reg rs2, sparc_reg rd);

   sparc_instr(FCmp, bool exceptionIfUnordered, unsigned precision,
               unsigned fcc_num, sparc_reg rs1, sparc_reg rs2);

   sparc_instr(Float2Int, unsigned src_reg_precision, bool dest_reg_extended,
               sparc_reg rs2, sparc_reg rd);
      // Both rs2 and rd are float regs (really!)

   sparc_instr(Float2Float, unsigned src_reg_precision, unsigned dest_reg_precision,
               sparc_reg rs2, sparc_reg rd);
      // Both rs2 and rd are float regs

   sparc_instr(Int2Float, bool src_reg_extended, unsigned dest_reg_precision,
               sparc_reg rs2, sparc_reg rd);
      // Both rs2 and rd are float regs (really!)

   sparc_instr(FMov, unsigned precision, sparc_reg rs2, sparc_reg rd);
   sparc_instr(FNeg, unsigned precision, sparc_reg rs2, sparc_reg rd);
   sparc_instr(FAbs, unsigned precision, sparc_reg rs2, sparc_reg rd);
   sparc_instr(FMul, unsigned src_precision, sparc_reg rs1, sparc_reg rs2,
               unsigned dest_precision, sparc_reg rd);
      // dest_precision must be either (1) same as src_precision, or (2) the next
      // higher precision choice.
   sparc_instr(FDiv, unsigned precision, sparc_reg rs1, sparc_reg rs2, sparc_reg rd);
   sparc_instr(FSqrt, unsigned precision, sparc_reg rs2, sparc_reg rd);

   sparc_instr(ReadStateReg, unsigned stateRegNum, sparc_reg dest);
   sparc_instr(WriteStateReg, sparc_reg val, unsigned stateRegNum);
      // we write val xor %g0 to stateReg, which is equivalent to writing val
   sparc_instr(ReadPrivReg, unsigned privRegNum, sparc_reg dest);
   // We use the next one to raise and drop PIL (Processor Interrupt Level)
   sparc_instr(WritePrivReg, sparc_reg src, unsigned privRegNum);

   static pdstring disass_int_cond_code(unsigned);
   static pdstring disass_float_cond_code(unsigned);
   static pdstring disass_reg_cond(unsigned);
   static pdstring disassFloatPrecision(unsigned precision); // 1, 2, or 4

   void getInformation(registerUsage *,    // NULL if you don't want reg usage
                       memUsage *,         // NULL if you don't want mem usage
		       disassemblyFlags *, // NULL if you don't want disassembly
                       controlFlowInfo *,  // NULL if you don't want control flow
                       relocateInfo *      // NULL if you don't want to relocate
		       ) const;

   void getDisassembly(disassemblyFlags &) const;
   pdstring disassemble(kptr_t insnaddr) const; // calls above with usual params
   void getRelocationInfo(relocateInfo &) const;

   //bool isBranch() const;
   bool isCall() const;
      // I know that philosophy dictates that the user always call getControlFlowInfo,
      // I couldn't resist having this one, since it's so much faster, and is used
      // in sparc_bb::doLiveRegisterAnalysis1BlockOnly()
      // NOTE: This will return true for both "call <const-addr>" and "jmpl <addr>, %o7"
   bool isUnanalyzableCall() const;

   // True iff jmpl %reg + offs, %o7. Fills in addrReg and addrOffs
   bool isCallRegPlusOffset(sparc_reg *addrReg, int *addrOffs) const;

   // True iff jmpl %reg1 + %reg2, %o7. Fills in ra1 and rs2
   bool isCallRegPlusReg(sparc_reg *rs1, sparc_reg *rs2) const;

   bool isCallInstr(kaddrdiff_t &offset) const;
      // NOTE: This will only return true for "call <const-addr>", not
      // for "jmpl <addr>, %o7"
   bool isCallToFixedAddress(kptr_t insnAddr, kptr_t &calleeAddr) const;

   bool isBranchToFixedAddress(kptr_t insnAddr, kptr_t &brancheeAddr) const;

   //bool isCmp() const;
   bool isRestore() const;
      // same apologies as above, but we need to check for a restore instr after
      // each call instr, to check for a particularly bizarre optimized sequence.
   bool isRestoreInstr(sparc_reg &rd) const;
   bool isRestoreUsingSimm13Instr(sparc_reg &rs1, // filled in
                                  int &simm13, // filled in
                                  sparc_reg &rd // filled in
                                  ) const;
   bool isRestoreUsing2RegInstr(sparc_reg &rs1, // filled in
                                sparc_reg &rs2, // filled in
                                sparc_reg &rd // filled in
                                ) const;

   bool isV9ReturnInstr() const;

   bool isSPSaveWithFixedOffset(int &offset) const;
   bool isSPSave() const;
   bool isSave() const;
   
   bool isNop() const;
      // same apologies as above.

   bool isLoad() const;
   bool isLoadRegOffset(sparc_reg &rd, // filled in
			sparc_reg &rs1, // filled in
			int &imm13 // filled in
			);
   bool isLoadRegOffsetToDestReg(const sparc_reg &rd,
				 sparc_reg &rs1, // filled in
				 int &imm13 // filled in
				 );
   bool isLoadDualRegToDestReg(const sparc_reg &rd, // must match
                               sparc_reg &rs1, sparc_reg &rs2 // filled in
                               ) const;

   //bool isStore() const;

   bool isSetHi(sparc_reg &rd) const;
   bool isSetHiToDestReg(const sparc_reg &rd, uint32_t &val) const;
      // dest reg must match; val is filled in for you

   bool isBSetLo(const sparc_reg &, uint32_t &val) const;
   bool isOR2RegToDestReg(const sparc_reg &rd, // must match
			  sparc_reg &rs1, // returned for you
			  sparc_reg &rs2  // returned for you
                         ) const;
   bool isORImmToDestReg(const sparc_reg &rd, // must match
                         sparc_reg &rs1, // returned for you
                         unsigned &val // returned for you
                         ) const;
   bool isORImm(sparc_reg &rs1, // returned for you
                unsigned &val, // returned for you (note: we OR to the prev value!!!)
                sparc_reg &rd // filled in for you
                ) const;

   bool isCmp(sparc_reg rs1, int &val) const;
      // rs1 must match; we fill in 'val' for you

   bool isCmpWithValue(sparc_reg rs1, int val) const;

   bool isAndnImmToDestReg(sparc_reg rd, sparc_reg &rs1, int &amt) const;

   bool isBiccCondition(unsigned cond, bool annul) const;
   bool isBicc(unsigned &cond) const;

   bool isBPccCondition(unsigned cond, bool annul, bool xcc) const;
   bool isBPcc(unsigned &cond, bool xcc) const;
   
   bool isMove(const sparc_reg &rd, // must match
               sparc_reg &rs // returned for you
               ) const;

   bool isShiftLeftLogicalToDestReg(const sparc_reg &rd, // must match
                                    sparc_reg &rs1, // returned for you
                                    unsigned &shiftamt // returned for you
                                    ) const;
   bool isShiftRightLogicalToDestReg(sparc_reg rd, // must match
                                     sparc_reg &rs1, // returned for you
                                     unsigned &shiftamt // returned for you
                                     ) const;

   bool isAdd2RegToDestReg(const sparc_reg &rd, // must match
			   sparc_reg &rs1, // filled in
			   sparc_reg &rs2 // filled in
			   ) const;
   bool isAddImmToDestReg(const sparc_reg &rd, // you provide; must match
                          sparc_reg &rs1, // we set this for you
                          unsigned &val // we ADD to this for you
                             // val must already be initialized with some value
                          ) const;
      // NOTE: if true, this routine *adds* to the parameter "val"!

   bool isAddImm(sparc_reg &rs1, // filled in for you
                 unsigned &val, // NOTE: we ADD to this for you!!!!!
                 sparc_reg &rd // filled in for you
                 ) const;

   bool isAddImm(int &val) const;
      // sets val iff true

   bool isSub2RegToDestReg(sparc_reg rd, // must match
                           sparc_reg &rs1, // filled in for you
                           sparc_reg &rs2 // filled in for you
                           );

   bool isRd() const;
   bool isRdPr() const;
   bool isWrPr() const;
   bool isDone() const;
   bool isRetry() const;

   // note: it may also be useful to have a routine which tells under what condition
   // the delay slot is executed.  getDelaySlotInfo() tells us if a delay slot exists
   // and if it's conditional, but that's all.  Slicing, for example, would like to know
   // when a delay slot is annulled (when taken?  when not taken? always? never?
   // getDelaySlotInfo() doesn't differentiate 'when-taken' and 'when-not-taken'.
   // Perhaps it should be altered to do so.)
   delaySlotInfo getDelaySlotInfo() const;

   dependence calcDependence(const instr_t *nextInstr) const;
      // assuming 'this' is executed, followed by 'nextInstr', calculate the
      // dependency.  If there is more than one dependence, the stronger one
      // is returned.  I.e, if a true dependence exists, then return true;
      // else if an anti-dependence exists then return anti;
      // else if an output-dependence exists then return output;
      // else return none;

   // --------------------------------------------------

   instr_t *relocate(kptr_t old_insnaddr, kptr_t new_insnaddr) const;

   static bool inRangeOfBicc(kptr_t from, kptr_t to);
   static bool inRangeOfFBfcc(kptr_t from, kptr_t to);
   static bool inRangeOfBPr(kptr_t from, kptr_t to);
   static bool inRangeOfBPcc(kptr_t from, kptr_t to);
   static bool inRangeOfFBPfcc(kptr_t from, kptr_t to);
   static bool inRangeOfCall(kptr_t from, kptr_t to);

   //

   static void getRangeOfBicc(kptr_t from, kptr_t &min_reachable_result,
                              kptr_t &max_reachable_result);

   static void getRangeOfFBfcc(kptr_t from, kptr_t &min_reachable_result,
                               kptr_t &max_reachable_result);
   
   static void getRangeOfBPr(kptr_t from, kptr_t &min_reachable_result,
                             kptr_t &max_reachable_result);
   
   static void getRangeOfBPcc(kptr_t from, kptr_t &min_reachable_result,
                              kptr_t &max_reachable_result);
   
   static void getRangeOfFBPfcc(kptr_t from, kptr_t &min_reachable_result,
                                kptr_t &max_reachable_result);

   void getRangeOfCall(kptr_t from, kptr_t &min_reachable_result,
		       kptr_t &max_reachable_result);

   // --------------------------------------------------------------

   void changeBranchOffset(int new_insnbytes_offset);
      // must be a conditional branch instruction.  Changes the offset.
   void changeBranchOffsetAndAnnulBit(const sparc_cfi &,
                                      int new_offset, bool new_annul);
   void changeBranchOffsetAndAnnulBit(int new_offset, bool new_annul);

   void reverseBranchPolarity(const sparc_cfi &old_cfi);
   void reverseBranchPolarity();

   void changeBranchPredictBitIfApplicable(const sparc_cfi &,
                                           bool newPredict);
   void changeBranchPredictBitIfApplicable(bool newPredict);


   // We need to transfer src in the previous register window to ours
   // A couple of cases are obvious, but the others require some code to be
   // generated
   static sparc_reg recoverFromPreviousFrame(sparc_reg src, sparc_reg scratch,
					     unsigned stackBias, pdvector<instr_t*> *seq);

   // Generate a sequence of instructions equivalent to this one, but
   // that writes to rv_reg instead of this instruction's dest. If
   // hasExtraSave is true -- the source registers need to be recovered
   // from the previous stack frame
   void changeDestRegister(bool hasExtraSave, sparc_reg rv, sparc_reg scratch,
			   unsigned stackBias, pdvector<instr_t*> *equiv) const;

   static sparc_instr
   cfi2ConditionalMove(const sparc_cfi &, // describes orig insn
                       sparc_reg srcReg, // if true, move this...
                       sparc_reg destReg // ...onto this
                       );
      // returns a conditional move instruction that moves srcReg onto destReg iff
      // the branch would be taken.  Works for any conditional branch (bicc, bpcc,
      // bpr, fbfcc, fbpfcc).  Probably works for unconditional branches (ba, bn)
      // too.

   void changeSimm13(int new_val);

   // --------------------------------------------------------------

   static void gen32imm(insnVec_t *piv, uint32_t val, const sparc_reg &rd);

   static void gen64imm(insnVec_t *piv, uint64_t val, const sparc_reg &rd, 
			const sparc_reg &rtemp);

   // Set the register to the given 32-bit address value
   // Try to produce the shortest code, depending on the address value
   static void genImmAddr(insnVec_t *piv, uint32_t addr, const sparc_reg &rd);

   // Set the register to the given 64-bit address value
   // Try to produce the shortest code, depending on the address value
   static void genImmAddr(insnVec_t *piv, uint64_t addr, const sparc_reg &rd);

   // The next two functions set the register to the given address
   // value, but do not attempt to produce smaller code, since we may patch
   // it with a different address later. Do not set the lowest 12 bits of
   // the address if the caller requests so.
   static void genImmAddrGeneric(insnVec_t *piv, uint32_t addr, 
				 const sparc_reg &rd, bool setLow = true); 
   static void genImmAddrGeneric(insnVec_t *piv, uint64_t addr, 
				 const sparc_reg &rd, bool setLow = true);

   // Generate "genImmAddr(hi(to)) -> link; jmpl [link + lo(to)], link"
   // Note: we require link != sparc_reg::g0, since we need a scratch 
   // register for sure
   static void genJumpAndLink(insnVec_t *piv, kptr_t to, 
			      sparc_reg &linkReg);

   // Generate either "call disp" or
   // "genImmAddr(hi(to)) -> o7, jmpl o7+lo(to),o7" if the displacement
   // is not in the range for the call instruction
   static void genCallAndLink(insnVec_t *piv, kptr_t from, kptr_t to);

   // Same goal, but do not optimize produced code for size. However,
   // we still want the 32-bit version to emit one call instruction -- hence
   // two overloaded functions
   static void genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, 
                                                     uint32_t to);
   static void genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, 
                                                     uint64_t to);

   static void genCallAndLinkGeneric_unknownFromAndToAddr(insnVec_t *piv);
};

#endif
