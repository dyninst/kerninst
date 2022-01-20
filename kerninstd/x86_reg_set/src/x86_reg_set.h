// x86_reg_set.h

#ifndef _X86_REG_SET_H_
#define _X86_REG_SET_H_

#include <limits.h>

#include "x86_reg.h"
#include "reg_set.h"

struct XDR;

class x86_reg_set : public regset_t {
 private:
   class Save {};

   unsigned int_bits;
   unsigned long long float_bits;
   union {
      unsigned misc_stuff;
      struct {
	 unsigned padding : 29; // padding
	 unsigned pc      : 1;
	 unsigned eflags  : 1;
         unsigned fp      : 1;
      } ms;
   };

   x86_reg expand1IntReg() const;
   x86_reg expand1FloatReg() const;

 public:

   static Save save;

   x86_reg_set(Raw, uint32_t iintbits, uint64_t ifloatbits,
	       uint32_t imisc_stuff) : regset_t() {
	int_bits = iintbits;
	float_bits = ifloatbits;
	misc_stuff = imisc_stuff;
	ms.padding = 0;
   }

   x86_reg_set(XDR *);
   bool send(XDR *) const;

   x86_reg_set(Random); // For testing
   void printSet(); // For debugging

   static x86_reg_set emptyset;
   static x86_reg_set fullset;
   static x86_reg_set always_live_set;
   static x86_reg_set allIntRegs;
   static x86_reg_set allFPs;
   static x86_reg_set allMiscRegs;
   static x86_reg_set regsThatIgnoreWrites;

   x86_reg_set(SingleIntReg, unsigned rnum);
   x86_reg_set(SingleFloatReg, unsigned precision, x86_reg r);
   x86_reg_set(Empty);
   x86_reg_set(Full);

   x86_reg_set() : regset_t() { int_bits = float_bits = misc_stuff = 0; }

   x86_reg_set(const x86_reg &); // initialize to a set of a single register
   x86_reg_set(const x86_reg_set &src);

   x86_reg_set(Save);

  ~x86_reg_set() {}

   void operator=(const regset_t &src);
   void operator=(const x86_reg_set &src);

   bool isempty() const;
   void clear();

   uint32_t getRawIntBits() const { return int_bits; }
   uint64_t getRawFloatBits() const { return float_bits; }
   uint32_t getRawMiscBits() const { return misc_stuff; }

   bool exists(const reg_t &) const; // membership test
   bool exists(const regset_t &) const; // membership test (subset)
   bool existsIntReg(unsigned) const;
   bool existsFpReg(unsigned, unsigned) const;
   bool existsFpRegSinglePrecision(unsigned) const;
   bool existsEFLAGS() const { return ms.eflags; }
   bool existsPC() const     { return ms.pc; }

   // Set Union
   regset_t& operator|=(const regset_t &src);
   regset_t& operator|=(const reg_t &);
   x86_reg_set& operator|=(const x86_reg_set &src);
   x86_reg_set& operator|=(const x86_reg &);
   regset_t& operator+=(const regset_t &src);
   regset_t& operator+=(const reg_t &);
   x86_reg_set& operator+=(const x86_reg_set &src);
   x86_reg_set& operator+=(const x86_reg &);

   // Set Intersection
   regset_t& operator&=(const regset_t &src);
   regset_t& operator&=(const reg_t &);
   x86_reg_set& operator&=(const x86_reg_set &src);
   x86_reg_set& operator&=(const x86_reg &);

   regset_t* setIntersection (const regset_t &set) const;
     // returns heap-allocated set, watch for memory leaks!!

   // Set Difference
   regset_t& operator-=(const regset_t &src);
   regset_t& operator-=(const reg_t &);
   x86_reg_set& operator-=(const x86_reg_set &src);
   x86_reg_set& operator-=(const x86_reg &);
   
   // Binary Operators
   x86_reg_set operator|(const x86_reg_set &src) const;
   x86_reg_set operator|(const x86_reg &) const;
   x86_reg_set operator+(const x86_reg_set &src) const;
   x86_reg_set operator+(const x86_reg &) const;
   x86_reg_set operator&(const x86_reg_set &src) const;
   x86_reg_set operator&(const x86_reg &) const;
   x86_reg_set operator-(const x86_reg_set &src) const;
   x86_reg_set operator-(const x86_reg &) const;

   bool operator==(const regset_t &src) const;
   bool operator==(const x86_reg_set &src) const;
   bool operator==(const reg_t &) const;
   bool operator!=(const regset_t &src) const;

   // Unary Operators
   friend x86_reg_set operator~(const x86_reg_set &src);

   x86_reg expand1() const;
      // assumes the x86_reg_set is a single register

   unsigned count() const;
   unsigned countIntRegs() const;
   unsigned countFPRegsAsSinglePrecision() const;
   unsigned countMiscRegs() const;

   pdstring disassx() const; // assumes the x86_reg_set holds just 1 register

   static bool inSet(const regset_t &, unsigned);
   static reg_t& getRegByBit(unsigned);

   const_iterator begin_intregs() const {
      // start of integer regs
      return const_iterator(*this, x86_reg::min_int_reg);
   }
   const_iterator end_intregs() const {
      // 1 beyond ndx of last integer reg
      return const_iterator(*this, x86_reg::max_int_reg+1); 
   }
};

#endif
