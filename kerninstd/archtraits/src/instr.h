// instr.h

#ifndef _INSTR_H_
#define _INSTR_H_

#include "common/h/String.h"
#include "util/h/kdrvtypes.h"

class insnVec_t;
#include "reg.h"
#include "reg_set.h"
#include "bitUtils.h"
#include "util/h/xdr_send_recv.h"

#include "simm.h" // signed immediate bits
#include "uimm.h" // unsigned immediate bits

class instr_t {
 public:
   class unimplinsn {};
   class unknowninsn {
    private:
      kptr_t theaddr;
    public:
      unknowninsn() : theaddr(0) {}
      unknowninsn(kptr_t addr) : theaddr(addr) {}
      kptr_t getAddr() { return theaddr; }
   };

   void dothrow_unimplinsn() const { throw unimplinsn(); }
   void dothrow_unknowninsn() const { throw unknowninsn(); }

   struct registerUsage {
      regset_t *definitelyUsedBeforeWritten;
      regset_t *maybeUsedBeforeWritten;
      regset_t *definitelyWritten;
      regset_t *maybeWritten;
      registerUsage();
     ~registerUsage();
   };

   class address {
    public:
      enum addrtype {
	addrUndefined, /* type is currently unknown */
	absoluteAddr, /* addr = offset */ 
	relativeOffset, /* addr = curr addr + offset */
	singleReg, /* addr = reg1 */
	singleRegPlusOffset, /* addr = reg1 + offset */
	scaledSingleRegPlusOffset, /* addr = reg2<<scale + offset */
	doubleReg, /* addr = reg1 + reg2 */
	doubleRegPlusOffset, /* addr = reg1 + reg2 + ofsset */
	scaledDoubleRegPlusOffset /* addr = reg1 + reg2<<scale + offset */
      };
      enum equality {definitelyEqual, definitelyNotEqual, unsureEqual};

    private:
      addrtype thetype;
      reg_t *reg1, *reg2;
      int32_t offset;
      int32_t scale; // think x86

    public:
      address() : thetype(addrUndefined), reg1(NULL), reg2(NULL),
	offset(0), scale(0) {}
      address(addrtype itype, reg_t *r1, reg_t *r2,
              int32_t ioffset, int32_t iscale);
      address(addrtype itype, reg_t *r1,
              signed int ioffset);
      address(addrtype itype, reg_t *r1, reg_t *r2);
      address(const address &src);
      virtual ~address(); //deallocate memory for registers if needed
      address &operator=(const address &src);
      equality equality_check(const address &other) const;
      addrtype getType() const { return thetype; }
      reg_t *getReg1() const { return reg1; }
      reg_t *getReg2() const { return reg2; }
      int32_t getOffset() const { return offset; }
      int32_t getScale() const { return scale; }
   };

   struct memUsage {
      bool memRead;
      address addrRead;

      bool memWritten;
      address addrWritten;

      memUsage() {memRead = false; memWritten=false;}
      memUsage(bool) {} // specifically intended to leave things undefined
   };

   struct disassemblyFlags {
      pdstring result;
      kptr_t pc; // needed when displaying non-pc-relative

      pdstring (*disassemble_destfuncaddr)(kptr_t addr);
         // obtains destination function name when given its address; used for
         // disassembling e.g. call instructions. If NULL, we won't try to.
      pdstring destfunc_result;
         // filled in only if the instruction is a call; it's up to the user to
         // decide what if anything to do with the string.

      disassemblyFlags() {
	 pc = 0;
	 disassemble_destfuncaddr = NULL;
      }
      disassemblyFlags(kptr_t ipc) : pc(ipc) {
         disassemble_destfuncaddr = NULL;
      }
     ~disassemblyFlags() {}
   };

   struct relocateInfo {
      kptr_t old_insnaddr;
      kptr_t new_insnaddr;
      bool success; // relocation can fail! (e.g. a instr moved so far that
                    // the displacement no longer fits in 22 bits)
      uint32_t result;
         // would prefer instr_t but at this point, it's an incomplete type.
         // on failure, this is left undefined.
   };

   enum delaySlotInfo {dsNone, dsAlways, dsNever, 
		       dsWhenTaken /*, dsWhenNotTaken*/};
      // dsNone --> no delay slot (non-DCTIs plus ba, a)
      // dsAlways --> delay slot exists and is always executed
      //              (b<any> when not annulled, even ba and bn)
      // dsNever --> delay slot exists but is never exec'd (bn, a) so it could
      //             even be data
      // dsWhenTaken --> delay slot exists and is executed only when the
      //                 branch is taken (b<cond>, a)
      // dsWhenNotTaken -> delay slot exists and is executed only when the
      //                   branch is not taken.  Since we haven't found any 
      //                   examples of this, dsWhenNotTaken doesn't yet exist.

   // note: it may also be useful to have a routine which tells under 
   // what condition the delay slot is executed.  getDelaySlotInfo() 
   // tells us if a delay slot exists and if it's conditional, but 
   // that's all.  Slicing, for example, would like to know when a 
   // delay slot is annulled (when taken?  when not taken? always? never?
   // getDelaySlotInfo() doesn't differentiate between taken & not taken
   // Perhaps it should be altered to do so.)
   virtual delaySlotInfo getDelaySlotInfo() const = 0;

   struct controlFlowInfo {
      // 1) What is being tested: condition code and comparison register
      /* I went with a simple enum to determine int vs. float vs. reg
	 and a reg_t * for specifying a comparison register (for sparc BPr) 
	 or the register holding the condition code (in case it's not
	 implicit). */
      enum { intcc, floatcc, regcc } cctype;
      reg_t *conditionReg;

      // 2) The condition that the above is being tested against.
      /* I went with a single unsigned int, since previously we 
	 had a union of an intcc, floatcc, and regcc, where each
	 type of cc was an enumerated type, and thus representable
	 by an integer. This way, the enumerated types can still be
	 defined in the arch-specific headers and then assigned to
	 this variable as required. */
      unsigned condition;

      // 3) The specific kind of control flow instruction, along with 
      //    predict & annul bits.
      /* The following info is all that should be needed by arch-
	 independent code. The controlflowinfo struct can be inherited
	 from in the arch-dependent headers where additional info can
	 be added to an arch-specific controlflowinfo struct. */
      union {
	 unsigned bit_stuff;
	 struct {
	    unsigned controlFlowInstruction : 1;
	    unsigned isCall : 1;
	    unsigned isJmpLink : 1;
	    unsigned isRet : 1;
	    unsigned isBranch : 1;
	    unsigned isConditional : 1; // a condition is checked
	    unsigned isAlways : 1; // no condition is checked
	 } fields;
      };

      // 4) Delay slot information:
      delaySlotInfo delaySlot;

      // 5) Destination information
      enum desttype {dstUndefined, pcRelative, regRelative, doubleReg, 
		     memIndirect, absolute};
         // pcRelative: dest addr is [pc + offset_nbytes]
         // regRelative: dest addr is [reg1 + offset_nbytes]
         // doubleReg: dest addr is [reg1 + reg2]
         // memIndirect: dest contained in memaddr
         // absolute: dest is 0 + offset_nbytes
      desttype destination;
      reg_t *destreg1, *destreg2; // reg2 can be used in jmpl and return instrs
      signed offset_nbytes;
         // offset from reg1 if destination==regRelative,
         // offset from pc if destination==pcRelative,
         // undefined if destination==doubleReg
      address memaddr; // for memIndirect

      controlFlowInfo();
     ~controlFlowInfo();
   };

 protected:
   uint32_t raw; 
   // first four bytes of raw instruction; on platforms with 
   // instructions that can be/are longer than 4 bytes, this field
   // will be ignored

 public:
   instr_t() : raw(0) {}
   instr_t(uint32_t iraw) : raw(iraw) {}
   instr_t(const instr_t *i) : raw(i->raw) {}
   instr_t(const instr_t &i) : raw(i.raw) {}
   virtual ~instr_t() {}

   //WARNING: the getInstr method returns a const reference to a new
   //         instr_t, so callers are responsible for destruction
   static const instr_t& getInstr(uint32_t);
   static instr_t* getInstr(const instr_t &);
   static void putInstr(const instr_t *, void *);
   static void putInstr(XDR *xdr, void *where);

   instr_t& operator=(const instr_t &i) {
     raw = i.raw;
     return *this;
   }

   virtual bool send(XDR *xdr) const = 0;

   virtual unsigned getNumBytes() const { return 4; }
   uint32_t getRaw() const { return raw; }

   // for direct emitting
   virtual void copyRawBytes(char *to_addr) const {
      // assumes there is enough space allocated at addr, use cautiously
      *(uint32_t*)to_addr = raw;
   }
   
   // get the value of bits of the raw instr, the default
   // implementations will only work for fixed 4-byte instr architectures
   virtual uint32_t getRawBits(unsigned hi_bit, unsigned lo_bit) const {
     return getBits(raw, hi_bit, lo_bit);
   }
   virtual uint32_t getRawBits0(unsigned hi_bit) const {
     return getBitsFrom0(raw, hi_bit);
   }
   virtual bool getRawBit(unsigned bit) const {
      const unsigned temp = raw >> bit;
      return ((temp & 0x1) != 0);
   }

   // characteristics of all instructions for the current arch
   static bool hasUniformInstructionLength();
   static bool condBranchesUseDelaySlot();

   // retrieve the requested information for this instr
   virtual void getInformation(registerUsage *,
			       memUsage *,
			       disassemblyFlags *,
			       controlFlowInfo *,
			       relocateInfo *) const = 0;
      // each pointer can be NULL, meaning you don't want that info
   void getRegisterUsage(registerUsage *ru) const {
      getInformation(ru, NULL, NULL, NULL, NULL);
   }
   void getControlFlowInfo(controlFlowInfo *cfi) const {
      getInformation(NULL, NULL, NULL, cfi, NULL);
   }
   virtual void getDisassembly(disassemblyFlags &) const = 0;
   virtual pdstring disassemble(kptr_t insnaddr) const = 0;
   virtual void getRelocationInfo(relocateInfo &) const = 0;

   // characteristics of this instruction
   static bool instructionHasValidAlignment(kptr_t addr);
   //virtual bool isBranch() const = 0;
   virtual bool isCall() const = 0;
   virtual bool isUnanalyzableCall() const = 0;
   //virtual bool isCmp() const = 0;
   virtual bool isNop() const = 0;
   virtual bool isRestore() const = 0;
   virtual bool isSave() const = 0;
   virtual bool isLoad() const = 0;
   //virtual bool isStore() const = 0;

   virtual bool isBranchToFixedAddress(kptr_t insnAddr, 
				       kptr_t &brancheeAddr) const = 0;
   virtual bool isCallInstr(kaddrdiff_t &offset) const = 0;
   virtual bool isCallToFixedAddress(kptr_t insnAddr,
				     kptr_t &calleeAddr) const = 0;

   enum dependence {deptrue, depanti, depoutput, depnone};
   virtual dependence calcDependence(const instr_t *nextInstr) const = 0;
      // assuming 'this' is executed, followed by 'nextInstr', calculate the
      // dependency.  If there is more than one dependence, the stronger one
      // is returned.  I.e, if a true dependence exists, then return true;
      // else if an anti-dependence exists then return anti;
      // else if an output-dependence exists then return output;
      // else return none;

   virtual instr_t *relocate(kptr_t old_insnaddr, 
			     kptr_t new_insnaddr) const = 0;

   /*  virtual void getRangeOfCall(kptr_t from, kptr_t &min_reachable_result, */
/*  			       kptr_t &max_reachable_result) = 0; */
   /*  virtual void getRangeOf(kptr_t from, unsigned numBitsOfSignedDisplacement, */
/*  			   kptr_t &min_reachable_result, */
/*  			   kptr_t &max_reachable_result) = 0; */

   virtual void changeBranchOffset(int new_insnbytes_offset) = 0;
      // must be a conditional branch instruction.  Changes the offset.

   static void gen32imm(insnVec_t *piv, uint32_t val, const reg_t &rd);
   static void genImmAddr(insnVec_t *piv, uint32_t addr, const reg_t &rd);
   static void genImmAddr(insnVec_t *piv, uint64_t addr, const reg_t &rd);
   static void genCallAndLink(insnVec_t *piv, kptr_t from, kptr_t to);
   static void genCallAndLinkGeneric_unknownFromAndToAddr(insnVec_t *piv);
   static void genCallAndLinkGeneric_unknownFromAddr(insnVec_t *piv, kptr_t to);
   static void gen64imm(insnVec_t *piv, uint64_t val, const reg_t &rd, 
                        const reg_t &rtemp);
   static void genImmAddrGeneric(insnVec_t *piv, uint32_t addr, 
                                 const reg_t &rd, bool setLow = true); 
   static void genImmAddrGeneric(insnVec_t *piv, uint64_t addr, 
                                 const reg_t &rd, bool setLow = true);
   static void getRangeOf(kptr_t from, unsigned numBitsOfSignedDisplacement,
			  kptr_t &min_reachable_result,
			  kptr_t &max_reachable_result); 
   static bool inRangeOf(kptr_t from, kptr_t to,
			 unsigned numBitsOfSignedDisplacement);
};

#endif /*_ARCH_INSTR_H_*/
