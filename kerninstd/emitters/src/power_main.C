// power_main.C
// tempBufferEmitter test program for Power

#include "tempBufferEmitter.h"
#include "power_tempBufferEmitter.h"
#include "abi.h"
#include "power32_abi.h"
#include <assert.h>
#include "power_instr.h"

static void test1(const abi &myabi) {
   power_tempBufferEmitter em(myabi);
   
   assert(em.getCurrChunkNdx() == 0);
   assert(em.getCurrChunk()->numInsnBytes() == 0);
   assert(&em.getABI() == &myabi);

   assert(!em.isCompleted());
   em.complete();
   assert(em.isCompleted());

   // get1EmittedAsInsnVec() etc. can't take place before complete, so it's down here
   assert(em.get1EmittedAsInsnVec(true) == em.getCurrChunk());
   assert(em.get1EmittedAsInsnVec(true)->numInsnBytes() == 0);
}

static void test2(const abi &myabi) {
   // test2: emit a couple of insns in one chunk, with nothing to patch up.
   // tests out getCurrChunk(), emit(), offset2Insn(), get1EmittedAsInsnVec()
   power_tempBufferEmitter em(myabi);
   
   instr_t *i1 = new power_instr( power_instr::addsub, power_instr::addXO, power_reg::gpr0, power_reg::gpr3, power_reg::gpr4, 0, 0 /*no overflow or cr mods */);
   instr_t *i2 =  new power_instr(power_instr::addsub, power_instr::subXO, power_reg::gpr5, power_reg::gpr6, power_reg::gpr7,0, 0 /*no overflow or cr mods */  );
   
   em.emit(i1);
   em.emit(i2);
   
   assert(em.getCurrChunkNdx() == 0);
   assert(em.getCurrChunk()->numInsnBytes() == 8);
   assert(&em.getABI() == &myabi);

   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 0)) == i1);
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 4)) == i2);

   assert(!em.isCompleted());
   em.complete();
   assert(em.isCompleted());
   
   assert(em.get1EmittedAsInsnVec(true) == em.getCurrChunk());
   assert(em.get1EmittedAsInsnVec(true)->numInsnBytes() ==  8);

   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 0)) == i1);
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 4)) == i2);
}

static void test3(const abi &myabi) {
   // test3: like test2, but with a couple of chunks.  Still nothing needed to
   // patch up, though.  Tets out getCurrChunkNdx(), switchToChunk()
   power_tempBufferEmitter em(myabi);
   
   instr_t *i1 = new power_instr(power_instr::addsub, power_instr::addXO, power_reg::gpr0, power_reg::gpr4, power_reg::gpr5,  0, 0 /*no overflow or cr mods */);

    instr_t *i2 = new power_instr (power_instr::addsub, power_instr::subXO, power_reg::gpr6, power_reg::gpr7, power_reg::gpr8,0, 0 /*no overflow or cr mods */ );
 
   // presently on chunk 0
   assert(em.getCurrChunkNdx() == 0);
   assert(em.numChunks() == 1);

   em.beginNewChunk();

   assert(em.getCurrChunkNdx() == 1);
   assert(em.numChunks() == 2);

   em.switchToChunk(0);

   em.emit(i1);
  
   instr_t *nop = new power_instr(power_instr::nop);
   em.emit(nop);
   
   em.switchToChunk(1);
   nop = new power_instr(power_instr::nop);
   em.emit(nop);
   em.emit(i2);
   nop = new power_instr(power_instr::nop);
   em.emit(nop);

   assert(em.numChunks() == 2);
   assert(em.getCurrChunkNdx() == 1);

   // now test out offset2Insn()
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(0, 0)) == i1);
   assert((power_instr &)*(em.offset2Insn(tempBufferEmitter::insnOffset(0, 4))) ==
          power_instr(power_instr::nop));
   assert((power_instr &)*(em.offset2Insn(tempBufferEmitter::insnOffset(1, 0))) ==
          power_instr(power_instr::nop));
   assert(em.offset2Insn(tempBufferEmitter::insnOffset(1, 4)) == i2);
   assert((power_instr &)*(em.offset2Insn(tempBufferEmitter::insnOffset(1, 8))) ==
          power_instr(power_instr::nop));

   assert(!em.isCompleted());
   em.complete();
   assert(em.isCompleted());
   
   const pdvector<insnVec_t*> &theChunks = em.getEmitted(true);
   assert(theChunks.size() == 2);
   const insnVec_t *chunk1 = theChunks[0];
   const insnVec_t *chunk2 = theChunks[1];
   
   assert(chunk1->numInsnBytes() == 8);
   assert(chunk2->numInsnBytes() == 12);
   
   assert(chunk1->get_by_byteoffset(0) == i1);
   assert((power_instr &)*(chunk1->get_by_byteoffset(4)) == power_instr(power_instr::nop));
   assert((power_instr &)*(chunk2->get_by_byteoffset(0)) == power_instr(power_instr::nop));
   assert(chunk2->get_by_byteoffset(4) == i2);
   assert((power_instr &)*(chunk2->get_by_byteoffset(8)) == power_instr(power_instr::nop));
}


static void testTempBufferEmitter() {
   power32_abi myabi;
   test1(myabi);
   test2(myabi);
   test3(myabi);
}

int main(int /*argc*/, char /* **argv */) {
   testTempBufferEmitter();
   return 0;
}
