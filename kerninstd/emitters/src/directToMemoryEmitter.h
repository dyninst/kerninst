// directToKernelEmitter.h

#ifndef _DIRECT_TO_KERNEL_EMITTER_H_
#define _DIRECT_TO_KERNEL_EMITTER_H_

#include "abi.h"
#include "instr.h"
#include "common/h/Vector.h"
#include "insnVec.h"

#include <inttypes.h> // uint32_t

class directToMemoryEmitter_t {
 protected:
   const abi &theABI;
   
   dptr_t whereEmittingStartsInKerninstd;
   kptr_t whereEmittingStartsInKernel;

   char *currEmittingPtr; // uninitialized raw memory...in kerninstd space
   unsigned insnBytesEmitted;

   unsigned totalNumInsnBytesOKToWrite;
      // for detecting overflow...
   
 public:
   directToMemoryEmitter_t(const abi &iABI,
                         dptr_t whereToBeginEmittingToInKerninstd,
                         kptr_t whereToBeginEmittingToInKernel,
                            // if NULL then getStartEmitAddrInKernel()
                            // and getCurrentEmitAddrInKernel() will assert fail
                         unsigned itotalNumInsnBytesOKToWrite) :
         theABI(iABI) {
      whereEmittingStartsInKerninstd = whereToBeginEmittingToInKerninstd;
      whereEmittingStartsInKernel = whereToBeginEmittingToInKernel;
         // may be NULL
      currEmittingPtr = (char*)whereEmittingStartsInKerninstd;
         // this is always a kerninstd pointer
      insnBytesEmitted = 0;
      totalNumInsnBytesOKToWrite = itotalNumInsnBytesOKToWrite;
   }

   //WARNING: the getDirectToMemoryEmitter method returns a pointer to a new
   //         directToMemoryEmitter_t, so callers are responsible for destruction
   static directToMemoryEmitter_t* 
   getDirectToMemoryEmitter(const abi &iABI,
			    dptr_t whereToBeginEmittingToInKerninstd,
			    kptr_t whereToBeginEmittingToInKernel,
			       // if NULL then getStartEmitAddrInKernel()
			       // and getCurrentEmitAddrInKernel() will fail
			    unsigned itotalNumInsnBytesOKToWrite);
   
   const abi &getABI() const {
      return theABI;
   }

   unsigned numInsnBytesEmitted() const {
      return insnBytesEmitted;
   }
   dptr_t getStartEmitAddrInKerninstd() const {
      return (dptr_t)whereEmittingStartsInKerninstd;
   }
   kptr_t getStartEmitAddrInKernel() const {
      assert(whereEmittingStartsInKernel);
      return whereEmittingStartsInKernel;
   }
   dptr_t getCurrentEmitAddrInKerninstd() const {
      return (dptr_t)currEmittingPtr;
   }
   kptr_t getCurrentEmitAddrInKernel() const {
      assert(whereEmittingStartsInKernel);
      const unsigned offset = (dptr_t)currEmittingPtr -
                              (dptr_t)whereEmittingStartsInKerninstd;
      assert(offset <= totalNumInsnBytesOKToWrite);
      return whereEmittingStartsInKernel + offset;
   }
   void emit(instr_t *i) {
      assert(insnBytesEmitted <= totalNumInsnBytesOKToWrite);
      unsigned num_bytes = i->getNumBytes();
      i->copyRawBytes(currEmittingPtr);
      currEmittingPtr += num_bytes;
      insnBytesEmitted += num_bytes;
      if(insnBytesEmitted > totalNumInsnBytesOKToWrite) {
         cerr << "insnBytesEmitted=" << insnBytesEmitted 
              << ", totalNumInsnBytesOKToWrite=" << totalNumInsnBytesOKToWrite
              << endl;
         assert(!"directToMemoryEmitter::emit() - emitted more than ok\n");
      }
      //we are responsible for destroying pointed-to objects passed to us
      delete i;
   }

   // Several functions in the instr_t class generate sequences of 
   // instructions. Lets emit them as they are.
   void emitSequence(const insnVec_t *iv) {
      insnVec_t::const_iterator iter;

      for (iter = iv->begin(); iter != iv->end(); iter++) {
	 instr_t *instr = instr_t::getInstr(**iter);
	 emit(instr);
      }
      delete iv;
   }

   void emitImm32(uint32_t num, reg_t &rd) {
      insnVec_t *iv = insnVec_t::getInsnVec();
      
      instr_t::gen32imm(iv, num, rd);
      emitSequence(iv);
   }

   void emitImmAddr(uint32_t addr, reg_t &rd) {
      emitImm32(addr, rd);
   }

   void emitImmAddr(uint64_t addr, reg_t &rd) {
       insnVec_t *iv = insnVec_t::getInsnVec();
       instr_t::genImmAddr(iv, addr, rd);
       emitSequence(iv);
   }

   void emitCall(kptr_t from, kptr_t to) {
      insnVec_t *iv = insnVec_t::getInsnVec();
      insnVec_t::const_iterator iter;

      instr_t::genCallAndLink(iv, from, to);
      for (iter = iv->begin(); iter != iv->end(); iter++) {
	 instr_t *instr = instr_t::getInstr(**iter);
	 emit(instr);
      }
      delete iv;
   }

   virtual void emitSave(bool, const regset_t *regsToSave) =0;
   virtual void emitRestore(bool, const regset_t *regsToRestore  )=0;
   static unsigned getNumBytesForEmitSaveRestore (const  unsigned  numInt32Regs, 
						  const unsigned numInt64Regs);
   static regset_t *getSaveRegSet(const regset_t *preSaveRegSet,
                                  regset_t **regsToSave, 
                                  unsigned numRegsToSave);
   instr_t &getByKerninstdInsnAddr(dptr_t insnAddrInKerninstd) {
      assert(insnAddrInKerninstd >= (dptr_t)whereEmittingStartsInKerninstd);
      assert(insnAddrInKerninstd <
             (dptr_t)whereEmittingStartsInKerninstd + insnBytesEmitted);
      
      instr_t *ptr = (instr_t *)insnAddrInKerninstd;
      return *ptr;
   }
   instr_t &getByInsnByteOffset(unsigned offset) {
      assert(offset % 4 == 0);
      instr_t *ptr = (instr_t*)whereEmittingStartsInKerninstd + (offset / 4);
      return *ptr;
   }
   
   virtual  void complete() =0;
};

#endif
