// sparc_reg_set.h
// Ariel Tamches
// Provides for fast operations on a set of sparc registers.

#ifndef _SPARC_REG_SET_H_
#define _SPARC_REG_SET_H_

#include <inttypes.h>
#include <limits.h>
#include "reg_set.h"
#include "sparc_reg.h"
#include "sparc_fp_regset.h"
#include "sparc_int_regset.h"
#include "sparc_misc_regset.h"

class sparc_reg_set : public regset_t {
 private:
   class Save {};

   // I recently split a register set up into 3 separate variables, in 
   // anticipation of making good use of this fact in dataflow functions 
   // to save memory.
   sparc_fp_regset theFloatRegSet;
   sparc_int_regset theIntRegSet;
   sparc_misc_regset theMiscRegSet;
   
   sparc_reg expand1IntReg() const;
   sparc_reg expand1FloatReg() const;

 public:
   // I'm not really crazy about having a default ctor, but it's not too 
   // much of a hack, and igen requires it for now (unless we're willing 
   // to go with throwing exceptions in igen-generated code).
   sparc_reg_set() {
      // the empty set
   }

   sparc_reg_set(XDR *);
   bool send(XDR *) const;
   
   sparc_reg_set(Raw,
                 uint32_t iintbits,
                 uint64_t ifloatbits,
                 uint32_t imisc_stuff) :
      theFloatRegSet(sparc_fp_regset::raw, ifloatbits),
      theIntRegSet(sparc_int_regset::raw, iintbits),
      theMiscRegSet(sparc_misc_regset::raw, imisc_stuff) {}

   static const sparc_reg_set emptyset;
   static const sparc_reg_set fullset;
   static const sparc_reg_set always_live_set; //%g0, %sp, %fp
   static Save save;
   static const sparc_reg_set allGs;
   static const sparc_reg_set allOs;
   static const sparc_reg_set allLs;
   static const sparc_reg_set allIs;
   static const sparc_reg_set allIntRegs;
   static const sparc_reg_set allFPs;
   static const sparc_reg_set allMiscRegs;
   static const sparc_reg_set regsThatIgnoreWrites; // %g0

   sparc_reg_set(SingleIntReg, unsigned rnum);
   sparc_reg_set(SingleFloatReg, unsigned precision, sparc_reg r);
      // the "single" float reg will in practice turn into "precision" entries
      // in float_bits
   sparc_reg_set(Empty);
   sparc_reg_set(Full);
   sparc_reg_set(Save, const sparc_reg_set &src, bool iValForLsAndOs);
      // the opposite of processRestore()

   sparc_reg_set(const reg_t &); // intialize to a set of a single register
      // As usual, fp regs are assumed to be single-precision.
   sparc_reg_set(const regset_t &src);
  ~sparc_reg_set() {}

   void operator=(const regset_t &src);
   void operator=(const sparc_reg_set &src);

   bool isempty() const;
   void clear();

   uint32_t getRawIntBits() const {
      return theIntRegSet.getRawBits();
   }
   uint64_t getRawFloatBits() const {
      return theFloatRegSet.getRawBits();
   }
   uint32_t getRawMiscBits() const {
      return theMiscRegSet.getRawBits();
   }
   
   bool exists(const reg_t &) const; // membership test
   bool exists(const regset_t &) const; // membership test (subset)
   bool existsIntReg(unsigned) const;
   bool existsFpReg(unsigned num, unsigned precision) const;
   bool existsFpRegSinglePrecision(unsigned num) const;
   bool existsIcc() const { return theMiscRegSet.existsIcc(); }
   bool existsXcc() const { return theMiscRegSet.existsXcc(); }
   bool existsFcc0() const { return theMiscRegSet.existsFcc0(); }
   bool existsFcc1() const { return theMiscRegSet.existsFcc1(); }
   bool existsFcc2() const { return theMiscRegSet.existsFcc2(); }
   bool existsFcc3() const { return theMiscRegSet.existsFcc3(); }
   bool existsFcc(unsigned fccnum) const { return theMiscRegSet.existsFcc(fccnum); }
   bool existsPC() const { return theMiscRegSet.existsPC(); }
   bool existsGSR() const { return theMiscRegSet.existsGSR(); }
   bool existsASI() const { return theMiscRegSet.existsASI(); }
   bool existsPIL() const { return theMiscRegSet.existsPIL(); }
   
   // Set Union
   regset_t& operator|=(const regset_t &src);
   regset_t& operator|=(const reg_t &);
   sparc_reg_set operator|=(const sparc_reg_set &src); // set-union
   sparc_reg_set operator|=(const sparc_reg &); // set-union
   regset_t& operator+=(const regset_t &src);
   regset_t& operator+=(const reg_t &);
   sparc_reg_set operator+=(const sparc_reg_set &src); // set-union
   sparc_reg_set operator+=(const sparc_reg &); // set-union

   // Set Intersection
   regset_t* setIntersection(const regset_t &set) const;
   regset_t& operator&=(const regset_t &src);
   regset_t& operator&=(const reg_t &);
   sparc_reg_set operator&=(const sparc_reg_set &src); // set-intersection
   sparc_reg_set operator&=(const sparc_reg &); // set-intersection

   // Set Difference
   regset_t& operator-=(const regset_t &src);
   regset_t& operator-=(const reg_t &);
   sparc_reg_set operator-=(const sparc_reg_set &src); // set-difference
   sparc_reg_set operator-=(const sparc_reg &); // set-difference

   sparc_reg_set operator~() const; // set-negate
   
   sparc_reg_set operator|(const sparc_reg_set &src) const;
   sparc_reg_set operator|(const sparc_reg &) const;
   sparc_reg_set operator+(const sparc_reg_set &src) const;
   sparc_reg_set operator+(const sparc_reg &) const;
   sparc_reg_set operator&(const sparc_reg_set &src) const;
   sparc_reg_set operator&(const sparc_reg &) const;
   sparc_reg_set operator-(const sparc_reg_set &src) const;
   sparc_reg_set operator-(const sparc_reg &) const;
   
   bool operator==(const regset_t &src) const;
   bool operator==(const reg_t &) const;
   bool operator!=(const regset_t &src) const;
   bool equalIsAndLs(const sparc_reg_set &src) const;

   sparc_reg expand1() const;
      // assumes the sparc_reg_set is a single register

   unsigned count() const;
      // NOTE: fp regs are counted rather arbitrarily!
      // (assumes 64 single-precision regs, which is not really a feasible situation)
   unsigned countIntRegs() const;
   unsigned countFPRegsAsSinglePrecision() const;
   unsigned countMiscRegs() const;
   
   void processRestore(const sparc_reg_set &prevWindow);
      // this's %g's gets prevWindow's %g's; this's %f's get prevWindow's %f's
      // this's %o's gets prevWindow's %i's
      // There is no processSave(); instead, there's a save-ctor

   pdstring disassx() const;
      // does not assume the set is just a single register.  When it's desired
      // to collapse several registers into one (e.g. a "single" dbl or quad
      // precision fp reg will appear as 2 or 4 regs here), just call
      // expand1().disass() instead of disassx(); that'll get you what you want.

   static bool inSet(const regset_t &, unsigned);
   static reg_t& getRegByBit(unsigned);

   const_iterator begin_intregs() const {
      return const_iterator(*this, 0); // 0 --> start of integer regs
   }
   const_iterator begin_intregs_afterG0() const {
      return const_iterator(*this, 1); // 1 --> start of integer regs, skipping %g0
   }
   const_iterator end_intregs() const {
      return const_iterator(*this, 32); // 32 --> 1 beyond ndx of last integer reg
   }
};

#endif
