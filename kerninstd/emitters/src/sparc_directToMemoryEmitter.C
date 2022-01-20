// sparc_directToMemoryEmitter.C

#include "sparc_directToMemoryEmitter.h"
#include "sparc_instr.h"
extern void flush_a_little(kptr_t); // .s file
extern abi *theKernelABI;

// The following isn't static because various launchers use it.  In fact it's
// so general that it should probably be moved.
void flush_a_bunch(kptr_t iaddr, unsigned numbytes) {
   // flush requires 4-byte alignment.  We don't enforce this (by nand'ing with 0x3)
   // because we know that we're already 4-byte aligned.
   // Now for the next wrinkle.  flush implicitly operates on the aligned 8-byte
   // entity in which 'addr' is contained.  So it'd be a waste to issue a flush for
   // each (4-byte) instruction we wrote.

   register kptr_t addr = iaddr;

   // make sure that numbytes is a multiple of 8, lest "numbytes-=8" eventually lead to
   // unsigned integer underflow wraparound.
   numbytes += 8-(numbytes % 8);
   assert(numbytes % 8 == 0);

   while (numbytes) {
//      flush_a_little(addr); // asm
      asm("flush %0" : /* no outputs */ : "r" (addr));
      addr += 8;
      numbytes -= 8;
   } 
}

void sparc_directToMemoryEmitter::complete() {
   // We just wrote a bunch of instructions.  They just made it to the data cache.
   // We need to issue SPARC flush instructions to make sure they reach
   // the icache
   // We could try and be clever, flushing during emit().  But flush operates
   // on 8-byte chunks, while emit() does 4 bytes, so it'd be a little messy.
   // Still may be worth trying, in case there's something to be gained by
   // locality of write/flush
   assert(insnBytesEmitted <= totalNumInsnBytesOKToWrite);
   flush_a_bunch(whereEmittingStartsInKerninstd, insnBytesEmitted);
}

void sparc_directToMemoryEmitter::emitSave(bool saveNeeded,
                                           const regset_t * /*regsToSave */) {
   if (saveNeeded) {
      instr_t *i = new sparc_instr(sparc_instr::save, 
                                   -theKernelABI->getMinFrameAlignedNumBytes());
      emit(i);
   }
}
void sparc_directToMemoryEmitter::emitRestore( bool saveNeeded,
                                               const regset_t *regsToRestore) {
   if(saveNeeded) {
      instr_t *i = new sparc_instr(sparc_instr::restore);
      emit(i);
   }
}
