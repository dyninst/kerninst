//sparc_tempBufferEmitter.C
#include "sparc_tempBufferEmitter.h"
#include <pair.h>
#include "sparc_instr.h"
#include "sparc_reg.h"
#include "abi.h"

sparc_tempBufferEmitter::sparc_tempBufferEmitter(const abi & iabi) : tempBufferEmitter(iabi) { }

// --------------------
bool sparc_tempBufferEmitter::isReadyToExecute() const {
   return unresolvedBranchesToLabels.size() == 0 &&
      unresolvedCallsByName.size() == 0 &&
      unresolvedCallsByAddr.size() == 0 &&
      unresolvedSymAddrs.size() == 0 &&
      unresolvedLabelAddrsAsData.size() == 0 &&
      unresolvedLabelAddrMinusLabelAddrThenDivides.size() == 0 &&
      unresolvedLabelAddrs.size() == 0 &&
      unresolvedSetHiBSetOrAddBackpatches.size() == 0;
}

// --------------------

sparc_tempBufferEmitter::
setHiBSetOrAddLocation::setHiBSetOrAddLocation(const pdstring &isetHiLabelName,
                                               uint32_t isetHiInsnByteOffset,
                                               const pdstring &ibsetOrAddLabelName,
                                               uint32_t ibsetOrAddInsnByteOffset) :
   setHiLabelName(isetHiLabelName),
   setHiInsnByteOffset(isetHiInsnByteOffset),
   bsetOrAddLabelName(ibsetOrAddLabelName),
   bsetOrAddInsnByteOffset(ibsetOrAddInsnByteOffset) {
}

// --------------------

const pdvector<sparc_tempBufferEmitter::setHiBSetBackpatchInfo> &
sparc_tempBufferEmitter::getUnresolvedSetHiBSetOrAddBackpatches() const {
   return unresolvedSetHiBSetOrAddBackpatches;
}

void sparc_tempBufferEmitter::
lateBackpatchOfSetHiBSetOrAddToLabelAddrPlusOffset(const setHiBSetOrAddLocation &insnLocs,
                                                   const pdstring &labelName,
                                                   unsigned byteOffset) {
   setHiBSetBackpatchInfo *info =
      unresolvedSetHiBSetOrAddBackpatches.append_with_inplace_construction();

   new((void*)info)setHiBSetBackpatchInfo(insnLocs, labelName, byteOffset);
}

void sparc_tempBufferEmitter::
emitLabelAddrAsData(const pdstring &labelName) {
   // like emitLabelAddrMinusLabelAddrDividedBy(), below, (in that it emits data)
   // but simpler.  Don't confuse with emitLabelAddr32(), which emits *code*
   // to set a register's value.  (The name of emitLabelAddr32() should be changed
   // since it is now confusing.)

   unresolvedLabelAddrsAsData += make_pair(currInsnOffset(), labelName);
   instr_t *i = (instr_t*) new sparc_instr(0u);
   emit(i);
      // dummy placeholder; resolved later
}

void sparc_tempBufferEmitter::
emitLabelAddrMinusLabelAddrDividedBy(const pdstring &label1,
                                     const pdstring &label2,
                                     unsigned divideByThis) {
   // always 1 "insn" (actually data)
   unresolvedLabelAddrMinusLabelAddrThenDivides +=
      make_pair(currInsnOffset(),
                make_triple(label1, label2, divideByThis));

   instr_t *i = (instr_t*) new sparc_instr(0u);
   emit(i);
}
// ---------------------------------------------------------------------------
// Resolving.  Note that in this class, absolute instructions addresses are NOT
// known for the code that we emit.  But that's a feature, not a bug: it makes
// the code that we emit **relocatable** (until fed to a relocatableCode object
// and a subsequent call to relocatableCode's obtainFullyResolvedCode(), but I
// digress).  The point is that we cannot always resolve all branches (because
// they may be cross-chunk branches, and we don't know how far chunk A will end
// up from chunk B when all it said and done [each chunk is relocatable to any
// address, after all).
// ---------------------------------------------------------------------------
void sparc_tempBufferEmitter::resolve1SetImmAddr(const insnOffset& offs, kptr_t addr)
{
   const kptr_t zero = 0;
   instr_t *first = offset2Insn(offs);
   sparc_reg rd = sparc_reg(sparc_reg::rawIntReg,
			    getRdBits(first->getRaw()));
   insnVec_t *oldv = insnVec_t::getInsnVec();
   insnVec_t *newv = insnVec_t::getInsnVec();
   
   sparc_instr::genImmAddrGeneric(oldv, zero, rd);
   sparc_instr::genImmAddr(newv, addr, rd);
   assert(newv->numInsns() <= oldv->numInsns()); 

   insnVec_t::const_iterator old_iter = oldv->begin();
   insnVec_t::const_iterator new_iter = newv->begin();

   insnOffset offset = offs;

   // Verify that we see the generic version emitted
   // Replace it with the resolved, optimized version
   for (; old_iter != oldv->end() && new_iter != newv->end();
	  old_iter++, new_iter++, offset.byteOffset += 4) {
      instr_t *current = offset2Insn(offset);
      assert(current->getRaw() == (*old_iter)->getRaw());
      *current = **new_iter;
   }
   // Pad with nop's, if necessary
   for (; old_iter != oldv->end(); 
	  old_iter++, offset.byteOffset += 4) {
      instr_t *current = offset2Insn(offset);
      assert(current->getRaw() == (*old_iter)->getRaw());
      *((sparc_instr*)current) = sparc_instr(sparc_instr::nop);
   }
   delete oldv;
   delete newv;
}

// ----------------------------------------------------------------------
void sparc_tempBufferEmitter::reset() {
   theChunks.clear();

   insnVec_t **iv_ptr = theChunks.append_with_inplace_construction();
   *iv_ptr = insnVec_t::getInsnVec(); // default insnVec ctor for this new chunk
   assert(theChunks.size() == 1);
   currChunkNdx = 0;

   completed = false;

   labels.clear();
   unresolvedBranchesToLabels.clear();
   unresolvedInterprocBranchesToName.clear();
   unresolvedInterprocBranchesToAddr.clear();
   unresolvedCallsByName.clear();
   unresolvedCallsByAddr.clear();
   unresolvedSymAddrs.clear();
   unresolvedLabelAddrsAsData.clear();
   unresolvedLabelAddrMinusLabelAddrThenDivides.clear();
   unresolvedLabelAddrs.clear();
   unresolvedSetHiBSetOrAddBackpatches.clear();
}

// ----------------------------------------------------------------------

void sparc_tempBufferEmitter::fixUpUnresolvedBranchesToLabels() {
   // We can only resolve branches to labels in the same "chunk", because otherwise
   // we don't know the offset.

   pdvector< pair<insnOffset,pdstring> >::iterator iter =
      unresolvedBranchesToLabels.begin();

   while (iter != unresolvedBranchesToLabels.end()) {
      // NOTE: it's vital to re-read unresolvedBranchesToLabels.end() each time thru the
      // loop, since we're deleting from the vector.
      const insnOffset &branchinsn_off = iter->first;
      const pdstring &labelName = iter->second;

      const insnOffset &label_off = labels.get(labelName);
      
      if (branchinsn_off.chunkNdx != label_off.chunkNdx) {
         // can't resolve this branch instruction because it's a cross-chunk branch.
         ++iter;
         continue;
      }
      
      const int insnbytes_delta = (int)label_off.byteOffset -
                                  (int)branchinsn_off.byteOffset;

      // sparc-specific (and thus temporary) assertion:
      assert(insnbytes_delta % 4 == 0);

      instr_t *i = offset2Insn(branchinsn_off);
      i->changeBranchOffset(insnbytes_delta);

      // We've resolved this unresolvedBranchesToLabels[] entry, so remove it
      *iter = unresolvedBranchesToLabels.back();
      unresolvedBranchesToLabels.pop_back();
      // note that we do NOT "++iter" in this case.
   }
}

void sparc_tempBufferEmitter::fixUpUnresolvedInterprocBranchesToName() {
   // In order to fix up a branch, we need to know the offset.
   // We don't have such information here.  More specifically, we might know
   // the exact address of the destination, but without our exact address, we can't
   // know the offset.  The other possibility is not knowing either exact address
   // but knowing the exact distance between them; we don't have that information
   // here either.
}

void sparc_tempBufferEmitter::fixUpUnresolvedInterprocBranchesToAddr() {
   // can't do anything in this class since we don't yet know insnAddr's
}

void sparc_tempBufferEmitter::fixUpUnresolvedLabelAddrs() {
   // We cannot resolve labelAddrs in this class, since we don't have any absolute
   // insn addresses.  See fixUpKnownCalls() for a similar situation.
}

void sparc_tempBufferEmitter::fixUpUnresolvedLabelAddrsAsData() {
   // can't fix anything up here, since absolute addresses of the labels aren't
   // known until fully resolved
}

void sparc_tempBufferEmitter::fixUpLabelAddrMinusLabelAddrDividedBys() {
   pdvector< pair<insnOffset, triple<pdstring, pdstring, unsigned> > >::iterator iter = unresolvedLabelAddrMinusLabelAddrThenDivides.begin();

   while (iter != unresolvedLabelAddrMinusLabelAddrThenDivides.end()) {
      // NOTE: it's vital to re-read end() above since we're deleting from the
      // vector within this loop

      const pair<insnOffset, triple<pdstring, pdstring, unsigned> > &info = *iter;
      
      const insnOffset &theInsnOffset = info.first;
      const insnOffset &label1Offset = labels.get(info.second.first);
      const insnOffset &label2Offset = labels.get(info.second.second);

      if (label1Offset.chunkNdx != label2Offset.chunkNdx) {
         // sorry, I don't know how far label1 is from label2 when they cross
         // chunk boundaries.  relocatableCode can handle this later on.
         ++iter;
         continue;
      }
         
      const unsigned difference = label1Offset.byteOffset - label2Offset.byteOffset;
      const unsigned result = difference / info.second.third;
      
      instr_t *i = offset2Insn(theInsnOffset);
      assert(i->getRaw() == 0);
      *((sparc_instr*)i) = sparc_instr(result);

      *iter = unresolvedLabelAddrMinusLabelAddrThenDivides.back();
      unresolvedLabelAddrMinusLabelAddrThenDivides.pop_back();

      // note that we do NOT "++iter" in this case
   }

}

void sparc_tempBufferEmitter::fixUpKnownSymAddrs() {
   pdvector< pair<insnOffset, pdstring> >::iterator iter = unresolvedSymAddrs.begin();
   
   while (iter != unresolvedSymAddrs.end()) {
      // NOTE: it's vital to re-read end() above, since we're deleting from
      // the vector in the middle of the loop

      const pair<insnOffset, pdstring> &theUnresolvedSymAddrInfo = *iter;

      const insnOffset &theInsnOffset = theUnresolvedSymAddrInfo.first;
      const pdstring &theSymName = theUnresolvedSymAddrInfo.second;

      if (knownSymAddrs.defines(theSymName)) {
         // We can resolve this guy
         const kptr_t valueToUse = knownSymAddrs.get(theSymName);
	 resolve1SetImmAddr(theInsnOffset, valueToUse);
         // remove from unresolvedSymAddrs
         *iter = unresolvedSymAddrs.back();
         unresolvedSymAddrs.pop_back();
         // note that we do *not* "++iter" in this case
      }
      else
         ++iter;
   }
}

void sparc_tempBufferEmitter::fixUpKnownCalls() {
   // This can only be done if we can know, for any given emitted instruction,
   // what its final emitted address will be.  This is because call instructions
   // are pc-relative.  Thus, we can't do anything in this class.
   // relocatableCode, on the other hand, can help, since one of its methods
   // takes in an emitAddr.
}

void sparc_tempBufferEmitter::fixUpSetHiBSetOrAddBackpatches() {
   // doesn't do anything because it would need to known insn addrs
}

void sparc_tempBufferEmitter::complete() {
   assert(!completed && "already called complete() for this emitter");

   // Fix up pending branches to labels
   fixUpUnresolvedBranchesToLabels();

   fixUpUnresolvedInterprocBranchesToName();
   fixUpUnresolvedInterprocBranchesToAddr();

   // Fix up emitLabelAddr()'s:
   fixUpUnresolvedLabelAddrs();

   fixUpUnresolvedLabelAddrsAsData();
   fixUpLabelAddrMinusLabelAddrDividedBys();

   // Use knownSymAddrs[] (rare that this gets us anywhere):
   fixUpKnownSymAddrs();
   fixUpKnownCalls();

   fixUpSetHiBSetOrAddBackpatches();
   
   // Can't (yet) resolve (most) calls or (most) symAddrs

   completed = true;
}
// emits the ld or ldx operation depending on the native pointer type
void sparc_tempBufferEmitter::emitLoadKernelNative(reg_t &srcAddrReg, 
					     signed offs, 
					     reg_t &rd) 
{
   //make sure the offset fits within 13 bits
   assert((offs < 4095) && (offs > -4096)); 
   if (sizeof(kptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::load, 
					       sparc_instr::ldUnsignedWord,
					       (sparc_reg&)srcAddrReg, offs, 
					       (sparc_reg&)rd);
      emit(i);
   }
   else if (sizeof(kptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::load, 
					       sparc_instr::ldExtendedWord,
					       (sparc_reg&)srcAddrReg, offs, 
					       (sparc_reg&)rd);
      emit(i);
   }
   else
      assert(false);
}

void sparc_tempBufferEmitter::emitLoadDaemonNative(reg_t &srcAddrReg, 
					     signed offs, 
					     reg_t &rd) 
{
   //make sure the offset fits within 13 bits
   assert((offs < 4095) && (offs > -4096)); 
   if (sizeof(dptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::load, 
					       sparc_instr::ldUnsignedWord,
					       (sparc_reg&)srcAddrReg, offs, 
					       (sparc_reg&)rd);
      emit(i);
   }
   else if (sizeof(dptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::load, 
					       sparc_instr::ldExtendedWord,
					       (sparc_reg&)srcAddrReg, offs, 
					       (sparc_reg&)rd);
      emit(i);
   }
   else
      assert(false);
}

// emits the ld or ldx operation depending on the native pointer type
void sparc_tempBufferEmitter::emitLoadKernelNative(reg_t &srcAddrReg1, 
					     reg_t &srcAddrReg2, 
					     reg_t &rd)
{
   if (sizeof(kptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::load, 
					       sparc_instr::ldUnsignedWord,
					       (sparc_reg&)srcAddrReg1, 
					       (sparc_reg&)srcAddrReg2, 
					       (sparc_reg&)rd);
      emit(i);
   }
   else if (sizeof(kptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::load, 
					       sparc_instr::ldExtendedWord,
					       (sparc_reg&)srcAddrReg1, 
					       (sparc_reg&)srcAddrReg2, 
					       (sparc_reg&)rd);
      emit(i);
   }
   else
      assert(false);
}

void sparc_tempBufferEmitter::emitLoadDaemonNative(reg_t &srcAddrReg1, 
					     reg_t &srcAddrReg2, 
					     reg_t &rd)
{
   if (sizeof(dptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::load, 
					       sparc_instr::ldUnsignedWord,
					       (sparc_reg&)srcAddrReg1, 
					       (sparc_reg&)srcAddrReg2, 
					       (sparc_reg&)rd);
      emit(i);
   }
   else if (sizeof(dptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::load, 
					       sparc_instr::ldExtendedWord,
					       (sparc_reg&)srcAddrReg1, 
					       (sparc_reg&)srcAddrReg2, 
					       (sparc_reg&)rd);
      emit(i);
   }
   else
      assert(false);
}

// emits the st or stx operation depending on the native pointer type
void sparc_tempBufferEmitter::emitStoreKernelNative(reg_t &valueReg, 
					      reg_t &addressReg, 
					      signed offset)
{
   //make sure the offset fits within 13 bits
   assert((offset < 4095) && (offset > -4096)); 
   if (sizeof(kptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store, 
					       sparc_instr::stWord,
					       (sparc_reg&)valueReg, 
					       (sparc_reg&)addressReg, offset);
      emit(i);
   }
   else if (sizeof(kptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store, 
					       sparc_instr::stExtended,
					       (sparc_reg&)valueReg, 
					       (sparc_reg&)addressReg, offset);
      emit(i);
   }
   else
      assert(false);
}

void sparc_tempBufferEmitter::emitStoreDaemonNative(reg_t &valueReg, 
					      reg_t &addressReg, 
					      signed offset)
{
   //make sure the offset fits within 13 bits
   assert((offset < 4095) && (offset > -4096)); 
   if (sizeof(dptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store, 
					       sparc_instr::stWord,
					       (sparc_reg&)valueReg, 
					       (sparc_reg&)addressReg, offset);
      emit(i);
   }
   else if (sizeof(dptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store, 
					       sparc_instr::stExtended,
					       (sparc_reg&)valueReg, 
					       (sparc_reg&)addressReg, offset);
      emit(i);
   }
   else
      assert(false);
}

// emits the st or stx operation depending on the native pointer type
void sparc_tempBufferEmitter::emitStoreKernelNative(reg_t &valueReg, 
					      reg_t &addressReg1, 
					      reg_t &addressReg2)
{
   if (sizeof(kptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store, 
					       sparc_instr::stWord,
					       (sparc_reg&)valueReg, 
					       (sparc_reg&)addressReg1, 
					       (sparc_reg&)addressReg2);
      emit(i);
   }
   else if (sizeof(kptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store, 
					       sparc_instr::stExtended,
					       (sparc_reg&)valueReg, 
					       (sparc_reg&)addressReg1, 
					       (sparc_reg&)addressReg2);
      emit(i);
   }
   else
      assert(false);
}

void sparc_tempBufferEmitter::emitStoreDaemonNative(reg_t &valueReg, 
					      reg_t &addressReg1, 
					      reg_t &addressReg2)
{
   if (sizeof(dptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store, 
					       sparc_instr::stWord,
					       (sparc_reg&)valueReg, 
					       (sparc_reg&)addressReg1, 
					       (sparc_reg&)addressReg2);
      emit(i);
   }
   else if (sizeof(dptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store, 
					       sparc_instr::stExtended,
					       (sparc_reg&)valueReg, 
					       (sparc_reg&)addressReg1, 
					       (sparc_reg&)addressReg2);
      emit(i);
   }
   else
      assert(false);
}

bool sparc_tempBufferEmitter::emit8ByteActualParameter(reg_t & srcReg64,
                                                 reg_t & paramReg,
                                                 reg_t & paramReg_lowbits) {
   // This one's a little complex, so pay attention: we have a 64-bit value
   // already in srcReg64, and we want to move it paramReg (a parameter to
   // some procedure that's about to be called).  paramReg may equal srcReg64.
   // If the ABI thinks that the 8-byte parameter can indeed fit into a single
   // register (e.g. sparcv9 ABI), then this routine simply emits code to move srcReg64
   // to paramReg and return true.  If not, then this routine emits code to split
   // split srcReg64 into two 4-byte registers: paramReg (gets the most significant
   // 32 bits) and paramReg_lowbits (gets the least significant 32 bits) and return
   // false.

   // paramReg may equal srcReg64; however, paramReg cannot equal paramReg_lowbits,
   // and paramReg_lowbits may not equal srcReg64.

   assert(theABI.isReg64Safe(srcReg64));
   assert(paramReg != paramReg_lowbits);
   assert(srcReg64 != paramReg_lowbits);
   
   if (theABI.integerRegParamNumBytes() >= 8) {
      // trivial; not much is needed
      if (paramReg != srcReg64) {
	 instr_t *i = (instr_t *) new sparc_instr(sparc_instr::mov, 
						  (sparc_reg&)srcReg64, 
						  (sparc_reg&)paramReg);
	 emit(i);
      }
      return true;
   }
   else {
      assert(theABI.integerRegParamNumBytes() == 4);
      // In case srcReg64 equals paramReg, we should write to paramReg_lowbits
      // first for safety.
      instr_t *i = (instr_t *) new sparc_instr(sparc_instr::shift, 
					       sparc_instr::rightLogical,
					       false, // not extended
					       (sparc_reg&)paramReg_lowbits, // dest
					       (sparc_reg&)srcReg64, 0);
      emit(i);
         // srl by 0 bits truncates to the low 32 bits

      i = (instr_t *) new sparc_instr(sparc_instr::shift, 
				      sparc_instr::rightLogical,
				      true, // extended
				      (sparc_reg&)paramReg, // dest
				      (sparc_reg&)srcReg64, 32);
      emit(i);
         // note that the above instruction is needed even if paramReg==srcReg64

      return false;
   }
}

// --------------------------------------------------------------------------------
// ----------------- Debugging: facility to disable instrumentation ---------------
// ------------------------------------ at runtime --------------------------------
// --------------------------------------------------------------------------------

void sparc_tempBufferEmitter::
emitPlaceholderDisablingBranch(const pdstring &disablingBranchLabelName) {
   emitLabel(disablingBranchLabelName);
   instr_t *i = (instr_t *) new sparc_instr(sparc_instr::nop); 
     // placeholder for disabling branch
   emit(i);
}

void sparc_tempBufferEmitter::emitDisablingCode(const pdstring &disablingCodeLabelName,
                                          const pdstring &disablingBranchLabelName,
                                          const pdstring & /*done_label_name*/,
                                          reg_t & scratch_reg0,
                                          reg_t & scratch_reg1) {
   emitLabel(disablingCodeLabelName);
   instr_t *nop = (instr_t *) new sparc_instr(sparc_instr::nop);
   emit(nop); // will be a sethi into scratch_reg0
   nop = (instr_t *) new sparc_instr(sparc_instr::nop);  //we don't want aliasing inside the chunk
   
   emit(nop); // will be a bset into scratch_reg0

   // assume that the branch instruction is in scratch_reg; store it
   // at location "disablingBranchLabelName"
   emitLabelAddr(disablingBranchLabelName, scratch_reg1); // location to write to
   instr_t *i = (instr_t *) new sparc_instr(sparc_instr::store,
					    sparc_instr::stWord, 
					    (sparc_reg&)scratch_reg0, 
					    (sparc_reg&)scratch_reg1, 0);
   emit(i);
   i = (instr_t *) new sparc_instr(sparc_instr::flush, 
				   (sparc_reg&)scratch_reg1, 0);
   emit(i);
   
   // at this point at runtime, uninstrumentation is done.
   // But in the here and now (kerninstd code generation), we still have some
   // backpatching to do in order to fill in some nop instructions with the real deal.
}

void sparc_tempBufferEmitter::completeDisablingCode(const pdstring &disablingBranchLabelName,
                                              const pdstring &done_label_name,
                                              const pdstring &disablingCodeLabelName,
                                              reg_t & branch_contents_reg) {
   // disablingBranchLabelName and done_label_name are needed to calculate the offset
   // that the disabling branch should have.
   // disablingCodeLabelName is needed so we know where to update a "set"
   // instruction (sethi/bset on sparc) once we figure out the bit pattern of the
   // disabling branch instruction.

   const insnOffset &done_label_offset = queryLabelOffset(done_label_name);
   const insnOffset &disablingBranch_offset = queryLabelOffset(disablingBranchLabelName);
   assert(done_label_offset.chunkNdx == disablingBranch_offset.chunkNdx &&
          "disabling code sequence must be in the same chunk (1)");
   
   const unsigned offset_bytes = done_label_offset.byteOffset -
                                 disablingBranch_offset.byteOffset;
   const sparc_instr branch_insn(sparc_instr::bpcc,
                                 sparc_instr::iccAlways,
                                 true, // annulled
                                 true, true, // predict taken, xcc
                                 offset_bytes);
   const unsigned branch_insn_raw = branch_insn.getRaw();

   const insnOffset &disablingCodeLabelName_offset = 
      queryLabelOffset(disablingCodeLabelName);
   assert(disablingCodeLabelName_offset.chunkNdx == done_label_offset.chunkNdx
	  && "disabling code sequence must be in the same chunk (2)");
   
   instr_t *sethi_insn_of_emergency_branch =
      offset2Insn(disablingCodeLabelName_offset);
   insnOffset disablingCodeLabelName_offset_insn2(disablingCodeLabelName_offset);
   disablingCodeLabelName_offset_insn2.byteOffset += 4;
   instr_t *bset_insn_of_emergency_branch =
      offset2Insn(disablingCodeLabelName_offset_insn2);

   *sethi_insn_of_emergency_branch = sparc_instr(sparc_instr::sethi,
						 sparc_instr::HiOf(),
						 branch_insn_raw,
						 (sparc_reg&)branch_contents_reg);
   *bset_insn_of_emergency_branch = sparc_instr(false, sparc_instr::bset,
						sparc_instr::lo_of_32(branch_insn_raw),
						(sparc_reg&)branch_contents_reg);
}
