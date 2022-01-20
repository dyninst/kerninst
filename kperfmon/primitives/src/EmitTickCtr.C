// EmitTickCtr.C

#include "EmitTickCtr.h"
#include "sparc_reg.h"
#include "sparc_instr.h"
#include "tempBufferEmitter.h"
#include "abi.h"

EmitTickCtr::EmitTickCtr(UseOrigValue, uint64_t iorig_value) :
   use_orig_value(true),
   orig_value(iorig_value) {}

EmitTickCtr::EmitTickCtr(DontUseOrigValue) :
   use_orig_value(false),
   orig_value(0) {}

EmitTickCtr::EmitTickCtr(const EmitTickCtr &src) : EmitCtr(src) {
   use_orig_value = src.use_orig_value;
   orig_value = src.orig_value;
}

EmitCtr *EmitTickCtr::dup() const {
   return new EmitTickCtr(*this);
}

void EmitTickCtr::emitReadValue(tempBufferEmitter &em,
                                unsigned numbits, // we'll clear bits [numbits...63]
                                sparc_reg destreg64, // must be a 64-bit reg
                                sparc_reg scratchreg64 // must be a 64-bit reg, too
   ) const {
   const abi &theABI = em.getABI(); 
   assert(theABI.isReg64Safe(destreg64));
   assert(theABI.isReg64Safe(scratchreg64));
   assert(destreg64 != scratchreg64);
      
   if (use_orig_value) {
      // scratchreg64 <-- orig_value:
      em.emitImm64(orig_value,
		   destreg64, // use this reg as a temp
		   scratchreg64); // put orig_value here
   }
      
   em.emit(sparc_instr(sparc_instr::readstatereg,
                       4, // 4 --> %tick
                       destreg64));

   if (use_orig_value)
      // destreg <-- destreg - scratchreg64:
      em.emit(sparc_instr(sparc_instr::sub, destreg64, scratchreg64, destreg64));

   // Now make sure everything fits in [numbits] bits:
   truncateReg(em,
               destreg64, // dest of truncation
               destreg64, // source of truncation
               64, // this many bits are defined...
               numbits // ...truncate it down to this many bits
               ); 
}
   
void EmitTickCtr::
emitCalculationOfDeltaValue(tempBufferEmitter &em,
                            unsigned oldnumbits,
                               // this many bits are defined, in BOTH current
                               // time and previous time regs
                            unsigned numbits,
                               // ...make delta fit in this many bits
                            sparc_reg destreg64,
                            sparc_reg currentTimeReg64,
                            sparc_reg lastChangeTimeReg64) const {
   const abi& theABI = em.getABI();
   assert(theABI.isReg64Safe(destreg64));
   assert(theABI.isReg64Safe(currentTimeReg64));
   assert(theABI.isReg64Safe(lastChangeTimeReg64));
   assert(currentTimeReg64 != lastChangeTimeReg64);
      
   em.emit(sparc_instr(sparc_instr::sub,
                       currentTimeReg64, lastChangeTimeReg64,
                       destreg64 // dest
      ));

   const bool guaranteedNoUnderflow = false;
   if (!guaranteedNoUnderflow)
      // since underflow might have happened, we must assume that
      // destreg64 may contain 1's in any or all of its 64 bits.
      // So it's important to do this, lest truncateReg() do the wrong thing.
      oldnumbits = 64;
      
   truncateReg(em,
               destreg64, // dest of truncation
               destreg64, // source of truncation
               oldnumbits, // this many bits are defined...
               numbits // ...truncate it down to this many bits
               );
}
