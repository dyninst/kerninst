// EmitPic1.h

#ifndef _EMIT_PIC1_H_
#define _EMIT_PIC1_H_

#include "EmitPicGeneric.h"

class EmitPic1 : public EmitPicGeneric {
 private:
   // note that there is no orig_value.  We fully expect pic1 to wrap around
   // frequently, anyway.
   
 public:
   EmitPic1() {}
   EmitPic1(const EmitPic1 &src) : EmitPicGeneric(src) {}
  ~EmitPic1() {}
   
   EmitCtr *dup() const {
      return new EmitPic1(*this);
   }

   void emitReadValue(tempBufferEmitter &em,
                      unsigned numbits, // we'll truncate down to this many bits
                      sparc_reg destreg64,
                         // must be a 64-bit safe reg, unless EmitPic0's version
                      sparc_reg /* unneeded scratch reg */) const;
};

#endif
