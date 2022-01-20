// tempBufferEmitter.h
// Can contain code and/or data.  Use this class in conjunction with
// class relocatableCode; in fact, it might be nice to combine the two
// classes some day (so that there's just a relocatableCode that can
// be emitted to instead of initialized en masse).
//
// tempBufferEmitters contain at least one "chunk".  Any one chunk contains contiguous
// but relocatable code.  With several chunks, since any one chunk is relocatable,
// we have the ability to emit what will be disjoint code (essential for outlining),
// and to emit data separate from code (useful for jump table stuff before the
// body of a function...two separate chunks in that case).

#ifndef _TEMP_BUFFER_EMITTER_H_
#define _TEMP_BUFFER_EMITTER_H_

#include <inttypes.h> // uint32_t, etc.
#include "common/h/Vector.h"
#include "insnVec.h"
#include "simm.h"
#include "util/h/Dictionary.h"
#include "common/h/triple.h"
#include "util/h/kdrvtypes.h"
class abi; // fwd decl avoids potentially expensive #include
#include <utility> // STL's pair

#include "reg.h" // change to reg.h if class is made arch indep

class tempBufferEmitter {
 public:
   struct insnOffset {
      unsigned chunkNdx;
      unsigned byteOffset; // offset within chunk
      insnOffset(unsigned ichunkNdx, unsigned ibyteOffset) :
         chunkNdx(ichunkNdx), byteOffset(ibyteOffset) {
      }
      int operator-(const insnOffset &other) const {
         assert(chunkNdx == other.chunkNdx); // otherwise we wouldn't know the offset
         return byteOffset - other.byteOffset;
      }
   };
   
 protected:
   pdvector<insnVec_t*> theChunks;
   unsigned currChunkNdx; // ndx into the above vector

   bool completed;

   const abi &theABI;
   
   dictionary_hash<pdstring, insnOffset> labels;
      // key: label name
   dictionary_hash<pdstring, kptr_t> knownSymAddrs;
      // It's rare for this dictionary to be anything but empty, but occasionally,
      // makeSymAddrKnown() gets called.
      // Entries in this dictionary can be used to resolve unresolvedSymAddrs[],
      // unresolvedCallsByName[], and even unresolvedBranchesToLabels[] (interproc
      // branch).

   pdvector< std::pair<insnOffset, pdstring> > unresolvedBranchesToLabels;
      // .second: label name
      // Note that due to cross-chunk branches, it's possible
      // that not all of these can be resolved in this class.
   pdvector< std::pair<insnOffset, pdstring> > unresolvedInterprocBranchesToName;
      // .second: "callee"/"branchee" dest name.  NOT a label name.  It's a symAddr32
      // name.  To resolve, call makeSymAddr32Known().
   pdvector< std::pair<insnOffset, kptr_t> > unresolvedInterprocBranchesToAddr;
      // .second: "branchee"'s address.  Can't be resolved in this class since we don't
      // yet know instruction addresses.
   pdvector< std::pair<insnOffset, pdstring> > unresolvedCallsByName;
      // .second: callee name (symAddr or (rare:) label name)
   pdvector< std::pair<insnOffset, kptr_t> > unresolvedCallsByAddr;
      // .second: callee's absolute address
   pdvector< std::pair<insnOffset, pdstring> > unresolvedSymAddrs;
      // .second: symbol name, assumed to reside at a 32-bit address.

   pdvector< std::pair<insnOffset, pdstring> > unresolvedLabelAddrsAsData;
   pdvector< std::pair<insnOffset, triple<pdstring, pdstring, unsigned> > > unresolvedLabelAddrMinusLabelAddrThenDivides;
      // second: a triple: label name, label name [subtract this], divisor
   pdvector< std::pair<insnOffset, pdstring> > unresolvedLabelAddrs;
      // .second: label name.

   void emitCallGeneric_unknownFromAddr(kptr_t to);
     // Emit the worst-case sequence of instructions, with a dummy call
     // displacement.
     // 32-bit version: emit "call disp"
     // 64-bit version: emit "genImmAddr(hi(to)) -> o7; jmpl o7+lo(to),o7"
     // as usual, we don't know fromAddr (we're emitting *relocatable* code)
   void emitCallGeneric_unknownFromAndToAddr();

   // Prevent copying:
   tempBufferEmitter(const tempBufferEmitter &);
   tempBufferEmitter &operator=(const tempBufferEmitter &);

 public:
   virtual bool isReadyToExecute() const = 0;
   tempBufferEmitter(const abi &);
      // we start off with a single empty chunk; no need for
      // a beginNewChunk() to start off, nor for a switchToChunk(0) to start off.
   virtual ~tempBufferEmitter();

   // The emit()-type routines below emit into the "current chunk", which can
   // be changed with the following call:
   void switchToChunk(unsigned chunk_ndx);
   unsigned getCurrChunkNdx() const { return currChunkNdx; }
      // useful for later calls to switchToChunk()
   insnVec_t* getCurrChunk() const { return theChunks[currChunkNdx]; }
   insnVec_t* getChunk(unsigned chunkNdx) const {
      return theChunks[chunkNdx];
   }
   unsigned numChunks() const { return theChunks.size(); }

   insnOffset currInsnOffset() const {
      const unsigned curr_chunk_ndx = getCurrChunkNdx();
      return insnOffset(curr_chunk_ndx, theChunks[curr_chunk_ndx]->numInsnBytes());
   }
   
   void beginNewChunk();
      // begins a blank new chunk, and effectively calls switchToChunk() on it

   virtual void reset()=0;

   const pdvector<insnVec_t*>& getEmitted(bool mustBeReadyToExecute) const;
   insnVec_t* get1EmittedAsInsnVec(bool mustBeReadyToExecute) const;
      // asserts only one chunk
      
   // --------------------------------------------------------------------------------
   // ----------------------------- Emitting Routines --------------------------------
   // --------------------------------------------------------------------------------

   // Reminder: the following emit-type routines emit code/labels/whatever in the
   // current chunk, which is changed with a call to switchToChunk() or beginNewChunk().
   void emit(instr_t *i);
   void emitSequence(insnVec_t *iv);
   void emitLabel(const pdstring &labelName);
   void emitCondBranchToForwardLabel(instr_t *i, const pdstring &fwdLabelName);
      // conditional branches backwards need not be concerned with labels, and emit()
      // can be used in those cases.
      // Supports all kinds of conditional branches, including bprs.
   void emitCondBranchToLabel(instr_t *i, const pdstring &labelName);
      // like emitCondBranchToForwardLabel(), but we won't assert, upon resolving,
      // that the label was a forward one.

   void emitInterprocCondBranch(instr_t *i, const pdstring &symName);
      // (Use makeSymAddrKnown() on symName to resolve.)
   void emitInterprocCondBranch(instr_t *i, kptr_t destAddr);
      // since branches are inherently pc-dependent, there will be some resolving left
      // to do.

   void emitCall(kptr_t from, kptr_t to);
      // Emit either "call disp" or
      // "genImmAddr(hi(to)) -> o7; jmpl o7+lo(to),o7" 
      // if the displacement doesn't fit in 30 bits

   void emitCall(const pdstring &calleeName);
      // can be a symbol address resolved much later, a symAddr() added here
      // (see makeSymAddrKnown() below), or even a label (rare but useful on
      // occasion).
   void emitCall(kptr_t calleeAddr);
      // since calls are inherently pc-dependent instructions, there will
      // still be some resolving left to do.
   void emitSymAddr(const pdstring &symbolName, reg_t &rd);
      // The vast majority of the time, "symbolName" won't be known/resolved
      // until way after complete() is called.  But sometimes, the symbol becomes
      // known, and we can call makeSymAddrKnown() before complete().  Very unusual
      // to do so, however.

   void makeSymAddrKnown(const pdstring &symbolName, kptr_t value);
      // Resolves any earlier emitSymAddr()'s or emitDiffFromSymAddr()'s
      // that used symbolName.
   void emitImm32(uint32_t num, reg_t &rd);
   void emitImm64(uint64_t num, reg_t &scratchreg, reg_t &rd);

   // Emits code for loading an immediate address value into a register
   void emitImmAddr(uint32_t addr, reg_t &rd);
   void emitImmAddr(uint64_t addr, reg_t &rd);

   // emits the ld or ldx operation depending on the native pointer type
   virtual void emitLoadKernelNative(reg_t &srcAddrReg, signed offset,  
				     reg_t &rd) = 0;
   virtual void emitLoadDaemonNative(reg_t &srcAddrReg, signed offset,
				     reg_t &rd) = 0;

   // emits the ld or ldx operation depending on the native pointer type
   virtual void emitLoadKernelNative(reg_t &srcAddrReg1, reg_t &srcAddrReg2, 
				     reg_t &rd) = 0;
   virtual void emitLoadDaemonNative(reg_t &srcAddrReg1, reg_t &srcAddrReg2, 
				     reg_t &rd) = 0;

   // emits the st or stx operation depending on the native pointer type
   virtual void emitStoreKernelNative(reg_t &valueReg, reg_t &addressReg, 
				      signed offset) = 0;
   virtual void emitStoreDaemonNative(reg_t &valueReg, reg_t &addressReg, 
				      signed offset) = 0;

   // emits the st or stx operation depending on the native pointer type
   virtual void emitStoreKernelNative(reg_t &valueReg, reg_t &addressReg1, 
				      reg_t &addressReg2) = 0;
   virtual void emitStoreDaemonNative(reg_t &valueReg, reg_t &addressReg1, 
				      reg_t &addressReg2) = 0;

   virtual bool emit8ByteActualParameter(reg_t &srcReg64, reg_t &paramReg,
					 reg_t &paramReg_lowbits) = 0;
      // This one's a little complex, so pay attention: we have a 64-bit value
      // already in srcReg64, and we want to move it to paramReg (a parameter to
      // some procedure that's about to be called).  paramReg may equal srcReg64.
      // If the ABI thinks that the 8-byte parameter can indeed fit into a single
      // register (e.g. sparcv9 ABI), then this routine emits code to move srcReg64
      // to paramReg and returns true.  If not, then this routine emits code to split
      // split srcReg64 into two 4-byte registers: paramReg (gets the most significant
      // 32 bits) and paramReg_lowbits (gets the least significant 32 bits) and returns
      // false.

   static const pdstring this_chunk_addr;
   void emitThisChunkAddr(reg_t &rd);

   void emitLabelAddr(const pdstring &labelName, reg_t &rd);

   void emitString(const pdstring &);

   // --------------------------------------------------------------------------------
   // -----------------------------  Utility Routines --------------------------------
   // --------------------------------------------------------------------------------

   const abi& getABI() const;

   insnOffset getCurrOffset() const;

   instr_t* offset2Insn(const insnOffset &);
      // note the non-const return type; you can use it to change code after
      // emitting it (e.g. backpatching).

   insnOffset queryLabelOffset(const pdstring &labelName) const;

   // --------------------------------------------------------------------------------
   // ----------------- Debugging: facility to disable instrumentation ---------------
   // ------------------------------------ at runtime --------------------------------
   // --------------------------------------------------------------------------------

   virtual void emitPlaceholderDisablingBranch(const pdstring &disablingBranchLabelName) = 0;

   virtual void emitDisablingCode(const pdstring &disablingCodeLabelName,
                                  const pdstring &disablingBranchLabelName,
                                  const pdstring &done_label_name,
                                  reg_t &scratch_reg0,
                                  reg_t &scratch_reg1) = 0;
   virtual void completeDisablingCode(const pdstring &disablingBranchLabelName,
                                      const pdstring &done_label_name,
                                      const pdstring &disablingCodeLabelName,
                                      reg_t &branch_contents_reg) = 0;

   // --------------------------------------------------------------------------------
   // Completion.  Resolves what we can, leaving the rest to class relocatableCode.
   // Briefly stated, we cannot resolve everything in this class because we don't
   // know any absolute instruction addresses.  But remember, that's a feature, not
   // a bug: it means that all code emitted to this class is relocatable.
   // --------------------------------------------------------------------------------

   virtual void complete() = 0;
      // Fix up pending branches to labels.  Doesn't try to fix up unres calls or
      // AddrOfSym's

   bool isCompleted() const { return completed; }

   // --------------------------------------------------------------------------------
   // ----------------------------- Getting Info For Resolution ----------------------
   // --------------------------------------------------------------------------------

   const dictionary_hash<pdstring, insnOffset>& getLabels() const;
   const dictionary_hash<pdstring, kptr_t>& getKnownSymAddrs() const;
      // This routine is needed because resolving pc-dependent things (like calls)
      // require last-minute resolution, when insnAddrs are known; they're not known
      // in this class.
      // sym name --> address

   const pdvector< std::pair<insnOffset, pdstring> >& getUnresolvedBranchesToLabels() const;
      // It used to be that branches to labels could always be resolved, but with
      // cross-chunk branches, this is no longer true.
   const pdvector< std::pair<insnOffset, pdstring> >&
   getUnresolvedInterprocBranchesToName() const;
      // branches to symAddr32's.

   const pdvector< std::pair<insnOffset, kptr_t> >&
   getUnresolvedInterprocBranchesToAddr() const;

   const pdvector< std::pair<insnOffset, pdstring> >& getUnresolvedCallsByName() const;
   const pdvector< std::pair<insnOffset, kptr_t> >& getUnresolvedCallsByAddr() const;
      // .second: callee's address
      // The instruction presently emitted for this will be a call insn with
      // an offset equal to the callee addr, so you can assert as such later on.

   const pdvector< std::pair<insnOffset, pdstring> >& getUnresolvedSymAddrs() const;

   const pdvector< std::pair<insnOffset, pdstring> >&
      getUnresolvedLabelAddrsAsData() const;

   const pdvector< std::pair<insnOffset, triple<pdstring, pdstring, unsigned> > >&
      getUnresolvedLabelAddrMinusLabelAddrThenDivides() const;

   const pdvector< std::pair<insnOffset, pdstring> >& getUnresolvedLabelAddrs() const;

};

#endif
