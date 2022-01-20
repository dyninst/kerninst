// EmitPic0.C

#include "EmitPic0.h"
#include "sparc_instr.h"
#include "tempBufferEmitter.h"

void EmitPic0::
emitReadValue(tempBufferEmitter &em,
              unsigned numbits, // we'll truncate down to this many bits
              sparc_reg destreg32,
                 // can be a 32-bit reg because if the high 32 bits get zero'd
                 // out w/o our knowledge, that's great because that's what
                 // we're going to do anyway.  Note that emitReadValue() for
                 // some other classes (EmitTickCtr) requires 64-bit registers;
                 // we're more the exception than the rule.
              sparc_reg /* unneeded scratch reg */
   ) const {
   em.emit(sparc_instr(sparc_instr::readstatereg,
                       17, // 17 --> %pic
                       destreg32));
   em.emit(sparc_instr(sparc_instr::shift,
                       sparc_instr::rightLogical,
                       false, // not extended
                       destreg32, // dest
                       destreg32, 0));
   // shift right logical, not extended, by 0 bits has the effect of
   // zero'ing out the high 32 bits.  That's exactly what we want here.

   truncateReg(em,
               destreg32, // source of truncation
               destreg32, // dest of truncation
               32, // this many bits are presently defined...
               numbits // ...truncate down to this many bits
               );
}
