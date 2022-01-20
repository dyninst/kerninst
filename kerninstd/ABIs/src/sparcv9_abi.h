// sparcv9_abi.h

#ifndef _SPARCV9_ABI_H_
#define _SPARCV9_ABI_H_

#include "abi.h"

class sparcv9_abi : public abi {
 private:
 public:
   sparcv9_abi() {}
   
   bool uint64FitsInSingleRegister() const {
      return true;
   }

   unsigned integerRegParamNumBytes() const {
      return 8;
   }

   // SPARCv9 ABI has a nasty 2047-byte gap between %fp and the actual
   // start of the stack frame in memory. v8 has no such problem
   unsigned getStackBias() const {
       return 2047;
   }

   // How many bytes to allocate on the stack if we need to save
   // global registers across a function call. On SPARC we need to
   // save G's, O's and condition codes.
   unsigned getBigFrameAlignedNumBytes() const {
       unsigned extraBytes = 8 * (7 + 8 + 1);
       pair<unsigned, unsigned> frameInfo = 
	   calculateStackFrameSize(extraBytes);
       return frameInfo.first;
   }

   unsigned getMinFrameUnalignedNumBytes() const {
      // Check out /usr/include/sys/stack.h on any solaris box for details:
      // [Differs a bit from v8plus abi]
      // 1) 16 xwords to save %i and %l regs on overflow
      // 2) first 6 outgoing params
      // And in the end, make sure to keep the stack 16-byte aligned [differs from v8]
      const unsigned num_xwords = 16 + 6;
      return 8*num_xwords;
   }

   unsigned alignStackOffset(unsigned tentativeOffset) const {
      // In sparcv9, the stack must be 16-byte aligned (see /usr/include/sys/stack.h)
      return roundup(tentativeOffset, 16);
   }

   regset_t* getCallerSavedRegs() const {
      // same as in sparcv8plus_abi.h
      regset_t *result = (regset_t *) new sparc_reg_set(sparc_reg_set::allOs - sparc_reg::sp - sparc_reg::o7 + sparc_reg::g1 + sparc_reg::reg_xcc + sparc_reg::reg_icc + sparc_reg_set::allFPs + sparc_reg::reg_fcc0 + sparc_reg::reg_fcc1 + sparc_reg::reg_fcc2 + sparc_reg::reg_fcc3);
      return result;
   }

   regset_t* volatileints_of_regset(const regset_t *src) const {
      regset_t *result = (regset_t *) new sparc_reg_set((sparc_reg_set &)*src & (sparc_reg_set::allOs + sparc_reg_set::allGs));
         // note that saying all %g's are volatile is being overly
         // conservative (and thus erring on the safe side)
      return result;
   }
   regset_t* robustints_of_regset(const regset_t *src) const {
      regset_t *result = (regset_t *) new sparc_reg_set((sparc_reg_set &)*src & (sparc_reg_set::allIs + sparc_reg_set::allLs));
      return result;
   }

   pair<regset_t*, regset_t*>
   regset_classify_int32s_and_int64s(const regset_t *src) const {
      // with sparcv9 ABI, all integer regs are 64-bit-safe
      return make_pair(regset_t::getRegSet(regset_t::empty), regset_t::getRegSet(*src));
   }

   bool enough_int_regs(const regset_t *iSet,
                        unsigned numInt32RegsNeeded,
                        unsigned numInt64RegsNeeded) {
      // v9 abi: all regs are 64 bit safe
      return (iSet->countIntRegs() >= numInt32RegsNeeded + numInt64RegsNeeded);
   }

   bool isReg64Safe(const reg_t &theReg) const {
       // Although all registers are 64-bit safe in sparcv9 we want
       // to check some assertions
       assert(theReg.isIntReg());

       if ((sparc_reg &)theReg == sparc_reg::g0) {
	   return false;
       }

       return true;
   }
};

#endif
