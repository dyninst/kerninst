// EmitTickCtr.h

#ifndef _EMIT_TICK_CTR_H_
#define _EMIT_TICK_CTR_H_

#include <inttypes.h>
#include "EmitCtr.h"

class EmitTickCtr : public EmitCtr {
 private:
   bool use_orig_value;
   uint64_t orig_value;
   
 public:
   class UseOrigValue {};
   class DontUseOrigValue {};

   EmitTickCtr(UseOrigValue, uint64_t iorig_value);
   EmitTickCtr(DontUseOrigValue);
   EmitTickCtr(const EmitTickCtr &src);
   
   EmitCtr *dup() const;

   void emitReadValue(tempBufferEmitter &em,
                      unsigned numbits, // we'll clear bits [numbits...63]
                      sparc_reg destreg64, // must be a 64-bit reg
                      sparc_reg scratchreg64 // must be a 64-bit reg, too
                      ) const;

   void emitCalculationOfDeltaValue(tempBufferEmitter &em,
                                    unsigned oldnumbits,
                                       // this many bits are defined, in BOTH current
                                       // time and previous time regs
                                    unsigned numbits,
                                       // ...make delta fit in this many bits
                                    sparc_reg destreg64,
                                    sparc_reg currentTimeReg64,
                                    sparc_reg lastChangeTimeReg64) const;
};

#endif
