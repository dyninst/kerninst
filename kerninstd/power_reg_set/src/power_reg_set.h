// power_reg_set.h
// Igor Grobman (based on sparc_reg_set by Ariel Tamches)
// Provides for fast operations on a set of power registers.

#ifndef _POWER_REG_SET_H_
#define _POWER_REG_SET_H_

#include <inttypes.h>
#include <limits.h>
#include <rpc/xdr.h>
#include "power_reg.h"
#include "power_fp_regset.h"
#include "power_int_regset.h"
#include "power_misc_regset.h"
#include "reg_set.h"

//#include <utility> // pair
//using namespace std;
struct XDR;



class power_reg_set : public regset_t { // 16 bytes
 private:
   // I recently split a register set up into 3 separate variables, in 
   // anticipation of making good use of this fact in dataflow functions 
   // to save memory.
   //Ari's comment applies here too -Igor
   power_fp_regset theFloatRegSet;
   power_int_regset theIntRegSet;
   power_misc_regset theMiscRegSet;
   
/*    class Raw {}; */
/*    class Random {}; */
/*    class Empty {}; */
/*    class Full {}; */

/*    class SingleIntReg {}; */
/*    class SingleFloatReg {}; */

   class SPR {};

 public:
   // I'm not really crazy about having a default ctor, but it's not too much of
   // a hack, and igen requires it for now (unless we're willing to go with
   // throwing exceptions in igen-generated code).
   power_reg_set() : regset_t() {
      // the empty set
   }

   power_reg_set(XDR *);
   bool send(XDR *) const;
   
   static Raw raw;
   static Random random;
   
   power_reg_set(Raw,
                 uint32_t iintbits,
                 uint64_t ifloatbits,
                 uint32_t imisc_stuff) :
      regset_t(),
      theFloatRegSet(power_fp_regset::raw, ifloatbits),
      theIntRegSet(power_int_regset::raw, iintbits),
      theMiscRegSet(power_misc_regset::raw, imisc_stuff)
   {
   }

   power_reg_set(Random);
      // Obviously, a (seemingly crazy) routine like this is only used in
      // regression torture test programs.

   static SingleIntReg singleIntReg;
   static SingleFloatReg singleFloatReg;
   
   static Empty empty;
   static Full full;
   static SPR spr_type;

   static const power_reg_set emptyset;
   static const power_reg_set fullset;

   static const power_reg_set allIntRegs;
   static const power_reg_set allFPs;
   static const power_reg_set allMiscRegs;
   static const power_reg_set allCRFields;
   static const power_reg_set allXERArithFields; //SO,OV,CA
   static const power_reg_set allXERFields;  //all of XER
   static const power_reg_set regsThatIgnoreWrites; //none, really
   static const power_reg_set always_live_set; 
   //r1, r2 (stack and TOC pointers)

   power_reg_set(SingleIntReg, unsigned rnum);
   power_reg_set(SingleFloatReg,  unsigned rnum);
     
   power_reg_set(Empty);
   power_reg_set(Full);
 
   power_reg_set(const power_reg &); // intialize to a set of a single register
      
   power_reg_set(const power_reg_set &src);
   power_reg_set(const regset_t &src);
  ~power_reg_set() {}

   void operator=(const regset_t &src);
   void operator=(const power_reg_set &src);


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
   bool existsFpReg(unsigned num) const;
   bool existsFpReg(unsigned num, unsigned /* precision */) const {
     return existsFpReg(num); //precision ignored on power
   }
   bool existsFpRegSinglePrecision(unsigned /* num */) const {
      assert(false); //inapplicable to power
      return false;
   }
   bool existsCrField (unsigned num) const { 
      return theMiscRegSet.existsCrField(num); }

   bool existsLr() const { return theMiscRegSet.existsLr(); }
   bool existsCtr() const { return theMiscRegSet.existsCtr(); }
 

   //XER fields
   bool existsSo() const  { return theMiscRegSet.existsSo(); }
   bool existsOv() const  { return theMiscRegSet.existsOv(); }
   bool existsXerResv() const  { return theMiscRegSet.existsXerResv(); }
   bool existsXerStrBytes() const  { return theMiscRegSet.existsXerStrBytes();}
   bool existsCa() const  { return theMiscRegSet.existsCa(); }

   
   bool existsFpscr() const { return theMiscRegSet.existsFpscr(); }
 
 
   

   bool existsTbr() const { return theMiscRegSet.existsTb(); }
   bool existsTbu() const { return theMiscRegSet.existsTbu(); }
   
   bool existsSprg(unsigned sprgnum) const { return theMiscRegSet.existsSprg(sprgnum); }

   bool existsAsr() const { return theMiscRegSet.existsAsr(); }
   bool existsDar() const { return theMiscRegSet.existsDar(); }
   bool existsDsisr() const { return theMiscRegSet.existsDsisr(); }
   bool existsMsr() const { return theMiscRegSet.existsMsr(); }
   bool existsDec() const { return theMiscRegSet.existsDec(); }
   bool existsPvr() const { return theMiscRegSet.existsPvr(); }
   bool existsSdr1() const { return theMiscRegSet.existsSdr1(); }
   bool existsSrr(unsigned num) const { 
      return theMiscRegSet.existsSrr(num); 
   }  
   bool existsEar() const { return theMiscRegSet.existsEar(); }
   bool existsPmc(unsigned pmcnum) const { return theMiscRegSet.existsPmc(pmcnum); }
   bool existsMmcra() const { return theMiscRegSet.existsMmcra(); }
   bool existsMmcr0() const { return theMiscRegSet.existsMmcr0(); }
   bool existsMmcr1() const { return theMiscRegSet.existsMmcr1(); }
   bool existsSiar() const { return theMiscRegSet.existsSiar(); }
   bool existsSdar() const { return theMiscRegSet.existsSdar(); }
   bool existsDabr() const { return theMiscRegSet.existsDabr(); }
   bool existsPir() const { return theMiscRegSet.existsPir(); }
   bool existsHdec() const { return theMiscRegSet.existsHdec(); }
   bool existsAccr() const { return theMiscRegSet.existsAccr(); }
   bool existsCtrl() const { return theMiscRegSet.existsCtrl(); }
   bool existsNull() const { return theMiscRegSet.existsNull(); }


   regset_t& operator|=(const regset_t &src); // set-union
   regset_t& operator|=(const reg_t &); // set-union
   power_reg_set& operator|=(const power_reg_set &src);
   power_reg_set& operator|=(const power_reg &r);
   regset_t& operator+=(const regset_t &src); // set-union
   regset_t& operator+=(const reg_t &); // set-union
   power_reg_set& operator+=(const power_reg_set &src); // set-union
   power_reg_set& operator+=(const power_reg &); // set-union

   
   regset_t * setIntersection ( const regset_t &set) const;
   //returns a new set allocated off heap, which represents
   //set1 intersected with set2.  Watch for memory leaks!!
   
   regset_t& operator&=(const regset_t &src); // set-intersection
   regset_t& operator&=(const reg_t &); // set-intersection
   power_reg_set& operator&=(const power_reg_set &src);
   power_reg_set& operator&=(const power_reg &r);

   regset_t& operator-=(const regset_t &src); // set-difference
   regset_t& operator-=(const reg_t &); // set-difference
   power_reg_set& operator-=(const power_reg_set &src);
   power_reg_set& operator-=(const power_reg &r);

   friend power_reg_set operator~(const power_reg_set &src);

   power_reg_set operator|(const power_reg_set &src) const;
   power_reg_set operator|(const power_reg &) const;
   power_reg_set operator+(const power_reg_set &src) const;
   power_reg_set operator+(const power_reg &) const;
   power_reg_set operator&(const power_reg_set &src) const;
   power_reg_set operator&(const power_reg &) const;
   power_reg_set operator-(const power_reg_set &src) const;
   power_reg_set operator-(const power_reg &) const;
   
   bool operator==(const regset_t &src) const;
   bool operator==(const reg_t &) const;
   bool operator!=(const regset_t &src) const;
   bool equalIsAndLs(const regset_t &src) const;

   bool operator==(const power_reg_set &src) const;

   void removeRegsThatIgnoreWrites() { *this -= regsThatIgnoreWrites;}
   void removeAlwaysLiveRegs() { *this -= always_live_set; }
   void addRegsThatIgnoreWrites() {   *this += regsThatIgnoreWrites;}
   void addAlwaysLiveRegs() {  *this += always_live_set; }
   void assignEmptySet() { *this = emptyset; }
   void assignFullSet() { *this = fullset; }


   power_reg expand1() const;
   // assumes the power_reg_set is a single register
   
   power_reg expand1IntReg() const;
   power_reg expand1FloatReg() const;   

   unsigned count() const;

   unsigned countIntRegs() const;
   unsigned countFPRegs() const;
   unsigned countFPRegsAsSinglePrecision() const {
      assert(false); //meaningless on power
      return false;
   }
   unsigned countMiscRegs() const;
 
   pdstring disassx() const;
   // does not assume the set is just a single register.  When it's desired
   // to collapse several registers into one (e.g. a "single" dbl or quad
   // precision fp reg will appear as 2 or 4 regs here), just call
   // expand1().disass() instead of disassx(); that'll get you what you want.
      
   static bool inSet(const regset_t &, unsigned);
#if 0 //now part of arch_regset      
   // Iteration, STL style
   class const_iterator {
    private:
      const power_reg_set &theset;
      unsigned bit; // 0-31 for int regs; 32-63 for fp regs; 63-114 for misc regs

      bool inset() const;
      
      bool done() const { return (bit == power_reg::max_reg_num); }

      void movetonext() {
         // move "bit" to the next element of the set, or end (if appropriate)
         assert(bit <= power_reg::max_reg_num);
         do {
            ++bit;
         } while (bit <= power_reg::max_reg_num && !inset());
      }

      void makeInSetOrEnd() {
         if (bit == power_reg::max_reg_num+1)
            // it's already end
            return;

         if (!inset())
            movetonext();
      }

      const_iterator &operator=(const const_iterator &src);
         // this *can't* be defined, even if we wanted it, so long as
         // "theset" is a reference.

    public:
      const_iterator(const const_iterator &src) : theset(src.theset) {
         // used by begin() and end()
         bit = src.bit;
      }

      const_iterator(const power_reg_set &iset, unsigned start_bit_ndx) : theset(iset) {
         bit = start_bit_ndx;
         makeInSetOrEnd();
      }

      //      power_reg operator*() const {
   
      // sorry, no operator->(); would be a little tough, imho. (Would have to
      // return a pointer to a sparc_reg...unfortunately, there is no sparc_reg
      // lying around in the sparc_reg_set class that can be pointed to...so this
      // would take a little time to implement.)

      const_iterator operator++(int) {
         const_iterator result = *this;
         movetonext();
         return result;
      }
      void operator++() { movetonext(); }

      bool operator==(const const_iterator &other) const {
         return (&theset == &other.theset &&
                 bit == other.bit);
      }
      bool operator!=(const const_iterator &other) const {
         return (&theset != &other.theset ||
                 bit != other.bit);
      }
   };

   const_iterator begin() const {
      return const_iterator(*this, 0); // 0 --> start of integer regs
   }
   const_iterator end() const {
      return const_iterator(*this, power_reg::total_num_regs); // ndx of last reg + 1
   }

   const_iterator begin_intregs() const {
      return const_iterator(*this, 0); // 0 --> start of integer regs
   }
   const_iterator begin_intregs_afterG0() const {
      return const_iterator(*this, 1); // 1 --> start of integer regs, skipping %g0
   }
   const_iterator end_intregs() const {
      return const_iterator(*this, 32); // 32 --> 1 beyond ndx of last integer reg
   }
#endif
   const_iterator begin_intregs() const {
      return const_iterator(*this, 0); // 0 --> start of integer regs
   }
   const_iterator end_intregs() const {
      return const_iterator(*this, 32); // 32 --> 1 beyond ndx of last integer reg
   }
};


#endif
