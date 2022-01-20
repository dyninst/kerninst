// power32_abi.h

#ifndef _POWER32_ABI_H_
#define _POWER32_ABI_H_

#include "abi.h"
#include "power_reg_set.h"

class power32_abi : public abi {
 private:
 public:
   power32_abi() {}
   
   bool uint64FitsInSingleRegister() const {
      return false;
   }

   unsigned integerRegParamNumBytes() const {
      return 4;
   }

   // SPARCv9 ABI has a nasty 2047-byte gap between %fp and the actual
   // start of the stack frame in memory. v8 has no such problem, and 
   //neither does Power, AFAIK.
   unsigned getStackBias() const {
       return 0;
   }

   // How many bytes to allocate on the stack if we need to save
   // global registers across a function call.   On Power, this implies
   //saving GPRs 13-31, plus (at least) CR fields 2-4, but might as well
   //save the whole thing.  What about FPRs??
   unsigned getBigFrameAlignedNumBytes() const {
      //disabled until I find out the details of linux/ppc stack convention
      assert(false && "nyi");
   }

   unsigned getMinFrameUnalignedNumBytes() const {
      //disabled until I find out the details of linux/ppc stack convention
      assert(false && "nyi");
   }

   unsigned alignStackOffset(unsigned tentativeOffset) const {
      //disabled until I find out the details of linux/ppc stack convention
      assert(false && "nyi");
   }

   regset_t* getCallerSavedRegs() const {
      //disabled until I find out the details of linux/ppc stack convention
      assert(false && "nyi");
   }

   regset_t* volatileints_of_regset(const regset_t *src) const {
      regset_t *result = (regset_t *) new power_reg_set((power_reg_set &)*src  & 
                                                        ( power_reg_set(power_reg::gpr0) +
                                                          power_reg_set(power_reg::gpr3) + 
                                                          power_reg_set(power_reg::gpr4) + 
                                                          power_reg_set(power_reg::gpr5) + 
                                                          power_reg_set(power_reg::gpr6) +
                                                          power_reg_set(power_reg::gpr7) + 
                                                          power_reg_set(power_reg::gpr8) +
                                                          power_reg_set(power_reg::gpr9) + 
                                                          power_reg_set(power_reg::gpr10) +
                                                          power_reg_set(power_reg::gpr11) + 
                                                          power_reg_set(power_reg::gpr12))); 
     
      return result;
   }
   regset_t* robustints_of_regset(const regset_t *src) const {
        regset_t *result = (regset_t *) new power_reg_set((power_reg_set &)*src  & 
                                                          (power_reg_set(power_reg::gpr13) + 
                                                           power_reg_set(power_reg::gpr14) + 
                                                           power_reg_set(power_reg::gpr15) + 
                                                           power_reg_set(power_reg::gpr16) +
                                                           power_reg_set(power_reg::gpr17) + 
                                                           power_reg_set(power_reg::gpr18) +
                                                           power_reg_set(power_reg::gpr19) + 
                                                           power_reg_set(power_reg::gpr20) +
                                                           power_reg_set(power_reg::gpr21) + 
                                                           power_reg_set(power_reg::gpr23) + 
                                                           power_reg_set(power_reg::gpr24) + 
                                                           power_reg_set(power_reg::gpr25) + 
                                                           power_reg_set(power_reg::gpr26) +
                                                           power_reg_set(power_reg::gpr27) + 
                                                           power_reg_set(power_reg::gpr28) +
                                                           power_reg_set(power_reg::gpr29) + 
                                                           power_reg_set(power_reg::gpr30) +
                                                           power_reg_set(power_reg::gpr31))); 
                                                         
      return result;
   }

   pair<regset_t*, regset_t*>
   regset_classify_int32s_and_int64s(const regset_t *src) const {
      // on power32, all registers are only 32 bits
      return make_pair(const_cast<regset_t*>(src), regset_t::getRegSet(regset_t::empty));
   }

   bool enough_int_regs(const regset_t *iSet,
                        unsigned numInt32RegsNeeded,
                        unsigned numInt64RegsNeeded) {
      //on 64-bit Power, all registers are only 32 bits
      return ( (iSet->countIntRegs() >= numInt32RegsNeeded) && (!numInt64RegsNeeded) );
   }

   bool isReg64Safe(const reg_t &theReg) const {
       assert(theReg.isIntReg());

       return false;
   }
};

#endif
