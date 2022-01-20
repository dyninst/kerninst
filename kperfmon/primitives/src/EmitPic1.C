// EmitPic1.C

#include "EmitPic1.h"
#include "sparc_instr.h"
#include "tempBufferEmitter.h"
#include "abi.h"

void EmitPic1::emitReadValue(tempBufferEmitter &em,
                             unsigned numbits, // we'll truncate down to this many bits
                             sparc_reg destreg64,
                                // must be a 64-bit safe reg, unless EmitPic0's version
                             sparc_reg /* unneeded scratch reg */) const {
   const abi& theABI = em.getABI();
   assert(theABI.isReg64Safe(destreg64));

   em.emit(sparc_instr(sparc_instr::readstatereg,
                       17, // 17 --> %pic
                       destreg64));
   em.emit(sparc_instr(sparc_instr::shift,
                       sparc_instr::rightLogical,
                       true, // extended
                       destreg64, // dest
                       destreg64, 32));

   truncateReg(em,
               destreg64, // source of truncation
               destreg64, // dest of truncation
               32, // this many bits are presently defined...
               numbits // ...truncate down to this many bits
               );
}
