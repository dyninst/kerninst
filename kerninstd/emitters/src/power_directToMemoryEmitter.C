// power_directToMemoryEmitter.C

#include "power_directToMemoryEmitter.h"
#include "power_instr.h"
#include "power64_abi.h"
#include "kernelDriver.h"

extern void flush_a_little(kptr_t); // .s file

// The following isn't static because various launchers use it.  In fact it's
// so general that it should probably be moved.
void flush_a_bunch(kptr_t iaddr, unsigned numbytes) {
    //We can't flush kernel addresses from userspace, so must ioctl.
   extern kernelDriver *global_kernelDriver; // sorry about this ugliness
   global_kernelDriver->flushIcacheRange(iaddr, numbytes);

   //OLD CODE
    // flush requires 4-byte alignment.  We don't enforce this (by nand'ing with 0x3)
   // because we know that we're already 4-byte aligned.
   //It is unclear what the instruction cache block size is on Power processors, and 
   //whether it varies.  Let's be safe for now, and leave it as 4
  
  
   
// register kptr_t addr = iaddr;
//   while (numbytes) {
//      flush_a_little(addr); // asm
//      asm("icbi 0, %0" : /* no outputs */ : "r" (addr));
//      addr += 4;
//      numbytes -= 4;
//   } 
   //should there be an isync here to make sure the pipeline is flushed??
}

void power_directToMemoryEmitter::complete() {
   // We just wrote a bunch of instructions.  They just made it to the data cache.
   // We need to issue cache block invalidate instructions to make sure they reach
   // the icache
   
   assert(insnBytesEmitted <= totalNumInsnBytesOKToWrite);
   //flush_a_bunch(whereEmittingStartsInKerninstd, insnBytesEmitted);
   //We flush the icache for the memory we write in kernel space, not
   //kerninstd space.
}

void power_directToMemoryEmitter::emitSave(bool saveNeeded, 
                                           const regset_t *regsToSave) {
  int regOffset =  ((power64_abi &)theABI).getGeneralRegSaveFrameOffset();
  const unsigned frameSize = theABI.getBigFrameAlignedNumBytes();
  //First, we need to generate a stack frame.
  instr_t *instr = new power_instr(power_instr::store, 
				   power_instr::stDoubleUpdateDS, 
				   power_reg::gpr1, 
				   power_reg::gpr1, //stack pointer
				   -frameSize); 
  
  emit(instr);

  unsigned saveVolatileOffset = 
         ((power64_abi &)theABI).getLocalVariableSpaceFrameOffset();
  unsigned saveCondRegOffset = 
         ((power64_abi &)theABI).getCondRegSaveFrameOffset();

 
  //Save r0, which we will use as a temp register
  instr = new power_instr(power_instr::store, power_instr::stDoubleDS, 
                               power_reg::gpr0, 
                               power_reg::gpr1, //stack pointer
                               saveVolatileOffset); //save at r0 position
  emit(instr);

  instr = new power_instr(power_instr::movefromcondreg, power_reg::gpr0);
  // Register 0 is actually the cr register now. 
   
  emit(instr);
  instr = new power_instr(power_instr::store, power_instr::stDoubleDS, 
                      power_reg::gpr0, 
                      power_reg::gpr1, //stack pointer
                      saveCondRegOffset); 
  emit(instr);

  if (saveNeeded) { 
     assert(regsToSave->count() < 19);
     regset_t::const_iterator rs_iter = regsToSave->begin();
     for (unsigned i = 0; rs_iter != regsToSave->end() ; rs_iter++, i++) {
        const power_reg &regToSave =   (const power_reg &)*rs_iter; 
        instr = new power_instr(power_instr::store, power_instr::stDoubleDS, 
                                regToSave,  
                                power_reg::gpr1, //stack pointer  
                                regOffset+GPRSIZE*i //offset off SP 
           );
        emit(instr);
     }  
  }
}

void power_directToMemoryEmitter::emitRestore(bool restoreNeeded, 
                                              const regset_t *regsToRestore) {
  int regOffset =  ((power64_abi &)theABI).getGeneralRegSaveFrameOffset();
  const unsigned frameSize = theABI.getBigFrameAlignedNumBytes();

  
  unsigned saveVolatileOffset = 
     ((power64_abi &)theABI).getLocalVariableSpaceFrameOffset();
  unsigned saveCondRegOffset = 
     ((power64_abi &)theABI).getCondRegSaveFrameOffset();

  instr_t *instr = new power_instr(power_instr::load, power_instr::ldDoubleDS, 
                          power_reg::gpr0, 
                          power_reg::gpr1, //stack pointer
                          saveCondRegOffset); //load from cr position
  emit(instr);
  instr = new power_instr(power_instr::movetocondregflds,0xff, power_reg::gpr0);
  emit(instr);
  
  //now, restore gpr0 itself
  instr = new power_instr(power_instr::load, power_instr::ldDoubleDS, 
                          power_reg::gpr0, 
                          power_reg::gpr1, //stack pointer
                          saveVolatileOffset); //load from r0 position
  emit(instr);

  if (restoreNeeded) {
     assert(regsToRestore->count() <= 19);
     regset_t::const_iterator rr_iter = regsToRestore->begin();
     for (unsigned i = 0; rr_iter != regsToRestore->end() ; rr_iter++, i++) {
        const power_reg &regToRestore =  (const power_reg &) rr_iter; 
        instr = new power_instr(power_instr::load, power_instr::ldDoubleDS, 
                                regToRestore,  
                                power_reg::gpr1, //stack pointer  
                                regOffset + GPRSIZE*i //offset off SP 
           );
        emit(instr);
     }
  }
  //restore the stack pointer
  instr = new power_instr(power_instr::load, 
				   power_instr::ldDoubleX, 
				   power_reg::gpr1, 
				   power_reg::gpr0, //no second addr reg.
				   power_reg::gpr1); 
  
  emit(instr);
}
