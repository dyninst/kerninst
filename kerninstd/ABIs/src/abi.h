// abi.h
// application binary interface.
// base class.
// provides some methods such as whether 64-bit values are contained in one or
// two registers, how to allocate a stack frame properly so that your generated
// code can interface with compiled code, etc.

#ifndef _ABI_H_
#define _ABI_H_

#include <utility> // pair
using std::pair;
using std::make_pair;
#include <sys/param.h> // roundup() on Linux
#include <sys/sysmacros.h> // roundup() on Solaris
#include "reg_set.h"

class abi {
 private:
   virtual unsigned alignStackOffset(unsigned tentativeOffset) const = 0;
      // each abi has different alignment requirements; hence this virtual method
   
 public:
   virtual bool uint64FitsInSingleRegister() const = 0;
   virtual unsigned integerRegParamNumBytes() const = 0;
      // e.g. sparcv8: 4 bytes, sparcv9: 8 bytes.  Useful when you
      // need to pass an 8-byte parameter: can it fit into a single register
      // (sparcv9) or do we need to split it into two registers?

   // SPARCv9 ABI has a nasty 2047-byte gap between %fp and the actual
   // start of the stack frame in memory
   virtual unsigned getStackBias() const = 0;

   // How many bytes to allocate on the stack if we need to save
   // global registers there. On SPARC we need to save G's, O's and
   // condition codes
   virtual unsigned getBigFrameAlignedNumBytes() const = 0;

   virtual unsigned getMinFrameUnalignedNumBytes() const = 0;
      // use this if you want minframe plus some more custom stuff of your own
      // on the stack.

   unsigned getMinFrameAlignedNumBytes() const {
      // If you have data of your own to put on the stack, then use
      // calculateStackFrameSize() instead.
      return alignStackOffset(getMinFrameUnalignedNumBytes());
   }
   pair<unsigned, unsigned>
       calculateStackFrameSize(unsigned data_nbytes) const {
      // determines the number of bytes that are needed for a save instruction:
      // minframe, plus padding to achieve data_alignment, plus data_nbytes,
      // plus padding to ensure the stack is aligned to ABI's dictates.
      // Result is a pair: (the stack frame nbytes, the offset of your data)
      // If you don't have any data of your own to put on the stack, then
      // use getMinFrameAlignedNumBytes() instead.
      
      unsigned nbytes = getMinFrameAlignedNumBytes();
      const unsigned my_data_offset = nbytes;

      nbytes += data_nbytes;
      const unsigned total_stack_frame_size = alignStackOffset(nbytes);

      return make_pair(total_stack_frame_size, my_data_offset);
   }
   
   virtual regset_t* getCallerSavedRegs() const = 0;
      // a callee can assume that these regs are free to mess with, since if the caller
      // needs them preserved across the fn call, it would've saved them itself.

   virtual regset_t* volatileints_of_regset(const regset_t *src) const = 0;
   virtual regset_t* robustints_of_regset(const regset_t *src) const = 0;
   
   virtual pair<regset_t*, regset_t*>
   regset_classify_int32s_and_int64s(const regset_t *src) const = 0;

   virtual bool enough_int_regs(const regset_t *theSet,
                                unsigned numInt32RegsNeeded,
                                unsigned numInt64RegsNeeded) = 0;
      // returns true iff "theSet" contains enough registers.  This fn makes sense
      // as a method of class abi because it knows how to distinguish 32 vs. 64
      // bit registers.
   virtual bool isReg64Safe(const reg_t &theReg) const = 0;
      //returns true if a register is int64Safe. Really needed for the 32bit
      //mode, since everything is 64 safe on the 64 bit
};

#endif
