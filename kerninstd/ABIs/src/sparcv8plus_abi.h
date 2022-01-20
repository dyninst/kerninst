// sparcv8plus_abi.h

#ifndef _SPARCV8PLUS_ABI_H_
#define _SPARCV8PLUS_ABI_H_

#include "abi.h"
#include "sparc_reg_set.h"

class sparcv8plus_abi : public abi {
 private:
 public:
   sparcv8plus_abi() {}
   
   bool uint64FitsInSingleRegister() const {
      return false;
   }

   unsigned integerRegParamNumBytes() const {
      return 4;
   }

   // SPARCv9 ABI has a nasty 2047-byte gap between %fp and the actual
   // start of the stack frame in memory. v8 has no such problem
   unsigned getStackBias() const {
       return 0;
   }

   // How many bytes to allocate on the stack if we need to save
   // global registers across a function call. On SPARC we need to
   // save G's, O's and condition codes. They are 64-bit safe, so we
   // allocate full 8 bytes for each of them, even on the 32-bit platform.
   unsigned getBigFrameAlignedNumBytes() const {
       unsigned extraBytes = 8 * (7 + 8 + 1);
       pair<unsigned, unsigned> frameInfo = 
	   calculateStackFrameSize(extraBytes);
       return frameInfo.first;
   }

   unsigned getMinFrameUnalignedNumBytes() const {
      // save %sp, -getMinFrameNumBytes(), %sp so that
      // your generated code can interface with the outside world.

      // See /usr/include/sys/stack.h on any solaris box for info.  Here's the details:
      // 1) 16 words to save %l and %i's in case of window overflow trap...
      // 2) 1-word struct return address
      // 3) 6 words for callee to dump reg arguments
      // Note that the stack must always be 8-byte aligned.

      // 16 words to save %l's and %i's in case of window overflow trap...
      unsigned numwords = 16;
      // ...plus 1 word for hidden parameter...
      numwords++;
      // ...plus 6 words for callee to save its register arguments
      numwords += 6;

      const unsigned result = 4*numwords;

      // NOTE: result is unaligned!  User must remember to call alignStackOffset() 
      // before emitting the actual save instruction!

      return result;
   }

   unsigned alignStackOffset(unsigned tentativeOffset) const {
      // in sparcv8+, the stack must be 8-byte aligned:
      return roundup(tentativeOffset, 8);
   }

   virtual regset_t* getCallerSavedRegs() const {
      // a callee can assume that these regs are free to mess with, since if the caller
      // needs them preserved across the fn call, it would've saved them itself.
      regset_t *result = (regset_t *) new sparc_reg_set(sparc_reg_set::allOs - sparc_reg::sp - sparc_reg::o7 + sparc_reg::g1 + sparc_reg::reg_xcc + sparc_reg::reg_icc + sparc_reg_set::allFPs + sparc_reg::reg_fcc0 + sparc_reg::reg_fcc1 + sparc_reg::reg_fcc2 + sparc_reg::reg_fcc3);
      return result;

   }

   regset_t* volatileints_of_regset(const regset_t *src) const {
      regset_t *result = (regset_t *) new sparc_reg_set((sparc_reg_set &)*src & (sparc_reg_set::allOs + sparc_reg_set::allGs));
      return result;
         // note that saying all %g's are volatile is being overly
         // conservative (and thus erring on the safe side)
   }
   regset_t* robustints_of_regset(const regset_t *src) const {
      regset_t *result = (regset_t *) new sparc_reg_set((sparc_reg_set &)*src & (sparc_reg_set::allIs + sparc_reg_set::allLs));
      return result;
   }
   
   pair<regset_t*, regset_t*>
   regset_classify_int32s_and_int64s(const regset_t *src) const {
      // Due to v8plus ABI restrictions, some registers can use all 64 bits safely
      // while for others, we must use only the low 32 bits.  This routine divvies
      // them up.  result.first are the 32-bit-only int regs; result.second are the
      // 64-bit-safe int regs.
      const sparc_reg_set sixty_four_bit_unsafe_regs = sparc_reg_set::allLs +
         sparc_reg_set::allIs;
      
      const sparc_reg_set sixty_four_bit_safe_regs = sparc_reg_set::allGs +
         sparc_reg_set::allOs;
      
      return make_pair((regset_t*)
		       (new sparc_reg_set((sparc_reg_set &)*src & 
					  sixty_four_bit_unsafe_regs)),
                       (regset_t*)
		       (new sparc_reg_set((sparc_reg_set &)*src & 
					  sixty_four_bit_safe_regs)));
   }

   bool enough_int_regs(const regset_t *iSet,
                        unsigned numInt32RegsNeeded,
                        unsigned numInt64RegsNeeded) {
      const pair<regset_t*, regset_t*> theSet =
         regset_classify_int32s_and_int64s(iSet);
         // .first: 32-bit-safe regs   .second: 64-bit-safe regs

      // Are there enough 64-bit registers?
      const unsigned num64sAvailable = theSet.second->countIntRegs();
      if (numInt64RegsNeeded > num64sAvailable)
         return false;

      const unsigned num32sAvailable =
         theSet.first->countIntRegs() + (num64sAvailable - numInt64RegsNeeded);
      if (numInt32RegsNeeded > num32sAvailable)
         return false;
      
      return true;
   }

   bool isReg64Safe(const reg_t &theReg) const {
       // sparcv8plus abi classifies some integer registers as 32 bits, 
       // and others as 64 bits. true iff a %g or %o reg
       assert(theReg.isIntReg());

       if ((sparc_reg &)theReg == sparc_reg::g0) {
	   return false;
       }

       unsigned ignore;
       // %g0, the exception, is handled above
       return ((sparc_reg &)theReg).is_gReg(ignore) || ((sparc_reg &)theReg).is_oReg(ignore); 
   }
};


#endif
