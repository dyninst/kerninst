// tempBufferEmitter.C
// see .h file for comments

#include "tempBufferEmitter.h"
#include "util/h/Dictionary.h"
#include "util/h/hashFns.h" // stringHash
#include "common/h/triple.h"
#include "instr.h" // change to instr.h if class is made arch indep
#include "abi.h"

const pdstring tempBufferEmitter::this_chunk_addr("{{thisChunkAddr!}}");

tempBufferEmitter::tempBufferEmitter(const abi &iabi) :
         theABI(iabi),
         labels(stringHash),
         knownSymAddrs(stringHash) {
   insnVec_t **iv_ptr = theChunks.append_with_inplace_construction();
   *iv_ptr = insnVec_t::getInsnVec(); // default insnVec ctor for this new chunk

   assert(theChunks.size() == 1);
   currChunkNdx = 0;
   
   completed = false;
}

tempBufferEmitter::~tempBufferEmitter() {
    for (unsigned chunk_lcv=0; chunk_lcv < theChunks.size(); ++chunk_lcv) {
       delete theChunks[chunk_lcv];
    } 
}

const abi &tempBufferEmitter::getABI() const {
   return theABI;
}

// ----------------------------------------------------------------------


const pdvector<insnVec_t*>&
tempBufferEmitter::getEmitted(bool mustBeReadyToExecute) const {
   assert(completed && "cannot getEmitted() until completed");
   if (mustBeReadyToExecute)
      assert(isReadyToExecute());
   
   return theChunks;
}

insnVec_t*
tempBufferEmitter::get1EmittedAsInsnVec(bool mustBeReadyToExecute) const {
   assert(completed && "cannot get1EmittedAsInsnVec() until completed");
   if (mustBeReadyToExecute)
      assert(isReadyToExecute());

   assert(theChunks.size() == 1 && currChunkNdx == 0);
   return theChunks[0];
}

// ----------------------------------------------------------------------

void tempBufferEmitter::switchToChunk(unsigned chunk_ndx) {
   currChunkNdx = chunk_ndx;
   assert(currChunkNdx < theChunks.size());
}

void tempBufferEmitter::beginNewChunk() {
   insnVec_t **iv_ptr = theChunks.append_with_inplace_construction();
   *iv_ptr = insnVec_t::getInsnVec(); // default insnVec ctor for this new chunk

   assert(theChunks.size() >= 2); // yes, 2

   currChunkNdx = theChunks.size() - 1;
}

// ----------------------------------------------------------------------

instr_t* tempBufferEmitter::offset2Insn(const insnOffset &off) {
   return theChunks[off.chunkNdx]->get_by_byteoffset(off.byteOffset);
}

// ----------------------------------------------------------------------

const dictionary_hash<pdstring, tempBufferEmitter::insnOffset>&
tempBufferEmitter::getLabels() const {
   return labels;
}

const pdvector< pair<tempBufferEmitter::insnOffset, pdstring> >&
tempBufferEmitter::getUnresolvedBranchesToLabels() const {
   return unresolvedBranchesToLabels;
}

const pdvector< pair<tempBufferEmitter::insnOffset, pdstring> >&
tempBufferEmitter::getUnresolvedInterprocBranchesToName() const {
   return unresolvedInterprocBranchesToName;
}

const pdvector< pair<tempBufferEmitter::insnOffset, kptr_t> >&
tempBufferEmitter::getUnresolvedInterprocBranchesToAddr() const {
   return unresolvedInterprocBranchesToAddr;
}

const pdvector< pair<tempBufferEmitter::insnOffset, pdstring> >&
tempBufferEmitter::getUnresolvedCallsByName() const {
   return unresolvedCallsByName;
}

const pdvector< pair<tempBufferEmitter::insnOffset, kptr_t> >&
tempBufferEmitter::getUnresolvedCallsByAddr() const {
   return unresolvedCallsByAddr;
}

const pdvector< pair<tempBufferEmitter::insnOffset, pdstring> >&
tempBufferEmitter::getUnresolvedSymAddrs() const {
   return unresolvedSymAddrs;
}

const pdvector< pair<tempBufferEmitter::insnOffset, pdstring> >&
tempBufferEmitter::getUnresolvedLabelAddrsAsData() const {
   return unresolvedLabelAddrsAsData;
}

const pdvector< pair<tempBufferEmitter::insnOffset, triple<pdstring, pdstring, unsigned> > >&
tempBufferEmitter::getUnresolvedLabelAddrMinusLabelAddrThenDivides() const {
   return unresolvedLabelAddrMinusLabelAddrThenDivides;
}

const pdvector< pair<tempBufferEmitter::insnOffset, pdstring> >&
tempBufferEmitter::getUnresolvedLabelAddrs() const {
   return unresolvedLabelAddrs;
}

const dictionary_hash<pdstring, kptr_t>&
tempBufferEmitter::getKnownSymAddrs() const {
   // This routine is needed because resolving pc-dependent things (like calls)
   // require last-minute resolution, when insnAddrs are known; they're not known
   // in this class.
   // sym name --> address
   return knownSymAddrs;
}

// ----------------------------------------------------------------------

void tempBufferEmitter::emit(instr_t *i) {
   assert(!completed && "cannot emit() after calling complete()");
   theChunks[currChunkNdx]->appendAnInsn(i);
}

// Several functions in the sparc_instr class generate sequences of 
// instructions. Lets emit them as they are.
void tempBufferEmitter::emitSequence(insnVec_t *iv)
{
   insnVec_t::const_iterator iter;
   
   for (iter = iv->begin(); iter != iv->end(); iter++) {
      instr_t *instr = instr_t::getInstr(**iter);
      emit(instr);
   }
   delete iv;
}

void tempBufferEmitter::emitThisChunkAddr(reg_t &rd) {
   emitSymAddr(this_chunk_addr, rd);
}

tempBufferEmitter::insnOffset
tempBufferEmitter::queryLabelOffset(const pdstring &labelName) const {
   return labels.get(labelName);
}



// --------------------------------------------------------------------------------
// ----------------------------- Emitting Routines --------------------------------
// --------------------------------------------------------------------------------

void tempBufferEmitter::emitLabel(const pdstring &labelName) {
   assert(!completed && "cannot emitLabel() after calling complete()");
   labels.set(labelName, currInsnOffset());
}

void tempBufferEmitter::emitCondBranchToForwardLabel(instr_t *i,
                                                     const pdstring &fwdLabelName) {
   // If the label name existed already, we wouldn't need to be using this.
   assert(!labels.defines(fwdLabelName));

   unresolvedBranchesToLabels += make_pair(currInsnOffset(), fwdLabelName);
   emit(i);
}

void tempBufferEmitter::emitCondBranchToLabel(instr_t *i,
                                              const pdstring &labelName) {
   unresolvedBranchesToLabels += make_pair(currInsnOffset(), labelName);
   emit(i);
}

void tempBufferEmitter::emitInterprocCondBranch(instr_t *i,
                                                const pdstring &destName) {
   // destName is NOT a label; it's a symbol name.  Use makeSymAddr32Known() on it
   // in order to resolve successfully.

   unresolvedInterprocBranchesToName += make_pair(currInsnOffset(), destName);
   emit(i);
}

void tempBufferEmitter::emitInterprocCondBranch(instr_t *i,
                                                kptr_t destAddr) {
   unresolvedInterprocBranchesToAddr += make_pair(currInsnOffset(), destAddr);
   emit(i);
}

// Emit the worst-case sequence of instructions that call "to"
// 32-bit version: emit "call disp", hope it is not that far
// 64-bit version: emit "genImmAddr(hi(to)) -> o7, jmpl o7+lo(to),o7"
// as usual, we don't know fromAddr (we're emitting *relocatable* code)
void tempBufferEmitter::emitCallGeneric_unknownFromAddr(kptr_t to) {
   insnVec_t *iv = insnVec_t::getInsnVec();
   
   instr_t::genCallAndLinkGeneric_unknownFromAddr(iv, to);
   emitSequence(iv);
}

void tempBufferEmitter::emitCallGeneric_unknownFromAndToAddr() {
   insnVec_t *iv = insnVec_t::getInsnVec();
   instr_t::genCallAndLinkGeneric_unknownFromAndToAddr(iv);
   emitSequence(iv);
}

// Emit either "call disp" or
// "genImmAddr(hi(to)) -> o7, jmpl o7+lo(to),o7" if the displacement
// does not fit in 30 bits
void tempBufferEmitter::emitCall(kptr_t from, kptr_t to) {
   insnVec_t *iv = insnVec_t::getInsnVec();
   instr_t::genCallAndLink(iv, from, to);
   emitSequence(iv);
}

void tempBufferEmitter::emitCall(const pdstring &calleeName) {
   unresolvedCallsByName += make_pair(currInsnOffset(), calleeName);
   // We have to put a full, non-optimized version for now, however 
   // it may be replaced with a single call instruction, padded with 
   // nop's at the resolution time
   emitCallGeneric_unknownFromAndToAddr();
}

void tempBufferEmitter::emitCall(kptr_t calleeAddr) {
   unresolvedCallsByAddr += make_pair(currInsnOffset(), calleeAddr);
   // We have to put a full, non-optimized version for now (we don't
   // know the pc yet), however it may be replaced with a single call
   // instruction, padded with nop's at the resolution time
   emitCallGeneric_unknownFromAddr(calleeAddr);
}

void tempBufferEmitter::emitSymAddr(const pdstring &symbolName, reg_t &rd) 
{
   // The vast majority of the time, "symbolName" won't be known/resolved
   // until way after complete() is called.  But sometimes, the symbol becomes
   // known, and we can call makeSymAddrKnown() before complete().  Very unusual
   // to do so, however.
   unresolvedSymAddrs += make_pair(currInsnOffset(), symbolName);

   // Emit dummy instructions as placeholders
   kptr_t zero = 0;
   insnVec_t *iv = insnVec_t::getInsnVec();
   
   instr_t::genImmAddrGeneric(iv, zero, rd);
   emitSequence(iv);
}

// Emit instructions for loading an address of the label into the given 
// register. We do not know the actual address yet, so we have to emit
// a dummy version.
void tempBufferEmitter::emitLabelAddr(const pdstring &labelName, reg_t &rd) 
{
   unresolvedLabelAddrs += make_pair(currInsnOffset(), labelName);

   // Emit dummy instructions as placeholders
   kptr_t zero = 0;
   insnVec_t *iv = insnVec_t::getInsnVec();
   
   instr_t::genImmAddrGeneric(iv, zero, rd);
   emitSequence(iv);
}

void tempBufferEmitter::makeSymAddrKnown(const pdstring &symbolName,
					 kptr_t value) {
   assert(!completed && "cannot makeSymAddrKnown() after calling complete()");
   knownSymAddrs.set(symbolName, value);
}

void tempBufferEmitter::emitImm32(uint32_t num, reg_t &rd) 
{
   insnVec_t *iv = insnVec_t::getInsnVec();
   
   instr_t::gen32imm(iv, num, rd);
   emitSequence(iv);
}

void tempBufferEmitter::emitImm64(uint64_t num, reg_t &scratch, 
				  reg_t &rd) 
{
   insnVec_t *iv = insnVec_t::getInsnVec();
   
   instr_t::gen64imm(iv, num, rd, scratch);
   emitSequence(iv);
}

void tempBufferEmitter::emitImmAddr(uint32_t addr, reg_t &rd) {
   emitImm32(addr, rd);
}

void tempBufferEmitter::emitImmAddr(uint64_t addr, reg_t &rd) {
   insnVec_t *iv = insnVec_t::getInsnVec();
   instr_t::genImmAddr(iv, addr, rd);
   emitSequence(iv);
}

void tempBufferEmitter::emitString(const pdstring &str) {
   // emit the string as fake instructions; this ensures alignment.
   const unsigned len = str.length();
   unsigned nwords = len / 4;
   if (len % 4)
      ++nwords;

   unsigned cndx = 0;
   while (nwords--) {
      char c0 = '\0', c1='\0', c2='\0', c3='\0';
      if (cndx < len) {
         c0 = str[cndx++];
         if (cndx < len) {
            c1 = str[cndx++];
            if (cndx < len) {
               c2 = str[cndx++];
               if (cndx < len) {
                  c3 = str[cndx++];
               }
            }
         }
      }
      unsigned raw_insn;
      rollin(raw_insn, uimmediate<8>(c0));
      rollin(raw_insn, uimmediate<8>(c1));
      rollin(raw_insn, uimmediate<8>(c2));
      rollin(raw_insn, uimmediate<8>(c3));

      instr_t *the_insn = const_cast<instr_t*>(&instr_t::getInstr(raw_insn));
         //note that the number of bytes in this "instruction" is 4 no matter what.
      //Would it be useful on x86 to take the size of these fake instructions as a param to this method?
      emit(the_insn);
   }
}
