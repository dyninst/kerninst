// x86_tempBufferEmitter.C

#include "x86_tempBufferEmitter.h"
#include <pair.h>
#include "x86_instr.h"
#include "x86_reg.h"
#include "abi.h"

x86_tempBufferEmitter::x86_tempBufferEmitter(const abi &iabi) : 
   tempBufferEmitter(iabi) {}

// --------------------
bool x86_tempBufferEmitter::isReadyToExecute() const {
   return unresolvedBranchesToLabels.size() == 0 &&
          unresolvedCallsByName.size() == 0 &&
          unresolvedCallsByAddr.size() == 0 &&
          unresolvedSymAddrs.size() == 0 &&
          unresolvedLabelAddrsAsData.size() == 0 &&
          unresolvedLabelAddrMinusLabelAddrThenDivides.size() == 0 &&
          unresolvedLabelAddrs.size();
}

// --------------------

void x86_tempBufferEmitter::
emitLabelAddrAsData(const pdstring &labelName) {
   assert(!"x86_tempBufferEmitter::emitLabelAddrAsData() - not supported on x86\n");
}

void x86_tempBufferEmitter::
emitLabelAddrMinusLabelAddrDividedBy(const pdstring &/*label1*/,
                                     const pdstring &/*label2*/,
                                     unsigned /*divideByThis*/) {
   assert(!"x86_tempBufferEmitter::emitLabelAddrMinusLabelAddrDividedBy() - not supported on x86\n");
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
void x86_tempBufferEmitter::resolve1SetImmAddr(const insnOffset& offs, 
					       kptr_t addr)
{
   assert(!"x86_tempBufferEmitter::resolve1SetImmAddr() - not supported on x86\n");
}

// ----------------------------------------------------------------------
void x86_tempBufferEmitter::reset() {
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

void x86_tempBufferEmitter::fixUpUnresolvedBranchesToLabels() {
   // We can only resolve branches to labels in the same "chunk", because 
   // otherwise we don't know the offset.

   pdvector< pair<insnOffset,pdstring> >::iterator iter = unresolvedBranchesToLabels.begin();

   while (iter != unresolvedBranchesToLabels.end()) {
      // NOTE: it's vital to re-read unresolvedBranchesToLabels.end() each 
      // time thru the loop, since we're deleting from the vector.
      const insnOffset &branchinsn_off = iter->first;
      const pdstring &labelName = iter->second;

      const insnOffset &label_off = labels.get(labelName);
      
      if (branchinsn_off.chunkNdx != label_off.chunkNdx) {
         // can't resolve this branch because it's a cross-chunk branch.
         ++iter;
         continue;
      }

      instr_t *i = offset2Insn(branchinsn_off);      
      const int insnbytes_delta = (int)label_off.byteOffset -
                                  ((int)branchinsn_off.byteOffset + i->getNumBytes());

      i->changeBranchOffset(insnbytes_delta);

      // We've resolved this unresolvedBranchesToLabels[] entry, so remove it
      *iter = unresolvedBranchesToLabels.back();
      unresolvedBranchesToLabels.pop_back();
      // note that we do NOT "++iter" in this case.
   }
}

void x86_tempBufferEmitter::fixUpUnresolvedInterprocBranchesToName() {
   // In order to fix up a branch, we need to know the offset.
   // We don't have such information here.  More specifically, we might know
   // the exact address of the destination, but without our exact address, 
   // we can't know the offset.  The other possibility is not knowing either 
   // exact address but knowing the exact distance between them; we don't 
   // have that information here either.
}

void x86_tempBufferEmitter::fixUpUnresolvedInterprocBranchesToAddr() {
   // can't do anything in this class since we don't yet know insnAddr's
}

void x86_tempBufferEmitter::fixUpUnresolvedLabelAddrs() {
   // We cannot resolve labelAddrs in this class, since we don't have any 
   // absolute insn addresses. See fixUpKnownCalls() for a similar situation.
}

void x86_tempBufferEmitter::fixUpUnresolvedLabelAddrsAsData() {
   // can't fix anything up here, since absolute addresses of the labels
   // aren't known until fully resolved
}

void x86_tempBufferEmitter::fixUpLabelAddrMinusLabelAddrDividedBys() {
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
      assert(!"x86_tempBufferEmitter::fixUpLabelAddrMinusLabelAddrDividedBys() - not supported on x86\n");

      *iter = unresolvedLabelAddrMinusLabelAddrThenDivides.back();
      unresolvedLabelAddrMinusLabelAddrThenDivides.pop_back();

      // note that we do NOT "++iter" in this case
   }

}

void x86_tempBufferEmitter::fixUpKnownSymAddrs() {
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

void x86_tempBufferEmitter::fixUpKnownCalls() {
   // This can only be done if we can know, for any given emitted instruction,
   // what its final emitted address will be.  This is because call instructions
   // are pc-relative.  Thus, we can't do anything in this class.
   // relocatableCode, on the other hand, can help, since one of its methods
   // takes in an emitAddr.
}

void x86_tempBufferEmitter::complete() {
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

   completed = true;
}

// emits the ld or ldx operation depending on the native pointer type
void x86_tempBufferEmitter::emitLoadKernelNative(reg_t &srcAddrReg, 
						 signed offs, 
						 reg_t &rd) 
{
   assert(!"x86_tempBufferEmitter::emitLoadKernelNative(reg,int,reg) nyi\n");
}

void x86_tempBufferEmitter::emitLoadDaemonNative(reg_t &srcAddrReg, 
						 signed offs, 
						 reg_t &rd) 
{
   assert(!"x86_tempBufferEmitter::emitLoadDaemonNative(reg,int,reg) nyi\n");
}

// emits the ld or ldx operation depending on the native pointer type
void x86_tempBufferEmitter::emitLoadKernelNative(reg_t &srcAddrReg1, 
						 reg_t &srcAddrReg2, 
						 reg_t &rd)
{
   assert(!"x86_tempBufferEmitter::emitLoadKernelNative(reg,reg,reg) nyi\n");
}

void x86_tempBufferEmitter::emitLoadDaemonNative(reg_t &srcAddrReg1, 
						 reg_t &srcAddrReg2, 
						 reg_t &rd)
{
   assert(!"x86_tempBufferEmitter::emitLoadDaemonNative(reg,reg,reg) nyi\n");
}

// emits the st or stx operation depending on the native pointer type
void x86_tempBufferEmitter::emitStoreKernelNative(reg_t &valueReg, 
						  reg_t &addressReg, 
						  signed offset)
{
   assert(!"x86_tempBufferEmitter::emitStoreKernelNative(reg,reg,int) nyi\n");
}

void x86_tempBufferEmitter::emitStoreDaemonNative(reg_t &valueReg, 
						  reg_t &addressReg, 
						  signed offset)
{
   assert(!"x86_tempBufferEmitter::emitStoreDaemonNative(reg,reg,int) nyi\n");
}

// emits the st or stx operation depending on the native pointer type
void x86_tempBufferEmitter::emitStoreKernelNative(reg_t &valueReg, 
						  reg_t &addressReg1, 
						  reg_t &addressReg2)
{
   assert(!"x86_tempBufferEmitter::emitStoreKernelNative(reg,reg,reg) nyi\n");
}

void x86_tempBufferEmitter::emitStoreDaemonNative(reg_t &valueReg, 
						  reg_t &addressReg1, 
						  reg_t &addressReg2)
{
   assert(!"x86_tempBufferEmitter::emitStoreDaemonNative(reg,reg,reg) nyi\n");
}

bool x86_tempBufferEmitter::emit8ByteActualParameter(reg_t & srcReg64,
						     reg_t & paramReg,
						     reg_t & paramReg_lowbits)
{
   // This one's a little complex, so pay attention: we have a 64-bit value
   // already in srcReg64, and we want to move it to paramReg (a parameter
   // to some procedure that's about to be called).  paramReg may equal 
   // srcReg64.
   // If the ABI thinks that the 8-byte parameter can indeed fit into a
   // single register, then this routine emits code to move srcReg64
   // to paramReg and returns true.  If not, then this routine emits code 
   // to split srcReg64 into two 4-byte registers: paramReg (gets the most 
   // significant 32 bits) and paramReg_lowbits (gets the least significant
   // 32 bits) and returns false.
   assert(!"x86_tempBufferEmitter::emit8ByteActualParameter() nyi\n");
}

// -----------------------------------------------------------------------
// ------ Debugging: facility to disable instrumentation at runtime ------
// -----------------------------------------------------------------------
void x86_tempBufferEmitter::
emitPlaceholderDisablingBranch(const pdstring &disablingBranchLabelName) 
{
   assert(!"x86_tempBufferEmitter::emitPlaceholderDisablingBranch() nyi\n");
}

void x86_tempBufferEmitter::
emitDisablingCode(const pdstring &disablingCodeLabelName,
		  const pdstring &disablingBranchLabelName,
		  const pdstring &done_label_name,
		  reg_t & scratch_reg0,
		  reg_t & scratch_reg1)
{
   assert(!"x86_tempBufferEmitter::emitDisablingCode() nyi\n");
}

void x86_tempBufferEmitter::
completeDisablingCode(const pdstring &disablingBranchLabelName,
		      const pdstring &done_label_name,
		      const pdstring &disablingCodeLabelName,
		      reg_t & branch_contents_reg)
{
   assert(!"x86_tempBufferEmitter::completeDisablingCode() nyi\n");
}
