// usualJump.h

#ifndef _USUAL_JUMP_H_
#define _USUAL_JUMP_H_

#include <inttypes.h> // uint32_t
#include "util/h/kdrvtypes.h"
class directToMemoryEmitter_t;
class tempBufferEmitter;
class sparc_reg_set;

class usualJump {
 protected:
   kptr_t from, to;
   
 public:
   class NotEnoughRegForUsualJump {};
   
   usualJump(kptr_t ifrom, kptr_t ito) : from(ifrom), to(ito) {}
   
   virtual ~usualJump() {}
   
   virtual void generate(directToMemoryEmitter_t &) const = 0;
   virtual void generate(tempBufferEmitter &) const = 0; // used by outlining code

   virtual unsigned numInsnBytes() const = 0; // too bad can't be static
      // This is always exact, not a conservative estimate.

   static unsigned maxNumInsnBytesNoFreeRegsNoOpenDS() {
      // ds can't (always) be left open with no free regs; 
      // that's just the way it is.
      if (sizeof(kptr_t) == sizeof(uint32_t)) {
	 return 16; // 32-bit version
         // save; sethi; jmp; restore (16), although 
	 // save; call; restore usually works in practice (12)
      }
      else {
	 return 24; // 64-bit version
	 // 6 insns: "save; genImmAddrHi (3 insns max); jmp; restore"
      }
   }
   static unsigned maxNumInsnBytes1FreeReg(bool leaveDsOpen) {
      unsigned rv;

     if (sizeof(kptr_t) == sizeof(uint32_t)) {
	 rv = 8; // 32-bit version: sethi; jmp
      }
      else {
	 rv = 16; // 64-bit version: "genImmAddrHi (3 insns max); jmp"
      }
      if (!leaveDsOpen) {
	 rv += 4; // nop
      }

      return rv;
   }
   static unsigned maxNumInsnBytes1FreeRegPlusDelayIfAny(bool/*leaveDsOpen*/) {
     if (sizeof(kptr_t) == sizeof(uint32_t)) {
	// 32-bit version: sethi; jmp; the dalay
	return (8 + 4); 
     }
     else {
	// 64-bit version: "genImmAddrHi (3 insns max); jmp; the delay"
	return (16 + 4); 
     }
   }

   // probably doesn't belong in this class:
   static usualJump *allocate_appropriate(kptr_t from, kptr_t to,
                                          const sparc_reg_set &availRegs,
                                          bool leaveDelaySlotOpen);
      // NOTE: can throw NotEnoughRegForUsualJump
};

#endif
