// EmitPic0.h

#ifndef _EMIT_PIC0_H_
#define _EMIT_PIC0_H_

#include "EmitPicGeneric.h"

class EmitPic0 : public EmitPicGeneric {
 private:
   // note that there is no orig_value.  We fully expect pic0 to wrap around
   // frequently, anyway.
   
 public:
   EmitPic0() {}
   EmitPic0(const EmitPic0 &src) : EmitPicGeneric(src) {}
  ~EmitPic0() {}

   EmitCtr *dup() const {
      return new EmitPic0(*this);
   }
   
   void emitReadValue(tempBufferEmitter &em,
                      unsigned numbits, // we'll truncate down to this many bits
                      sparc_reg destreg32,
                         // can be a 32-bit reg because if the high 32 bits get zero'd
                         // out w/o our knowledge, that's great because that's what
                         // we're going to do anyway.  Note that emitReadValue() for
                         // some other classes (EmitTickCtr) requires 64-bit registers;
                         // we're more the exception than the rule.
                      sparc_reg /* unneeded scratch reg */
                      ) const;
};

#endif
