// EmitCtr.h

#ifndef _EMIT_CTR_H_
#define _EMIT_CTR_H_

// fwd decl, instead of #include, makes for smaller .o file, especially whenever
// templates are present:
class tempBufferEmitter;
class sparc_reg;


class EmitCtr {
 private:
   
 public:
   virtual ~EmitCtr();
   virtual EmitCtr *dup() const = 0;

   static void truncateReg(tempBufferEmitter &em,
                           sparc_reg destreg64,
                           sparc_reg sourcereg64,
                           unsigned oldnumbits,
                              // meaning: bits [oldnumbits...63] of destreg64 already 0
                           unsigned newnumbits
                              // set bits [newnumbits...63] of destreg64 to zero
                           );

   virtual void emitReadValue(tempBufferEmitter &,
                              unsigned numbits, // we'll truncate down to this many
                              sparc_reg destreg,
                              sparc_reg scratchreg) const = 0;

   virtual void emitCalculationOfDeltaValue(tempBufferEmitter &,
                                            unsigned oldnumbits,
                                               // tells which bits in currtime/prevtime
                                               // were defined (higher bits assumed to
                                               // be zero).
                                            unsigned numbits,
                                               // we'll truncate down to this amt
                                               // for the result.
                                            sparc_reg destreg,
                                            sparc_reg currValReg,
                                            sparc_reg prevValReg) const = 0;
};

#endif
