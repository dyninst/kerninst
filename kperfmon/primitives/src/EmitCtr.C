// EmitCtr.C

#include "EmitCtr.h"
#include "sparc_instr.h"
#include "tempBufferEmitter.h"

EmitCtr::~EmitCtr() {}

void EmitCtr::truncateReg(tempBufferEmitter &em,
                          sparc_reg destreg64,
                          sparc_reg sourcereg64,
                          unsigned oldnumbits,
                             // meaning: bits [oldnumbits...63] of destreg64
                             // are already zero
                          unsigned newnumbits
                             // set bits [newnumbits...63] of destreg64 to zero
                          ) {
   // a static method

   if (oldnumbits <= newnumbits) {
      // there's nothing to truncate.  Only need a move
      if (destreg64 != sourcereg64)
         em.emit(sparc_instr(sparc_instr::mov, sourcereg64, destreg64));
   }
   else if (newnumbits == 32) {
      // this special case can be achieved through a srl by 0 bytes
      em.emit(sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
                          false, // not extended
                          destreg64, // dest
                          sourcereg64, 0));
   }
   else {
      const unsigned shiftcount = 64 - newnumbits;
      em.emit(sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                          true, // extended
                          destreg64, // dest
                          sourcereg64, shiftcount));
      em.emit(sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
                          true, // extended
                          destreg64, // dest
                          destreg64, shiftcount));
   }
}
