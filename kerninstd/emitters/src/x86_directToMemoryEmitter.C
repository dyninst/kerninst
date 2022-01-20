// x86_directToMemoryEmitter.C

#include "x86_directToMemoryEmitter.h"
#include "x86_instr.h"

// The following isn't static because various launchers use it.  In fact it's
// so general that it should probably be moved.
void flush_a_bunch(kptr_t iaddr, unsigned numbytes)
{
   // On x86, flushing is per-cache line, and cache lines vary in size from 
   // 32 to 128 bytes. Flushing 32-byte chunks should be safe.

   kptr_t addr = iaddr;

   // make sure that numbytes is a multiple of 32, lest "numbytes -= 32"
   // eventually lead to unsigned integer underflow wraparound.
   numbytes += 32-(numbytes % 32);
   assert(numbytes % 32 == 0);

   while (numbytes) {
      //asm("clflush %0" : /* no outputs */ : "m" (addr));
      addr += 32;
      numbytes -= 32;
   } 
}

void x86_directToMemoryEmitter::complete()
{
   // We just wrote a bunch of instructions.  They just made it to the data cache.
   // We need to issue clflush instructions to make sure they reach
   // the icache
   // We could try and be clever, flushing during emit().  But flush operates
   // on 32-byte chunks, while emit() does 4 bytes, so it'd be a little messy.
   // Still may be worth trying, in case there's something to be gained by
   // locality of write/flush
   assert(insnBytesEmitted <= totalNumInsnBytesOKToWrite);
   flush_a_bunch(whereEmittingStartsInKerninstd, insnBytesEmitted);
}

void x86_directToMemoryEmitter::emitSave(bool saveNeeded,
                                         const regset_t * /*regsToSave*/)
{
   instr_t *i;
   if (saveNeeded) {
      i = new x86_instr(x86_instr::pushad);
      emit(i);
   }
   i = new x86_instr(x86_instr::pushfd);
   emit(i);
      
}

void x86_directToMemoryEmitter::emitRestore(bool saveNeeded,
                                            const regset_t */*regsToRestore*/)
{
 
   instr_t *i = new x86_instr(x86_instr::popfd);
   emit(i);
   if (saveNeeded) {
      i = new x86_instr(x86_instr::popad);
      emit(i);
   }
}
