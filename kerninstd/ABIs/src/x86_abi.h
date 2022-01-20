// x86_abi.h

#ifndef _X86_ABI_H_
#define _X86_ABI_H_

#include "abi.h"

class x86_abi : public abi {
 private:
   unsigned alignStackOffset(unsigned tentativeOffset) const {
       // on x86, stack is 4-byte aligned, assuming 32-bit mode
       return roundup(tentativeOffset, 4);
   }

 public:
   x86_abi() {}
   
   bool uint64FitsInSingleRegister() const {
      return false;
   }

   unsigned integerRegParamNumBytes() const {
      return 4;
   }

   // SPARCv9 ABI has a nasty 2047-byte gap between %fp and the actual
   // start of the stack frame in memory. x86 has no such problem
   unsigned getStackBias() const {
      return 0;
   }

   // How many bytes to allocate on the stack if we need to save
   // global registers across a function call. On x86, assuming we
   // only care about integer registers, we need room for the 8
   // GPRs, each one being 4 bytes, thus 32 bytes. We're making the
   // assumption that the caller is worrying about EFLAGS if it needs
   // to be saved across the call.
   unsigned getBigFrameAlignedNumBytes() const {
      unsigned extraBytes = 32;
      pair<unsigned, unsigned> frameInfo = calculateStackFrameSize(extraBytes);
      return frameInfo.first;
   }

   unsigned getMinFrameUnalignedNumBytes() const {
      // at minimum, need room for return instruction pointer 
      return 4;
   }

   regset_t* getCallerSavedRegs() const {
      // a callee can assume that these regs are free to mess with, since if 
      // the caller needs them preserved across the fn call, it would've 
      // saved them itself.
      return (regset_t*) new x86_reg_set(x86_reg_set::save);
   }

   regset_t* volatileints_of_regset(const regset_t *src) const {
      assert(!"x86_abi::volatileints_of_regset() nyi\n");
      return NULL;
   }
   regset_t* robustints_of_regset(const regset_t *src) const {
      assert(!"x86_abi::robustints_of_regset() nyi\n");
      return NULL;
   }

   pair<regset_t*, regset_t*>
   regset_classify_int32s_and_int64s(const regset_t *src) const {
      return pair<regset_t*,regset_t*>(regset_t::getRegSet(*src), NULL);
   }

   bool enough_int_regs(const regset_t *iSet,
                        unsigned numInt32RegsNeeded,
                        unsigned numInt64RegsNeeded) {
      if(iSet->countIntRegs() >= numInt32RegsNeeded)
         return true;
      else
         return false;
   }

   bool isReg64Safe(const reg_t &theReg) const {
      return false;
   }
};

#endif
