// sparc_int_regset.h

#ifndef _SPARC_INT_REGSET_H_
#define _SPARC_INT_REGSET_H_

#include "sparc_reg.h"
#include <inttypes.h>
struct XDR;

class sparc_int_regset {
 private:
   uint32_t int_bits;
      // bit 0 (least significant) represents g0...bit 31 represents %r31.
      // see sparc_reg.h/.C for details on register assignment.

   class Raw {};
   class Random {};
   class Empty {};
   class Full {};
   class Save {};
   class SingleIntReg {};
   
 public:
   sparc_int_regset() : int_bits(0) {}
   sparc_int_regset(const sparc_int_regset &src) : int_bits(src.int_bits) {}

   uint32_t getRawBits() const { return int_bits; }
   
   void operator=(const sparc_int_regset &src) { int_bits = src.int_bits; }
   bool operator==(const sparc_int_regset &other) const {
      return int_bits==other.int_bits;
   }
   bool operator==(const sparc_reg &r) const {
      return r.isIntReg() && (1U << r.getIntNum() == int_bits);
   }
   bool operator!=(const sparc_int_regset &other) const {
      return int_bits != other.int_bits;
   }
   bool equalIsAndLs(const sparc_int_regset &src) const {
      uint32_t temp = int_bits ^ src.int_bits; // XOR --> i's and l's now 0 iff equal
      temp &= 0xffff0000;
      return (temp == 0);
   }
   bool exists(const sparc_int_regset &subset) const {
      return 0U == (subset.int_bits & (~int_bits));
   }
   bool existsIntRegNum(unsigned num) const {
      return 0 != ((1U << num) & int_bits);
   }

   bool isempty() const { return int_bits == 0; }
   void clear() { int_bits = 0; }

   sparc_int_regset(XDR *);
   bool send(XDR *) const;

   static Raw raw;
   static Random random;
   
   sparc_int_regset(Raw, uint32_t iint_bits) : int_bits(iint_bits) {}
   sparc_int_regset(Random) : int_bits((uint32_t)::random()) {}
   
   static SingleIntReg singleIntReg;
   static Empty empty;
   static Full full;
   
   sparc_int_regset(SingleIntReg, unsigned num) : int_bits(1U << num) {
      assert(num < 32);
   }
   sparc_int_regset(SingleIntReg, const sparc_reg &r) :
      int_bits(1U << r.getIntNum())
   {
      assert(r.isIntReg());
   }
   
   sparc_int_regset(Empty) : int_bits(0) {}
   sparc_int_regset(Full) : int_bits(0xffffffff) {}

   static Save save;
   sparc_int_regset(Save, const sparc_int_regset &src, bool iValForLsAndOs) {
      int_bits = 0; // for now...

      // g's get a copy of src's g's:
      int_bits |= (src.int_bits & 0x000000ff);

      // i's get a copy of src's o's:
      int_bits |= (src.int_bits & 0x0000ff00) << 16;

      // at the moment, l's and o's should all be cleared:
      assert((int_bits & 0x00ffff00) == 0);
   
      // l's and o's get 'iValForLsAndOs'
      if (iValForLsAndOs)
         int_bits |= 0x00ffff00;
   }

   void operator|=(const sparc_int_regset &other) {
      int_bits |= other.int_bits;
   }
   void operator&=(const sparc_int_regset &other) {
      int_bits &= other.int_bits;
   }
   void operator-=(const sparc_int_regset &other) {
      int_bits &= ~other.int_bits;
   }

   sparc_reg expand1() const;

   unsigned count() const;

   void processRestore(const sparc_int_regset &prevWindow) {
      // %g registers are unchanged from prevWindow.
      // %l and %i registers unchanged from any window.
      // this's %o's get prevWindow's %i's

      int_bits &= 0xffff0000; // clear o's, g's
      int_bits |= (prevWindow.int_bits & 0x000000ff); // copy g's from prevWindow
      int_bits |= ((prevWindow.int_bits & 0xff000000) >> 16);
         // copy i's from prevWindow; assign to o's
   }
};

#endif
