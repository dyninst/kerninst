//power_tempBufferEmitter.C
#include "power_tempBufferEmitter.h"
#include <pair.h>
#include "power_instr.h"
#include "power_reg.h"
#include "abi.h"

power_tempBufferEmitter::power_tempBufferEmitter(const abi & iabi) : tempBufferEmitter(iabi) { }

// --------------------
bool power_tempBufferEmitter::isReadyToExecute() const {
   return unresolvedBranchesToLabels.size() == 0 &&
      unresolvedCallsByName.size() == 0 &&
      unresolvedCallsByAddr.size() == 0 &&
      unresolvedSymAddrs.size() == 0 &&
      unresolvedLabelAddrsAsData.size() == 0 &&
      unresolvedLabelAddrMinusLabelAddrThenDivides.size() == 0 &&
      unresolvedLabelAddrs.size() == 0;
}

void power_tempBufferEmitter::
emitLabelAddrAsData(const pdstring  & /* labelName */) {
   //This is only used by outliner, and in any case, jump tables would be handled differently on power
   assert(false && "no data in code section on Power");
}

void power_tempBufferEmitter::
emitLabelAddrMinusLabelAddrDividedBy(const pdstring & /* label1 */,
                                     const pdstring &/* label2 */,
                                     unsigned /* divideByThis */) {
   //This is only used by outliner, and in any case, jump tables would be handled differently on power
   assert(false && "no data in code section on Power");
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
void power_tempBufferEmitter::resolve1SetImmAddr(const insnOffset& offs, kptr_t addr)
{
   const kptr_t zero = 0;
   instr_t *first = offset2Insn(offs);
   power_reg rd = power_reg(power_reg::rawIntReg,
			    getRdBits(first->getRaw()));
   insnVec_t *oldv = insnVec_t::getInsnVec();
   insnVec_t *newv = insnVec_t::getInsnVec();
   
   power_instr::genImmAddrGeneric(oldv, zero, rd);
   power_instr::genImmAddr(newv, addr, rd);
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
      *((power_instr*)current) = power_instr(power_instr::nop);
   }
   delete oldv;
   delete newv;
}

// ----------------------------------------------------------------------
void power_tempBufferEmitter::reset() {
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
}

// ----------------------------------------------------------------------

void power_tempBufferEmitter::fixUpUnresolvedBranchesToLabels() {
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

      assert(insnbytes_delta % 4 == 0);

      instr_t *i = offset2Insn(branchinsn_off);
      i->changeBranchOffset(insnbytes_delta);

      // We've resolved this unresolvedBranchesToLabels[] entry, so remove it
      *iter = unresolvedBranchesToLabels.back();
      unresolvedBranchesToLabels.pop_back();
      // note that we do NOT "++iter" in this case.
   }
}

void power_tempBufferEmitter::fixUpUnresolvedInterprocBranchesToName() {
   // In order to fix up a branch, we need to know the offset.
   // We don't have such information here.  More specifically, we might know
   // the exact address of the destination, but without our exact address, we can't
   // know the offset.  The other possibility is not knowing either exact address
   // but knowing the exact distance between them; we don't have that information
   // here either.
}

void power_tempBufferEmitter::fixUpUnresolvedInterprocBranchesToAddr() {
   // can't do anything in this class since we don't yet know insnAddr's
}

void power_tempBufferEmitter::fixUpUnresolvedLabelAddrs() {
   // We cannot resolve labelAddrs in this class, since we don't have any absolute
   // insn addresses.  See fixUpKnownCalls() for a similar situation.
}

void power_tempBufferEmitter::fixUpUnresolvedLabelAddrsAsData() {
   // can't fix anything up here, since absolute addresses of the labels aren't
   // known until fully resolved
}

void power_tempBufferEmitter::fixUpLabelAddrMinusLabelAddrDividedBys() {
   //This is only used by outliner, and in any case, jump tables would be handled differently on power
   assert(false && "no data in code section on Power");
}

void power_tempBufferEmitter::fixUpKnownSymAddrs() {
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

void power_tempBufferEmitter::fixUpKnownCalls() {
   // This can only be done if we can know, for any given emitted instruction,
   // what its final emitted address will be.  This is because call instructions
   // are pc-relative.  Thus, we can't do anything in this class.
   // relocatableCode, on the other hand, can help, since one of its methods
   // takes in an emitAddr.
}


void power_tempBufferEmitter::complete() {
   assert(!completed && "already called complete() for this emitter");

   // Fix up pending branches to labels
   fixUpUnresolvedBranchesToLabels();

   fixUpUnresolvedInterprocBranchesToName();
   fixUpUnresolvedInterprocBranchesToAddr();

   // Use knownSymAddrs[] (rare that this gets us anywhere):
   fixUpKnownSymAddrs();
   fixUpKnownCalls();

   completed = true;
}
// emits the ld or ldx operation depending on the native pointer type
void power_tempBufferEmitter::emitLoadKernelNative(reg_t &srcAddrReg, 
					     signed offs, 
					     reg_t &rd) 
{
   //make sure the offset fits within 16 bits
   assert((offs < 32767) && (offs > -32768)); 
   if (sizeof(kptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::load, 
					       power_instr::ldWordZeroD,
					       (power_reg&)rd,
					       (power_reg&)srcAddrReg, offs); 
      emit(i);
   }
   else if (sizeof(kptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::load, 
					       power_instr::ldDoubleDS,
                                               (power_reg &)rd, 
                                               (power_reg&)srcAddrReg, offs);
      emit(i);
   }
   else
      assert(false);
}

// emits the ld or ldx operation depending on the daemon pointer type
void power_tempBufferEmitter::emitLoadDaemonNative(reg_t &srcAddrReg, 
					     signed offs, 
					     reg_t &rd) 
{
   //make sure the offset fits within 16 bits
   assert((offs < 32767) && (offs > -32768)); 
   if (sizeof(dptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::load, 
					       power_instr::ldWordZeroD,
					       (power_reg&)rd,
					       (power_reg&)srcAddrReg, offs); 
      emit(i);
   }
   else if (sizeof(dptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::load, 
					       power_instr::ldDoubleDS,
                                               (power_reg &)rd, 
                                               (power_reg&)srcAddrReg, offs);
      emit(i);
   }
   else
      assert(false);
}

// emits the ld or ldx operation depending on the native pointer type
void power_tempBufferEmitter::emitLoadKernelNative(reg_t &srcAddrReg1, 
					     reg_t &srcAddrReg2, 
					     reg_t &rd)
{
   if (sizeof(kptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::load, 
                                               power_instr::ldWordZeroX,
                                               (power_reg&)rd,
                                               (power_reg&)srcAddrReg1, 
                                               (power_reg&)srcAddrReg2);
      emit(i);
   }
   else if (sizeof(kptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::load, 
					       power_instr::ldDoubleX,
                                               (power_reg&)rd,
                                               (power_reg&)srcAddrReg1, 
                                               (power_reg&)srcAddrReg2);
      emit(i);
   }
   else
      assert(false);
}

void power_tempBufferEmitter::emitLoadDaemonNative(reg_t &srcAddrReg1, 
					     reg_t &srcAddrReg2, 
					     reg_t &rd)
{
   if (sizeof(dptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::load, 
                                               power_instr::ldWordZeroX,
                                               (power_reg&)rd,
                                               (power_reg&)srcAddrReg1, 
                                               (power_reg&)srcAddrReg2);
      emit(i);
   }
   else if (sizeof(dptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::load, 
                                               power_instr::ldDoubleX,
                                               (power_reg&)rd,
                                               (power_reg&)srcAddrReg1, 
                                               (power_reg&)srcAddrReg2);
      emit(i);
   }
   else
      assert(false);
}


// emits the st or stx operation depending on the native pointer type
void power_tempBufferEmitter::emitStoreKernelNative(reg_t &valueReg, 
					      reg_t &addressReg, 
					      signed offset)
{
   //make sure the offset fits within 16 bits
   assert((offset < 32767) && (offset > -32768)); 
   if (sizeof(kptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::store, 
					       power_instr::stWordD,
					       (power_reg&)valueReg, 
					       (power_reg&)addressReg, offset);

      emit(i);
   }
   else if (sizeof(kptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::store, 
					       power_instr::stDoubleDS,
					       (power_reg&)valueReg, 
					       (power_reg&)addressReg, offset);
      emit(i);
   }
   else
      assert(false);
}

void power_tempBufferEmitter::emitStoreDaemonNative(reg_t &valueReg, 
					      reg_t &addressReg, 
					      signed offset)
{
   //make sure the offset fits within 16 bits
   assert((offset < 32767) && (offset > -32768)); 
   if (sizeof(dptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::store, 
					       power_instr::stWordD,
					       (power_reg&)valueReg, 
					       (power_reg&)addressReg, offset);
      emit(i);
   }
   else if (sizeof(dptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::store, 
					       power_instr::stDoubleDS,
					       (power_reg&)valueReg, 
					       (power_reg&)addressReg, offset);
      emit(i);
   }
   else
      assert(false);
}

// emits the st or stx operation depending on the native pointer type
void power_tempBufferEmitter::emitStoreKernelNative(reg_t &valueReg, 
					      reg_t &addressReg1, 
					      reg_t &addressReg2)
{
   if (sizeof(kptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::store, 
					       power_instr::stWordX,
					       (power_reg&)valueReg, 
					       (power_reg&)addressReg1, 
					       (power_reg&)addressReg2);
      emit(i);
   }
   else if (sizeof(kptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::store, 
					       power_instr::stDoubleX,
					       (power_reg&)valueReg, 
					       (power_reg&)addressReg1, 
					       (power_reg&)addressReg2);
      emit(i);
   }
   else
      assert(false);
}

void power_tempBufferEmitter::emitStoreDaemonNative(reg_t &valueReg, 
					      reg_t &addressReg1, 
					      reg_t &addressReg2)
{
   if (sizeof(dptr_t) == sizeof(uint32_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::store, 
					       power_instr::stWordX,
					       (power_reg&)valueReg, 
					       (power_reg&)addressReg1, 
					       (power_reg&)addressReg2);
      emit(i);
   }
   else if (sizeof(dptr_t) == sizeof(uint64_t)) {
      instr_t *i = (instr_t *) new power_instr(power_instr::store, 
					       power_instr::stDoubleX,
					       (power_reg&)valueReg, 
					       (power_reg&)addressReg1, 
					       (power_reg&)addressReg2);
      emit(i);
   }
   else
      assert(false);
}

bool power_tempBufferEmitter::emit8ByteActualParameter(reg_t & /* srcReg64 */,
                                                 reg_t &/* paramReg */,
                                                 reg_t & /* paramReg_lowbits */) {
   assert(false);
}

// --------------------------------------------------------------------------------
// ----------------- Debugging: facility to disable instrumentation ---------------
// ------------------------------------ at runtime --------------------------------
// --------------------------------------------------------------------------------
//It's unclear how and if this is ever used... disabled on Power for now.


void power_tempBufferEmitter::
emitPlaceholderDisablingBranch(const pdstring & /* disablingBranchLabelName */) {
   assert(false && "nyi");
}

void power_tempBufferEmitter::emitDisablingCode(const pdstring & /* disablingCodeLabelName */,
                                          const pdstring & /* disablingBranchLabelName */,
                                          const pdstring & /*done_label_name*/,
                                          reg_t & /* scratch_reg0 */,
                                          reg_t & /* scratch_reg1 */) {
   assert(false && "nyi");
}

void power_tempBufferEmitter::completeDisablingCode(const pdstring & /* disablingBranchLabelName */,
                                              const pdstring & /* done_label_name */,
                                              const pdstring & /* disablingCodeLabelName */,
                                              reg_t & /* branch_contents_reg */) {
   assert(false && "nyi");
}
