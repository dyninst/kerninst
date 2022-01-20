// main.C
// tempBufferEmitter test program

#include "tempBufferEmitter.h"
#include "abi.h"
#include "sparcv8plus_abi.h"
#include <assert.h>
#include "sparc_instr.h"

typedef insnVec<sparc_instr> insnVec_t;

static void test1(const abi &myabi) {
   tempBufferEmitter em(myabi);
   
   assert(em.getCurrChunkNdx() == 0);
   assert(em.getCurrChunk().numInsnBytes() == 0);
   assert(&em.getABI() == &myabi);

   assert(!em.isCompleted());
   em.complete();
   assert(em.isCompleted());

   // get1EmittedAsInsnVec() etc. can't take place before complete, so it's down here
   assert(&em.get1EmittedAsInsnVec(true) == &em.getCurrChunk());
   assert(em.get1EmittedAsInsnVec(true).numInsnBytes() == 0);
}

static void test2(const abi &myabi) {
   // test2: emit a couple of insns in one chunk, with nothing to patch up.
   // tests out getCurrChunk(), emit(), offset2Insn(), get1EmittedAsInsnVec()
   tempBufferEmitter em(myabi);
   
   const sparc_instr i1(sparc_instr::add, sparc_reg::o0, sparc_reg::o1, sparc_reg::o2);
   const sparc_instr i2(sparc_instr::sub, sparc_reg::l0, sparc_reg::l1, sparc_reg::l2);
   
   em.emit(i1);
   em.emit(i2);
   
   assert(em.getCurrChunkNdx() == 0);
   assert(em.getCurrChunk().numInsnBytes() == 8);
   assert(&em.getABI() == &myabi);

   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 0)) == i1);
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 4)) == i2);

   assert(!em.isCompleted());
   em.complete();
   assert(em.isCompleted());
   
   assert(&em.get1EmittedAsInsnVec(true) == &em.getCurrChunk());
   assert(em.get1EmittedAsInsnVec(true).numInsnBytes() == 8);

   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 0)) == i1);
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 4)) == i2);
}

static void test3(const abi &myabi) {
   // test3: like test2, but with a couple of chunks.  Still nothing needed to
   // patch up, though.  Tets out getCurrChunkNdx(), switchToChunk()
   tempBufferEmitter em(myabi);
   
   const sparc_instr i1(sparc_instr::add, sparc_reg::o0, sparc_reg::o1, sparc_reg::o2);
   const sparc_instr i2(sparc_instr::sub, sparc_reg::l0, sparc_reg::l1, sparc_reg::l2);

   // presently on chunk 0
   assert(em.getCurrChunkNdx() == 0);
   assert(em.numChunks() == 1);

   em.beginNewChunk();

   assert(em.getCurrChunkNdx() == 1);
   assert(em.numChunks() == 2);

   em.switchToChunk(0);

   em.emit(i1);
   em.emit(sparc_instr(sparc_instr::nop));
   
   em.switchToChunk(1);

   em.emit(sparc_instr(sparc_instr::nop));
   em.emit(i2);
   em.emit(sparc_instr(sparc_instr::nop));

   assert(em.numChunks() == 2);
   assert(em.getCurrChunkNdx() == 1);

   // now test out offset2Insn()
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 0)) == i1);
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 4)) ==
          sparc_instr(sparc_instr::nop));
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(1, 0)) ==
          sparc_instr(sparc_instr::nop));
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(1, 4)) == i2);
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(1, 8)) ==
          sparc_instr(sparc_instr::nop));

   assert(!em.isCompleted());
   em.complete();
   assert(em.isCompleted());
   
   const pdvector<insnVec_t> &theChunks = em.getEmitted(true);
   assert(theChunks.size() == 2);
   const insnVec_t &chunk1 = theChunks[0];
   const insnVec_t &chunk2 = theChunks[1];
   
   assert(chunk1.numInsnBytes() == 8);
   assert(chunk2.numInsnBytes() == 12);
   
   assert(chunk1.get_by_byteoffset(0) == i1);
   assert(chunk1.get_by_byteoffset(4) == sparc_instr(sparc_instr::nop));
   assert(chunk2.get_by_byteoffset(0) == sparc_instr(sparc_instr::nop));
   assert(chunk2.get_by_byteoffset(4) == i2);
   assert(chunk2.get_by_byteoffset(8) == sparc_instr(sparc_instr::nop));
}


static void testTempBufferEmitter() {
   sparcv8plus_abi myabi;
   test1(myabi);
   test2(myabi);
   test3(myabi);
}

int main(int /*argc*/, char /* **argv */) {
   testTempBufferEmitter();
}
