// power64_abi.h

#ifndef _POWER64_ABI_H_
#define _POWER64_ABI_H_

#include "abi.h"
#include "power_reg_set.h"

//Here is a paste from the ABI spec relevant to frame-related methods below
//Stack Frame Organization
// High Address
//
//           +-> Back chain
//           |   Floating point register save area
//           |   General register save area
//           |   Local variable space
//           |   Parameter save area    (SP + 48)
//           |   TOC save area          (SP + 40)
//           |   link editor doubleword (SP + 32)
//           |   compiler doubleword    (SP + 24)
//           |   LR save area           (SP + 16)
//           |   CR save area           (SP + 8)
// SP  --->  +-- Back chain             (SP + 0)

// Low Address
// Note that r3-r10 always must have preallocated space in in param save area
//register save area is preallocated only for those of  r14-r31 regs that are actually
//used in the function.  Note also the lack of %fp.  It is only necessary for dynamic 
//stack allocation, i.e. if we don't know in advance how much space to allocate for local
//vars or for general/float registers saving.  This is not the case for C compilers--hence
//no fp.

//Minimum stack frame. see above diagram (everything above parameter save 
// area is not mandatory). 112 happens to be quadword (16 bytes) aligned too.
#define MINFRAME 112

//register sizes 
#define GPRSIZE 8
#define FPRSIZE 8


#define FUNCARGS  64
#define LINKAREA  48
#define FUNC_CALL_SAVE (LINKAREA + FUNCARGS)

#define CTR_AST_DATAREG 32
#define LR_AST_DATAREG 33 

class power64_abi : public abi {
 private:
 public:
   power64_abi() {}
   
   bool uint64FitsInSingleRegister() const {
      return true;
   }

   unsigned integerRegParamNumBytes() const {
      return 8;
   }

   // SPARCv9 ABI has a nasty 2047-byte gap between %fp and the actual
   // start of the stack frame in memory. v8 has no such problem, and 
   //neither does Power, AFAIK.
   unsigned getStackBias() const {
       return 0;
   }

   // How many bytes to allocate on the stack if we need to save
   // global registers across a function call.   On Power, all
   //registers are global, really, so we need space for
   //(at most) 32 GPRs and 32 FPRs.  CR and LR have  reserved
   //spaces on the MINFRAME
   unsigned getBigFrameAlignedNumBytes() const {
      return MINFRAME + GPRSIZE * 32 + FPRSIZE * 32; 
   }

   unsigned getMinFrameUnalignedNumBytes() const {
     return MINFRAME;
   }

  unsigned alignStackOffset(unsigned tentativeOffset) const {
     return roundup(tentativeOffset, 16);
   }

   regset_t* getCallerSavedRegs() const {
     //r3-r10 are essentially caller-saved, i.e. they are volatile--available to use by the callee
         return ( (regset_t *) new power_reg_set(
	   (power_reg_set(power_reg::gpr3) + 
	   power_reg_set(power_reg::gpr4) + 
	   power_reg_set(power_reg::gpr5) + 
	   power_reg_set(power_reg::gpr6) +
	   power_reg_set(power_reg::gpr7) + 
	   power_reg_set(power_reg::gpr8) +
	   power_reg_set(power_reg::gpr9) + 
	   power_reg_set(power_reg::gpr10))));
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
      // on power64, all regs are 64-bit safe.
      return make_pair(regset_t::getRegSet(regset_t::empty), regset_t::getRegSet(*src));
   }

   bool enough_int_regs(const regset_t *iSet,
                        unsigned numInt32RegsNeeded,
                        unsigned numInt64RegsNeeded) {
      //on 64-bit Power, all regs are 64 bit safe
      return (iSet->countIntRegs() >= numInt32RegsNeeded + numInt64RegsNeeded);
   }

   bool isReg64Safe(const reg_t &theReg) const {
       // Although all registers are 64-bit safe in power we want
       // to make an assertion first.
       assert(theReg.isIntReg());

       return true;
   }
   unsigned getLocalVariableSpaceFrameOffset() const {
      return MINFRAME;
   }

   //We save the  volatile registers (gpr0-gpr13) in the 
   //"local variable space", hence we must add in 14 
   //"local variables" here
   unsigned getGeneralRegSaveFrameOffset() const {
       return MINFRAME + 14*GPRSIZE;
   }
   unsigned getLinkRegSaveFrameOffset() const {
       return 2*GPRSIZE;
   }
   unsigned getCondRegSaveFrameOffset() const {
       return GPRSIZE;
   }
   unsigned getMachineStateRegSaveFrameOffset() const {
      return 3*GPRSIZE; //use "compiler doubleword" area
   }


   unsigned getGPRSize() const { return GPRSIZE; }
   unsigned getFPRSize() const { return FPRSIZE; }
};

#endif
