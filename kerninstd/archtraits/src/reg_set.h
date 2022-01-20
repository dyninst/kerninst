// reg_set.h

#ifndef _REG_SET_H_
#define _REG_SET_H_

#include "inttypes.h" //uint32_t
#include "common/h/String.h"

#include "reg.h" //reg_t
struct XDR;

class regset_t {
 protected:
   class Raw {};
   class Random {};
   class Empty {};
   class Full {};

   class SingleIntReg {};
   class SingleFloatReg {};

 public:
   regset_t() {}
   virtual ~regset_t() {}

   virtual bool send(XDR *) const = 0;

   static Raw raw;
   static Random random;
   static Empty empty;
   static Full full;
   static SingleIntReg singleIntReg;
   static SingleFloatReg singleFloatReg;
   
   static regset_t* getRegSet(Empty);
   static regset_t* getRegSet(Full);
   static regset_t* getRegSet(XDR*);
   static regset_t* getRegSet(const regset_t &);

   static const regset_t& getEmptySet();
   static const regset_t& getFullSet();
   static const regset_t& getAlwaysLiveSet();
   static const regset_t& getRegsThatIgnoreWrites();
   static const regset_t& getAllIntRegs();

   void operator=(const regset_t &src);

   virtual bool isempty() const = 0;
   virtual void clear() = 0;

   virtual uint32_t getRawIntBits() const = 0;
   virtual uint64_t getRawFloatBits() const = 0;
   virtual uint32_t getRawMiscBits() const = 0;

   virtual bool exists(const reg_t &r) const = 0; // membership test
   virtual bool exists(const regset_t &src) const = 0; // membership (subset)
   virtual bool existsIntReg(unsigned) const = 0;
   virtual bool existsFpReg(unsigned num, unsigned precision) const = 0;
   virtual bool existsFpRegSinglePrecision(unsigned num) const = 0;

   // set-union 
   virtual regset_t& operator|=(const regset_t &src) = 0; 
   virtual regset_t& operator|=(const reg_t &r) = 0;
   virtual regset_t& operator+=(const regset_t &src) = 0;
   virtual regset_t& operator+=(const reg_t &r) = 0;

   // set-intersection
   virtual regset_t& operator&=(const regset_t &src) = 0;
   virtual regset_t& operator&=(const reg_t &r) = 0;
   virtual regset_t* setIntersection (const regset_t &set) const = 0;
      //returns a new set allocated off heap, which represents
      //set1 intersected with set2.  Watch for memory leaks!!

   // set-difference
   virtual regset_t& operator-=(const regset_t &src) = 0;
   virtual regset_t& operator-=(const reg_t &r) = 0;

   virtual bool operator==(const regset_t &src) const = 0;
   virtual bool operator==(const reg_t &) const = 0;
   virtual bool operator!=(const regset_t &src) const = 0;

   virtual unsigned count() const = 0;
   virtual unsigned countIntRegs() const = 0;
   virtual unsigned countFPRegsAsSinglePrecision() const = 0;
   virtual unsigned countMiscRegs() const = 0;
   virtual pdstring disassx() const = 0;

   static bool inSet(const regset_t &, unsigned);

   // Iteration, STL style
   class const_iterator {
    private:
      const regset_t &theset;
      unsigned bit; // indicates which reg, as defined by arch reg set

      bool inset() const { return inSet(theset, bit); }
      
      bool done() const { return (bit == reg_t::getMaxRegNum()); }

      void movetonext() {
         // move "bit" to the next element of the set, or end (if appropriate)
         assert(bit <= reg_t::getMaxRegNum());
         do {
            ++bit;
         } while ((bit <= reg_t::getMaxRegNum()) && !inset());
      }

      void makeInSetOrEnd() {
         if (bit == reg_t::getMaxRegNum()+1)
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

      const_iterator(const regset_t &iset, unsigned start_bit_ndx) : theset(iset) {
         bit = start_bit_ndx;
         makeInSetOrEnd();
      }

      reg_t& operator*() const {
	 return reg_t::getRegByBit(bit);
      }
      // sorry, no operator->(); would be a little tough, imho. (Would have to
      // return a pointer to a reg...unfortunately, there is no reg
      // lying around in the reg set class that can be pointed to...so this
      // would take a little time to implement.)

      const_iterator operator++(int) {
         const_iterator result = *this;
         movetonext();
         return result;
      }
      void operator++() { movetonext(); }

      bool operator==(const const_iterator &other) const {
         return ((theset == other.theset) && (bit == other.bit));
      }
      bool operator!=(const const_iterator &other) const {
	 return ((theset != other.theset) || (bit != other.bit));
      }
   };

   const_iterator begin() const {
      return const_iterator(*this, 0); // 0 --> start of regs
   }
   const_iterator end() const {
      return const_iterator(*this, reg_t::getMaxRegNum()+1); // ndx of last reg + 1
   }
};

#endif /*_ARCH_REG_SET_H_*/
