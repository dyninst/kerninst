// x86_instr.h
#ifndef _X86_INSTR_H
#define _X86_INSTR_H

#include <assert.h>
#include <stdio.h>
#include <string.h> // memcpy
#include <new>

#include "common/h/String.h"
#include "util/h/kdrvtypes.h"

#include "instr.h"
#include "x86_parse.h"
class x86_reg;

class x86_instr : public instr_t {
 public:
   // enums for representing various differences between insns
   enum CondCodes { Overflow=0x00, NoOverflow=0x01, Below=0x02, NotBelow=0x03,
                    Equal=0x04, NotEqual=0x05, NotAbove=0x06, Above=0x07,
                    Sign=0x08, NotSign=0x09, Parity=0x0A, NotParity=0x0B,
                    LessThan=0x0C, GreaterThanEqual=0x0D, LessThanEqual=0x0E,
                    GreaterThan=0x0F };

   // class definitions for creating instructions
   class Adc {};
   class Add {};
   class And {};
   class Call {};
   class Cli {};
   class Cmov {};
   class Cmp {};
   class CmpXChg {};
   class CmpXChg8b {};
   class Div {};
   class Int3 {};
   class Jcc {};
   class Jmp {};
   class Leave {};
   class Load {};
   class Neg {};
   class Nop {};
   class Not {};
   class Mov {};
   class Mult {};
   class Or {};
   class Pop {};
   class PopAD {};
   class PopFD {};
   class Push {};
   class PushAD {};
   class PushFD {};
   class Rdpmc {};
   class Rdtsc {};
   class Ret {};
   class SetCC {};
   class SHL {};
   class SHR {};
   class SHLd {};
   class SHRd {};
   class Sbb {};
   class Sub {};
   class Sti {};
   class Store {};
   class Test {};
   class Xor {};
   
   // The following static member vrbles are meant to be passed as the first
   // param to the constructors of this class...making them each of a different
   // type (instead of an enum) really beefs up type checking nicely.
   static Adc adc;
   static Add add;
   static And andL;
   static Call call;
   static Cli cli;
   static Cmov cmovcc;
   static Cmp cmp;
   static CmpXChg cmpxchg;
   static CmpXChg8b cmpxchg8b;
   static Div idiv;
   static Int3 int3;
   static Jcc jcc;
   static Jmp jmp;
   static Leave leave;
   static Load load;
   static Mov mov;
   static Mult imul;
   static Neg neg;
   static Nop nop;
   static Not notL;
   static Or orL;
   static Pop pop;
   static PopAD popad;
   static PopFD popfd;
   static Push push;
   static PushAD pushad;
   static PushFD pushfd;
   static Rdpmc rdpmc;
   static Rdtsc rdtsc;
   static Ret ret;
   static SetCC setcc;
   static SHL shl;
   static SHR shr;
   static SHLd shld;
   static SHRd shrd;
   static Sti sti;
   static Store store;
   static Sbb sbb;
   static Sub sub;
   static Test test;
   static Xor xorL;

 private:
   char raw_bytes[16];
   unsigned num_bytes; // how many instruction bytes we have

   void print_raw_bytes() const;

 public:
   struct x86_cfi : public controlFlowInfo {
      union {
	 unsigned x86_bit_stuff;
	 struct {
	    unsigned isInt : 1;
	    unsigned isFarJmp : 1;
	    unsigned isFarRet : 1;
	    unsigned isUd2 : 1;
	 } x86_fields;
      };
      unsigned short farSegment; // the segment portion of a far address,
                                 // combined with offset_nbytes
      x86_cfi() : controlFlowInfo(), x86_bit_stuff(0), farSegment(0) {}
     ~x86_cfi() {}
   };

   void* operator new(size_t);
   void* operator new(size_t, void *place) {return place;}
   void operator delete(void*);

   x86_instr(const x86_instr &src) : instr_t() {
      num_bytes = src.num_bytes;
      memcpy(&raw_bytes[0], &src.raw_bytes[0], num_bytes);
   }
   // For an unparsed instruction (ctor determines its length).
   x86_instr(const char *addr);

   x86_instr(XDR* xdr);

   ~x86_instr() {}

   // For code generation, a bunch of direct constructors for various insns
   x86_instr(Adc, uint32_t imm_val, const x86_reg &src_dst);
   x86_instr(Adc, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(Add, uint32_t imm_val, const x86_reg &src_dst);
   x86_instr(Add, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(And, uint32_t imm_val, const x86_reg &src_dst);
   x86_instr(And, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(Call, kaddrdiff_t displacement);
   x86_instr(Call, const x86_reg &src);
   x86_instr(Cli);
   x86_instr(Cmov, CondCodes code, const x86_reg &src, const x86_reg &dst);
   x86_instr(Cmp, uint32_t imm_val, const x86_reg &src);
   x86_instr(Cmp, const x86_reg &src1, const x86_reg &src2);
   x86_instr(CmpXChg, const x86_reg &addr_reg, const x86_reg &new_val);
   x86_instr(CmpXChg8b, const x86_reg &addr_reg);
   x86_instr(Div, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(Int3);
   x86_instr(Jcc, CondCodes code, int32_t displacement);
   x86_instr(Jmp, int16_t displacement);
   x86_instr(Jmp, int32_t displacement);
   x86_instr(Leave);
   x86_instr(Load, const x86_reg &addr, const x86_reg &dst);
   x86_instr(Load, signed char disp8, const x86_reg &addr, const x86_reg &dst);
   x86_instr(Load, int32_t disp32, const x86_reg &addr, const x86_reg &dst);
   x86_instr(Mov, uint32_t val, const x86_reg &dst);
   x86_instr(Mov, const x86_reg &src, const x86_reg &dst);
   x86_instr(Mult, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(Mult, const x86_reg &src, uint32_t imm_val, const x86_reg &dst);
   x86_instr(Neg, const x86_reg &src_dst);
   x86_instr(Nop);
   x86_instr(Not, const x86_reg &src_dst);
   x86_instr(Or, uint32_t imm_val, const x86_reg &src_dst);
   x86_instr(Or, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(Pop, const x86_reg &dst);
   x86_instr(PopAD);
   x86_instr(PopFD);
   x86_instr(Push, uint32_t val);
   x86_instr(Push, const x86_reg &src);
   x86_instr(PushAD);
   x86_instr(PushFD);
   x86_instr(Rdpmc);
   x86_instr(Rdtsc);
   x86_instr(Ret);
   x86_instr(SetCC, CondCodes code, const x86_reg &dst);
   x86_instr(SHLd, uint32_t imm_val, const x86_reg &src, const x86_reg &dst);
   x86_instr(SHL, uint32_t imm_val, const x86_reg &src_dst);
   x86_instr(SHL, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(SHRd, uint32_t imm_val, const x86_reg &src, const x86_reg &dst);
   x86_instr(SHR, uint32_t imm_val, const x86_reg &src_dst);
   x86_instr(SHR, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(Sbb, uint32_t imm_val, const x86_reg &src_dst);
   x86_instr(Sbb, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(Sub, uint32_t imm_val, const x86_reg &src_dst);
   x86_instr(Sub, const x86_reg &src2, const x86_reg &src_dst);
   x86_instr(Sti);
   x86_instr(Store, uint32_t imm_val, const x86_reg &addr);
   x86_instr(Store, const x86_reg &val, const x86_reg &addr);
   x86_instr(Store, const x86_reg &val, int32_t disp32, const x86_reg &addr);
   x86_instr(Test, uint32_t imm_val, const x86_reg &src);
   x86_instr(Xor, uint32_t imm_val, const x86_reg &src_dst);
   
   x86_instr& operator=(const x86_instr &src) {
      num_bytes = src.num_bytes;      
      memcpy(&raw_bytes[0], &src.raw_bytes[0], src.num_bytes);
      return *this;
   }
   bool operator==(const x86_instr &src) const {
     return (num_bytes == src.num_bytes) && 
            (memcmp(&raw_bytes[0], &src.raw_bytes[0], num_bytes) == 0);
   }

   bool send(XDR *xdr) const;

   // for direct emitting
   void copyRawBytes(char *to_addr) const {
      // assumes there is enough space allocated at addr, use cautiously
      memcpy(to_addr, &raw_bytes[0], num_bytes);
   }

   unsigned getNumBytes() const { return num_bytes; }
   const char* getRawBytes() const { return &raw_bytes[0]; }
   uint32_t getRawBits(unsigned /*hi_bit*/, unsigned /*lo_bit*/) const {
      assert(0);
      return 0; // need to define if we ever use
   }
   uint32_t getRawBits0(unsigned /*hi_bit*/) const {
      assert(0);
      return 0; // need to define if we ever use
   }

   // characteristics of all instructions for the current arch
   static bool hasUniformInstructionLength() {
      return false;
   }
   static bool condBranchesUseDelaySlot() {
      return false;
   }
   instr_t::delaySlotInfo getDelaySlotInfo() const {
      return instr_t::dsNone;
   }

   // retrieve the requested information for this instr
   void getInformation(registerUsage *,
		       memUsage *,
		       disassemblyFlags *,
		       controlFlowInfo *,
		       relocateInfo *) const;
   void getDisassembly(disassemblyFlags &) const;
   pdstring disassemble(kptr_t insnaddr) const {
      disassemblyFlags df(insnaddr);
      getDisassembly(df);
      return df.result;
   }
   void getRelocationInfo(relocateInfo &) const;
 private:
   void updateControlFlowInfo(x86_instr::x86_cfi *cfi, unsigned insn_type,
                              const ia32_instruction &insn) const;
   void updateRegisterUsage(instr_t::registerUsage *ru, unsigned insn_type,
                            const ia32_instruction &insn) const;
   void updateDisassemblyFlags(disassemblyFlags &df, unsigned insn_type,
                               const ia32_instruction &insn) const;

 public:
   // characteristics of this instruction
   static bool instructionHasValidAlignment(kptr_t) {
      return true; // An x86 insn can begin at any address
   }
   //bool isBranch() const;
   bool isCall() const;
   bool isUnanalyzableCall() const;
   bool isCmp() const;
   bool isNop() const;
   bool isRestore() const;
   bool isSave() const;
   bool isLoad() const { return false; }
   //bool isStore() const { return false; }
   bool isMov() const;

   bool isBranchToFixedAddress(kptr_t insnAddr, 
			       kptr_t &brancheeAddr) const;
   bool isCallInstr(kaddrdiff_t &offset) const;
   bool isCallToFixedAddress(kptr_t insnAddr,
			     kptr_t &calleeAddr) const;

   dependence calcDependence(const instr_t *nextInstr) const;
      // assuming 'this' is executed, followed by 'nextInstr', calculate the
      // dependency.  If there is more than one dependence, the stronger one
      // is returned.  I.e, if a true dependence exists, then return true;
      // else if an anti-dependence exists then return anti;
      // else if an output-dependence exists then return output;
      // else return none;

   instr_t *relocate(kptr_t old_insnaddr, kptr_t new_insnaddr) const;

   void changeBranchOffset(int new_insnbytes_offset);
      // actually works for jmp, jcc, and call.  Changes the offset.

   static unsigned char getRegRMVal(const x86_reg &reg);
      // returns appropriate ModRM 'reg' or 'rm' field value for reg

   static void gen32imm(insnVec_t *piv, uint32_t val, const x86_reg &rd);
   static void gen64imm(insnVec_t *piv, uint64_t val, 
                        const x86_reg &rd, const x86_reg &rtemp);
   static void genImmAddrGeneric(insnVec_t *piv, uint32_t addr, 
                                 const x86_reg &rd, bool setLow);
   static void genImmAddrGeneric(insnVec_t *piv, uint64_t addr, 
                                 const x86_reg &rd, bool setLow);

   static void genCallAndLink(insnVec_t *piv, kptr_t from, kptr_t to);
   static void genCallAndLinkGeneric_unknownFromAndToAddr(insnVec_t *piv);
   static void genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, kptr_t to);
   static unsigned numInsnsInRange(kptr_t startaddr, kptr_t endaddr);
};

#endif /* _X86_INSTR_H */
