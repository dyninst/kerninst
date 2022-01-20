// sparc_tempBufferEmitter.h

#ifndef _SPARC_TEMPBUFFEREMITTER_H_
#define _SPARC_TEMPBUFFEREMITTER_H_

#include "tempBufferEmitter.h"

class sparc_tempBufferEmitter :  public tempBufferEmitter { 

 public:
    struct setHiBSetOrAddLocation {
      pdstring setHiLabelName;
      uint32_t setHiInsnByteOffset;
      pdstring bsetOrAddLabelName;
      uint32_t bsetOrAddInsnByteOffset;

      setHiBSetOrAddLocation(const pdstring &isetHiLabelName,
                             uint32_t isetHiInsnByteOffset,
                             const pdstring &ibsetOrAddLabelName,
                             uint32_t ibsetOrAddInsnByteOffset);
   };
   class setHiBSetBackpatchInfo {
    private:
      // private prevents it from being called
      setHiBSetBackpatchInfo& operator=(const setHiBSetBackpatchInfo &src);
      
    public:
      // patch up these instructions...
      setHiBSetOrAddLocation insnLocs;

      // ...to set the address of this label...
      pdstring labelName;
      // ...plus this byte offset
      unsigned byteOffset;

      setHiBSetBackpatchInfo(const setHiBSetOrAddLocation &iinsnLocs,
                             const pdstring &destLabelName,
                             unsigned destByteOffsetFromLabel) :
         insnLocs(iinsnLocs),
         labelName(destLabelName), byteOffset(destByteOffsetFromLabel) {
      }
      setHiBSetBackpatchInfo(const pdstring &setHiLabelName,
                             unsigned setHiInsnByteOffset,
                             const pdstring &bsetOrAddLabelName,
                             unsigned bsetOrAddInsnByteOffset,
                             const pdstring &destLabelName,
                             unsigned destByteOffsetFromLabel) :
         insnLocs(setHiLabelName, setHiInsnByteOffset,
                  bsetOrAddLabelName, bsetOrAddInsnByteOffset),
         labelName(destLabelName), byteOffset(destByteOffsetFromLabel) {
      }
      setHiBSetBackpatchInfo(const setHiBSetBackpatchInfo &src) :
         insnLocs(src.insnLocs), labelName(src.labelName), byteOffset(src.byteOffset) {
      }
   };

 private:
     pdvector<setHiBSetBackpatchInfo> unresolvedSetHiBSetOrAddBackpatches;
      // .first: the location of two instructions: a sethi and a bset/add, specified
      // as offsets from labels
      // .second: a label name.  When we find out the absolute address of this
      // label, we patch up the sethi and bset/add instructions.  Of course, this
      // class never knows absolute addresses of its labels, since its code is meant
      // to be relocatable.  Class relocatableCode::obtainFullyResolvedCode()
      // can know precise addresses, however.

     bool isReadyToExecute() const;

     // Replace the placeholder instructions for setting an immediate address
     // with valid ones. Verify that we are replacing the desired instructions
     void resolve1SetImmAddr(const insnOffset& offs, kptr_t addr);
     
     // Routines called by complete():
     // Note that since relocatableCode has to do similar work, most of these routines
     // should arguably be deleted.
     void fixUpUnresolvedBranchesToLabels();
     // can't always fully resolve, due to cross-chunk branches
     void fixUpUnresolvedInterprocBranchesToName();
     // only resolves if makeSymAddr32Known() was called as appropriate.
     void fixUpUnresolvedInterprocBranchesToAddr();
     void fixUpUnresolvedLabelAddrs();
     void fixUpUnresolvedLabelAddrsAsData();
     void fixUpLabelAddrMinusLabelAddrDividedBys();
     void fixUpKnownSymAddrs();
     // only does something if knownSymAddrs[] is nonempty
     void fixUpKnownCalls();
     // doesn't do anything because it can't
     // (would need to know insnAddr's).  Fortunately, relocatableCode can resolve it.
     void fixUpSetHiBSetOrAddBackpatches();
     // doesn't do anything because it would need to known insn addrs
     
     // Prevent copying:
     sparc_tempBufferEmitter(const sparc_tempBufferEmitter &);
     sparc_tempBufferEmitter &operator=(const sparc_tempBufferEmitter &);

 public:
     sparc_tempBufferEmitter(const abi &);

     void lateBackpatchOfSetHiBSetOrAddToLabelAddrPlusOffset(const setHiBSetOrAddLocation &,
                                                           const pdstring &labelName,
                                                           unsigned byteOffset);

     void emitLabelAddrAsData(const pdstring &labelName);
     // useful for emitting jump table data
     void emitLabelAddrMinusLabelAddrDividedBy
        (const pdstring &labelName1,
         const pdstring &labelName2, // subtract this
         unsigned divideByThis);
     // useful for emitting jump table data

     // emits the ld or ldx operation depending on the native pointer type
     void emitLoadKernelNative(reg_t &srcAddrReg, signed offset,  
			       reg_t &rd);
     void emitLoadDaemonNative(reg_t &srcAddrReg, signed offset,
			       reg_t &rd);

     // emits the ld or ldx operation depending on the native pointer type
     void emitLoadKernelNative(reg_t &srcAddrReg1, reg_t &srcAddrReg2, 
			       reg_t &rd) ;
     void emitLoadDaemonNative(reg_t &srcAddrReg1, reg_t &srcAddrReg2, 
			       reg_t &rd);

     // emits the st or stx operation depending on the native pointer type
     void emitStoreKernelNative(reg_t &valueReg, reg_t &addressReg, 
				signed offset);
     void emitStoreDaemonNative(reg_t &valueReg, reg_t &addressReg, 
				signed offset);

     // emits the st or stx operation depending on the native pointer type
     void emitStoreKernelNative(reg_t &valueReg, reg_t &addressReg1, 
				reg_t &addressReg2);
     void emitStoreDaemonNative(reg_t &valueReg, reg_t &addressReg1, 
				reg_t &addressReg2);

     bool emit8ByteActualParameter(reg_t &srcReg64,
				   reg_t &paramReg,
				   reg_t &paramReg_lowbits);
      // This one's a little complex, so pay attention: we have a 64-bit value
      // already in srcReg64, and we want to move it to paramReg (a parameter to
      // some procedure that's about to be called).  paramReg may equal srcReg64.
      // If the ABI thinks that the 8-byte parameter can indeed fit into a single
      // register (e.g. sparcv9 ABI), then this routine emits code to move srcReg64
      // to paramReg and returns true.  If not, then this routine emits code to split
      // split srcReg64 into two 4-byte registers: paramReg (gets the most significant
      // 32 bits) and paramReg_lowbits (gets the least significant 32 bits) and returns
      // false.
    
      // -------------------------------------------------------------------------
    // Completion.  Resolves what we can, leaving the rest to class relocatableCode.
    // Briefly stated, we cannot resolve everything in this class because we don't
    // know any absolute instruction addresses.  But remember, that's a feature, not
    // a bug: it means that all code emitted to this class is relocatable.
    // ---------------------------------------------------------------------------
    
     void complete();

     void reset();

     // --------------------------------------------------------------------------------
   // ----------------- Debugging: facility to disable instrumentation ---------------
   // ------------------------------------ at runtime --------------------------------
   // --------------------------------------------------------------------------------

     void emitPlaceholderDisablingBranch(const pdstring &disablingBranchLabelName);

     void emitDisablingCode(const pdstring &disablingCodeLabelName,
			    const pdstring &disablingBranchLabelName,
			    const pdstring &done_label_name,
			    reg_t &scratch_reg0,
			    reg_t &scratch_reg1);
     void completeDisablingCode(const pdstring &disablingBranchLabelName,
				const pdstring &done_label_name,
				const pdstring &disablingCodeLabelName,
				reg_t &branch_contents_reg);


     const pdvector<setHiBSetBackpatchInfo>&
       getUnresolvedSetHiBSetOrAddBackpatches() const;
};
#endif
