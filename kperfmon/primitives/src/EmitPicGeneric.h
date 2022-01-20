// EmitPicGeneric.h
// A base class for EmitPic0 and EmitPic1, since they can share the delta routine.

#ifndef _EMIT_PIC_GENERIC_H_
#define _EMIT_PIC_GENERIC_H_

#include "EmitCtr.h"

class EmitPicGeneric : public EmitCtr {
 private:
 public:
   // no change to emitReadValue() from EmitCtr; derived class must define it
   virtual ~EmitPicGeneric() {}

   void emitCalculationOfDeltaValue(tempBufferEmitter &,
                                    unsigned oldnumbits,
                                       // tells which bits in currtime/prevtime
                                       // were defined (higher bits assumed to
                                       // be zero).
                                    unsigned numbits,
                                       // we'll truncate down to this amt
                                       // for the result.
                                    sparc_reg destreg,
                                    sparc_reg currValReg,
                                    sparc_reg prevValReg) const;
};


#endif
