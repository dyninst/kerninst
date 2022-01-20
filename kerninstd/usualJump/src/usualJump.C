// usualJump.C

#include "usualJump.h"
#include "tempBufferEmitter.h"
#include "directToMemoryEmitter.h"
#include "abi.h"
#include "insnVec.h"

#include "sparc_instr.h"
#include "sparc_reg.h"

// ----------------------------------------------------------------------

class ba_a_usualJump : public usualJump {
 private:
   template <class emitter> void emit(emitter &) const;
 public:
   ba_a_usualJump(kptr_t ifrom, kptr_t ito) : usualJump(ifrom, ito) { }
   void generate(directToMemoryEmitter_t &em) const { emit(em); }
   void generate(tempBufferEmitter &em) const { emit(em); }
   unsigned numInsnBytes() const { return 4; }
};

class ba_usualJump : public usualJump { // leaves delay slot open
 private:
   template <class emitter> void emit(emitter &) const;
 public:
   ba_usualJump(kptr_t ifrom, kptr_t ito) : usualJump(ifrom, ito) { }
   void generate(directToMemoryEmitter_t &em) const { emit(em); }
   void generate(tempBufferEmitter &em) const { emit(em); }
   unsigned numInsnBytes() const { return 4; }
};

class call_nop_usualJump : public usualJump {
 private:
   template <class emitter> void emit(emitter &) const;
 public:
   call_nop_usualJump(kptr_t ifrom, kptr_t ito) :
           usualJump(ifrom, ito) { }
   void generate(directToMemoryEmitter_t &em) const { emit(em); }
   void generate(tempBufferEmitter &em) const { emit(em); }
   unsigned numInsnBytes() const { return 8; }
};

class call_usualJump : public usualJump { // leaves delay slot open
 private:
   template <class emitter> void emit(emitter &) const;
 public:
   call_usualJump(kptr_t ifrom, kptr_t ito) : usualJump(ifrom, ito) { }
   void generate(directToMemoryEmitter_t &em) const { emit(em); }
   void generate(tempBufferEmitter &em) const { emit(em); }
   unsigned numInsnBytes() const { return 4; }
};

class move_call_move_usualJump : public usualJump {
 private:
   sparc_reg freeReg;
   template <class emitter> void emit(emitter &) const;
 public:
   move_call_move_usualJump(kptr_t ifrom,
                            kptr_t ito,
                            const sparc_reg &iFreeReg) :
        usualJump(ifrom, ito), freeReg(iFreeReg) {}
   void generate(directToMemoryEmitter_t &em) const { emit(em); }
   void generate(tempBufferEmitter &em) const { emit(em); }
   unsigned numInsnBytes() const { return 12; }
};

class save_call_restore_usualJump : public usualJump {
 private:
   template <class emitter> void emit(emitter &) const;
 public:
   save_call_restore_usualJump(kptr_t ifrom, kptr_t ito) :
                          usualJump(ifrom, ito) {}
   void generate(directToMemoryEmitter_t &em) const { emit(em); }
   void generate(tempBufferEmitter &em) const { emit(em); }
   unsigned numInsnBytes() const { return 12; }
};

class farJump : public usualJump {
 protected:
   insnVec_t *iv;
   template <class emitter> void emit(emitter &theEmitter) const 
   {
      const abi &theABI = theEmitter.getABI();

      insnVec_t::const_iterator iter = iv->begin();
      for (; iter != iv->end(); iter++) {
	 if ((*iter)->isSave()) {
	    // We constructed the save with a bogus frame size (didn't have
	    // access to the abi). Lets fix this right now.
	    // see save_genaddr_jmp_restore below
	    instr_t *i = new sparc_instr(sparc_instr::save,
					 -theABI.getMinFrameAlignedNumBytes());
	    theEmitter.emit(i);
	 } 
	 else {
	    instr_t *i = instr_t::getInstr(**iter);
	    theEmitter.emit(i);
	 }
      }
   }
 public:
   farJump(kptr_t ifrom, kptr_t ito) : usualJump(ifrom, ito) {
     iv = insnVec_t::getInsnVec();
   }
   virtual ~farJump () { delete iv; }
   void generate(directToMemoryEmitter_t &em) const 
   { 
      emit(em); 
   }
   void generate(tempBufferEmitter &em) const 
   { 
      emit(em); 
   }
   unsigned numInsnBytes() const 
   { 
      return iv->numInsns() * 4; 
   }
};

class genaddr_jmp_nop_usualJump : public farJump {
 public:
   genaddr_jmp_nop_usualJump(kptr_t ifrom, kptr_t ito,
			     const sparc_reg &iFreeReg) :
      farJump(ifrom, ito) 
   {
      sparc_reg freeReg = iFreeReg;
      sparc_instr::genJumpAndLink(iv, ito, freeReg);
      instr_t *nop = new sparc_instr(sparc_instr::nop);
      iv->appendAnInsn(nop);
   }
};

class genaddr_jmp_usualJump : public farJump { // leaves ds open
 public:
   genaddr_jmp_usualJump(kptr_t ifrom, kptr_t ito,
			 const sparc_reg &iFreeReg) :
      farJump(ifrom, ito) 
   {
      sparc_reg freeReg = iFreeReg;
      sparc_instr::genJumpAndLink(iv, ito, freeReg);
   }
};

class save_genaddr_jmp_restore_usualJump : public farJump { // leaves ds open
 public:
   save_genaddr_jmp_restore_usualJump(kptr_t ifrom, kptr_t ito) :
      farJump(ifrom, ito) 
   {
      // We can't calculate the required frame size. Let's construct a dummy
      // version for now. emit() will detect a save and fix it
      instr_t *i = new sparc_instr(sparc_instr::save, -0x60);
      iv->appendAnInsn(i); 
      sparc_instr::genJumpAndLink(iv, ito, sparc_reg::l0);
      i = new sparc_instr(sparc_instr::restore);
      iv->appendAnInsn(i);
   }
};

// The following methods are placed in the .h file so the compiler has no trouble
// locating them:
template <class emitter>
void ba_a_usualJump::emit(emitter &theEmitter) const {
   const long displacement = (long)to - (long)from;
   instr_t *i = new sparc_instr(sparc_instr::bicc, sparc_instr::iccAlways,
				true, // annulled
				displacement);
   theEmitter.emit(i);
}

template <class emitter>
void ba_usualJump::emit(emitter &theEmitter) const {
   const long displacement = (long)to - (long)from;
   instr_t *i = new sparc_instr(sparc_instr::bicc, sparc_instr::iccAlways,
				false, // annulled
				displacement);
   theEmitter.emit(i);
}

template <class emitter>
void call_nop_usualJump::emit(emitter &theEmitter) const {
   instr_t *i = new sparc_instr(sparc_instr::callandlink, to, from);
   theEmitter.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   theEmitter.emit(i);
}

template <class emitter>
void call_usualJump::emit(emitter &theEmitter) const {
   instr_t *i = new sparc_instr(sparc_instr::callandlink, to, from);
   theEmitter.emit(i);
}


template <class emitter>
void move_call_move_usualJump::emit(emitter &theEmitter) const {
   instr_t *i = new sparc_instr(sparc_instr::mov, sparc_reg::o7, freeReg);
   theEmitter.emit(i);
   i = new sparc_instr(sparc_instr::callandlink, to, from+4);
   theEmitter.emit(i);
   i = new sparc_instr(sparc_instr::mov, freeReg, sparc_reg::o7);
   theEmitter.emit(i);
}

template <class emitter>
void save_call_restore_usualJump::emit(emitter &theEmitter) const {
   const abi &theABI = theEmitter.getABI();
   instr_t *i = new sparc_instr(sparc_instr::save,
				-theABI.getMinFrameAlignedNumBytes());
   theEmitter.emit(i);
   i = new sparc_instr(sparc_instr::callandlink, to, from+4);
   theEmitter.emit(i);
   i = new sparc_instr(sparc_instr::restore);
   theEmitter.emit(i);
}


// ----------------------------------------------------------------------

usualJump *usualJump::allocate_appropriate(kptr_t from, kptr_t to,
                                           const sparc_reg_set &availRegs,
                                           bool leaveDelaySlotOpen) {
   // can throw NotEnoughRegForUsualJump

   if (sparc_instr::inRangeOfBicc(from, to))
      if (leaveDelaySlotOpen)
         return new ba_usualJump(from, to);
      else
         return new ba_a_usualJump(from, to);

   if (availRegs.exists(sparc_reg::o7) && sparc_instr::inRangeOfCall(from, to))
      if (leaveDelaySlotOpen)
         return new call_usualJump(from, to);
      else
         return new call_nop_usualJump(from, to);

   // For the remaining options, we hope to have 1 available scratch register.
   sparc_reg_set::const_iterator iter = availRegs.begin_intregs_afterG0();
   if (iter == availRegs.end_intregs()) {
      // Sorry, no available registers.  We'll need to use some kind of save/restore.
      if (sparc_instr::inRangeOfCall(from, to) && !leaveDelaySlotOpen)
         // save; call; restore.  Can't leave delay slot open.
         return new save_call_restore_usualJump(from, to);
      else if (!leaveDelaySlotOpen)
         // save; genaddr; jmp; restore
         return new save_genaddr_jmp_restore_usualJump(from, to);
      else
         // ugh; we can't handle this case (have no scratch registers and need to
         // leave the delay slot open).
         throw NotEnoughRegForUsualJump();
   }
   
   const sparc_reg &freeReg = (const sparc_reg&)*iter;

   if (leaveDelaySlotOpen || !sparc_instr::inRangeOfCall(from, to)) {
      if (leaveDelaySlotOpen)
         return new genaddr_jmp_usualJump(from, to, freeReg);
      else
         return new genaddr_jmp_nop_usualJump(from, to, freeReg);
   }
   else
      return new move_call_move_usualJump(from, to, freeReg);
}
