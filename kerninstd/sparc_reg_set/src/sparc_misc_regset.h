// sparc_misc_regset.h
// Miscellaneous regs like cond codes

#ifndef _SPARC_MISC_REGSET_H_
#define _SPARC_MISC_REGSET_H_

#include "sparc_reg.h"
#include <inttypes.h>
struct XDR;

class sparc_misc_regset {
 private:
   union {
      uint32_t misc_stuff;
      struct {
#ifdef _BIG_ENDIAN_         
         unsigned padding : 22; // padding
         unsigned icc : 1; // #96
         unsigned xcc : 1;
         unsigned fcc0 : 1;
         unsigned fcc1 : 1;
         unsigned fcc2 : 1;
         unsigned fcc3 : 1;
         unsigned pc  : 1;
         unsigned gsr : 1; // gfx status reg of ultrasparcs.  #103
         unsigned asi : 1; // %asi reg of v9.  #104
         unsigned pil : 1; // %pil reg of v9.  #105
#elif defined(_LITTLE_ENDIAN_)
         unsigned pil : 1; // %pil reg of v9.  #105
         unsigned asi : 1; // %asi reg of v9.  #104
         unsigned gsr : 1; // gfx status reg of ultrasparcs.  #103
         unsigned pc  : 1;
         unsigned fcc3 : 1;
         unsigned fcc2 : 1;
         unsigned fcc1 : 1;
         unsigned fcc0 : 1;
         unsigned xcc : 1;
         unsigned icc : 1; // #96
         unsigned padding : 22; // padding
#else
#error unrecognized endianness; #define either _BIG_ENDIAN_ or _LITTLE_ENDIAN_
#endif

      } ms;
   };
   
   class Raw {};
   class Random {};
   class Empty {};
   class Full {};
   
 public:
   sparc_misc_regset() : misc_stuff(0) {}
   
   sparc_misc_regset(XDR *);
   bool send(XDR *) const;

   static Raw raw;
   sparc_misc_regset(Raw, uint32_t imisc_stuff) : misc_stuff(imisc_stuff) {
      ms.padding = 0;
   }

   sparc_misc_regset(const sparc_reg &r);

   static Random random;
   sparc_misc_regset(Random);
      // Obviously, a (seemingly crazy) routine like this is only used in
      // regression torture test programs.

   static Empty empty;
   static Full full;
   
   sparc_misc_regset(Empty) : misc_stuff(0) {}
   sparc_misc_regset(Full) {
      misc_stuff = 0xffffffff;
      ms.padding = 0;
   }
   
   sparc_misc_regset(const sparc_misc_regset &src) : misc_stuff(src.misc_stuff) {}

   void operator=(const sparc_misc_regset &src) { misc_stuff = src.misc_stuff; }

   uint32_t getRawBits() const { return misc_stuff; }

   bool operator==(const sparc_misc_regset &other) const {
      return misc_stuff == other.misc_stuff;
         // we don't mask misc_stuff by 0x1ff since it should already
         // have been done by setting 'padding' to 0 for both "this" and "other"
   }
   bool operator==(const sparc_reg &r) const;
   bool operator!=(const sparc_misc_regset &other) const {
      return misc_stuff != other.misc_stuff;
         // we don't mask misc_stuff by 0x1ff since it should have
         // been done already
   }

   bool exists(const sparc_misc_regset &subset) const {
      return 0U == (subset.misc_stuff & (~misc_stuff));
   }

   bool exists(const sparc_reg &r) const;
   
   bool existsIcc() const { return ms.icc; }
   bool existsXcc() const { return ms.xcc; }
   bool existsFcc0() const { return ms.fcc0; }
   bool existsFcc1() const { return ms.fcc1; }
   bool existsFcc2() const { return ms.fcc2; }
   bool existsFcc3() const { return ms.fcc3; }
   bool existsFcc(unsigned fccnum) const;
   bool existsPC() const { return ms.pc; }
   bool existsGSR() const { return ms.gsr; }
   bool existsASI() const { return ms.asi; }
   bool existsPIL() const { return ms.pil; }

   bool isempty() const {
      return misc_stuff == 0;
   }
   void clear() {
      misc_stuff = 0;
   }

   void operator|=(const sparc_misc_regset &other) {
      misc_stuff |= other.misc_stuff; // OK for bit-fields?
   }
   void operator&=(const sparc_misc_regset &other) {
      misc_stuff &= other.misc_stuff; // okay on bit fields?
   }
   void operator-=(const sparc_misc_regset &other) {
      misc_stuff &= (~other.misc_stuff);
      ms.padding = 0; // necessary
   }

   unsigned count() const;

   sparc_reg expand1() const;
};

#endif
