// EmitPicGeneric.C

#include "EmitPicGeneric.h"
#include "sparc_reg.h"
#include "sparc_instr.h"
#include "tempBufferEmitter.h"
#include "abi.h"

void EmitPicGeneric::
emitCalculationOfDeltaValue(tempBufferEmitter &em,
                            unsigned oldnumbits,
                               // tells which bits in currtime/prevtime
                               // were defined (higher bits assumed to
                               // be zero).
                            unsigned numbits,
                               // we'll truncate down to this amt
                               // for the result.
                            sparc_reg destreg64,
                            sparc_reg currValReg, // can be a 32-bit reg
                            sparc_reg prevValReg // can be a 32-bit reg
                            ) const {
   // Since this class deals with %pic values, which are always just 32 bits long
   // (whether pic0 or pic1), we can do the following:
   // 1) allow currValReg and prevValReg to be 32-bit registers, as opposed to 64
   // 2) max oldnumbits out at 32.  This might optimize
   // the truncateReg() call at the end of this routine.
   if (oldnumbits > 32)
      oldnumbits=32;

   // Assuming no more that 1 wraparound, the delta of two 32-bit values is
   // a 32-bit value. We must make sure that even if the result is negative, 
   // we zero out the high 32 bits. See Ari's comments below.
   if (numbits > 32)
      numbits = 32;

   // The concern is underflow when an unusual number of bits are defined.
   // (If there's no underflow, then it's easy to show that delta is simply
   // the result of a 64-bit subtract, even if less than 64 bits were defined
   // for curr and lastChange.)
   // Let us work through an example of underflow when only 16 bits are defined.
   // Say that the previous value is 199, and the newly read value is 100
   // (so rollover obviously happens).  What should the correct answer be?
   // It should be -99.  But should it be -99 sign extended to its full 64
   // bit glory (0xffff ffff ffff ff9d), or should it be -99 in some simulated
   // 16-bit-register dream world? (0xff9d).  I think it should be the latter.
   // Here is why.  When underflow occurs, we want to assume that the underlying
   // hardware counter has rolled over exactly once.  Assuming our 16 bit
   // example, that means we assume that the 16 bit underlying counter went
   // from the old value of 199, all the way up to 65535, then to 0, and then
   // up to 100.  The cumulative delta of that is (65535-199 + 1 + 100) = 65437.
   // We look at that in hex and get, 0xff9d.  That's the delta we want.
   // And that's the delta we get if we simply truncate the delta result to
   // 16 bits, without having to consider sign extension/underflow stuff.
   // 2's complement rules.
   // So: (1) we do a regular subtract (no need to sign extend to 64 bits before
   // subtracting, since we assume that the orignal values are non-negative.
   // In fact, I'd say that's the key: since *everything* [before, after, delta
   // times] are unsigned, we don't need to sign extend), and (2) a simple truncate.

   const abi &theABI = em.getABI();
   assert(theABI.isReg64Safe(destreg64));

   assert(currValReg != prevValReg);

   em.emit(sparc_instr(sparc_instr::sub,
                       currValReg, prevValReg, destreg64));

   // note that only [oldnumbits] of the subtract result are defined.
   // Of course, the register can be bigger than that, in which case the higher
   // bits will be either 0 or 1 depending on sign extension.  If they're 1, then
   // overflow has occurred.  Not that it really matters, since we're not going to
   // use any of those bits.  See the long discussion above.

   const bool guaranteedNoUnderflow = false;
   if (!guaranteedNoUnderflow)
      // due to the possibility of underflow, destreg64 may contain 1's
      // in any or all 64 bits, so it's important to set this, lest truncateReg()
      // work incorrectly
      oldnumbits = 64;
      
   truncateReg(em, destreg64, destreg64,
               oldnumbits, // used to be this many bits...
               numbits // ...truncate down to this many bits
               );
}

