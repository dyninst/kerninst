// callToObtainPCCodeObject.C

#include "callToObtainPCCodeObject.h"
#include "xdr_send_recv_common.h" // operations on instr_t
#include "util/h/xdr_send_recv.h"

callToObtainPCCodeObject::
callToObtainPCCodeObject(bool idelayInsnInSameBlock,
                         instr_t *idsInsn) :
   delayInsnInSameBlock(idelayInsnInSameBlock),
   dsInsn(idsInsn)
{
}

template <class T>
static void destruct1(T &t) {
   t.~T();
}

callToObtainPCCodeObject::
callToObtainPCCodeObject(XDR *xdr) : delayInsnInSameBlock(false) {
   dsInsn = (instr_t*) new sparc_instr(sparc_instr::nop); //dummy instr - will be overwritten
   if (!P_xdr_recv(xdr, delayInsnInSameBlock))
      throw xdr_recv_fail();
   
   if (delayInsnInSameBlock) {
      destruct1(dsInsn);
      if (!P_xdr_recv(xdr, dsInsn))
         throw xdr_recv_fail();
   }
}

bool callToObtainPCCodeObject::send(bool, XDR *xdr) const {
   if (!P_xdr_send(xdr, delayInsnInSameBlock))
      return false;
   
   if (delayInsnInSameBlock) {
      if (!P_xdr_send(xdr, dsInsn))
         return false;
   }

   return true;
}

void callToObtainPCCodeObject::
emitWhenRelocatedTo(bool /*tryToKeepOriginalCode*/,
                    const function_t & /*parentFn*/,
                    bbid_t owning_bb_id,
                    tempBufferEmitter &em,
                    const outliningLocations &whereBlocksNowReside,
                    bool thisIsLastCodeObjectInBlock,
                    bbid_t fallThruBlockId
                       // NOT nec. the same as the block that'll get
                       // emitted next
                    ) const {
   // Now let's think about what code to emit here.  The original code sequence
   // was "call <curraddr + 8>"; <delay>
   // The problem that I have with this sequence is the fact that call will
   // push an entry onto the USparc-II's return address stack (RAS), expecting that
   // the next ret/retl will be going to the insn following the delay slot of the
   // call (which here, happens to also be the destination of the call).
   // Of course this is all nonsense and the poor RAS will get fooled and the
   // next ret/retl, whenever that may be, will now surely prefetch instructions from
   // the wrong address.  (Not to say that it was definitely going to prefetch from
   // the correct address before, but now we're *sure* it's messed up).
   //
   // All of this can be fixed by simply using a rdasr instruction to mov the %pc
   // into %o7.  Then emit the delay slot instruction as before.  Done; have a nice
   // day.

   if (thisIsLastCodeObjectInBlock) {
      if (delayInsnInSameBlock) {
         if (whereBlocksNowReside.whoIfAnyoneWillBeEmittedAfter(owning_bb_id) ==
             fallThruBlockId) {
            // Original block ended with call <+8>; delay and fortunately, the
            // block that used to be at +8 is still going to be emitted immediately
            // after this one
	    instr_t *i = (instr_t*) new sparc_instr(sparc_instr::readstatereg, 0x5, // 5 --> %pc
						    sparc_reg::o7);
            em.emit(i);
            if (! dsInsn->isNop() ) {
	      instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
	      em.emit(dsInsn_copy);
	    }
         }
         else {
            // Orig block ended with call <+8>; delay but unfortunately, the
            // block that used to be emitted immediately after this one has been
            // moved away.  We could just emit a call to that destination block
            // (using a label), then the delay, and be pretty happy.  But that has
            // the above problem of fooling the USparc-II's return address stack.
            // An alternative is rd %pc, %o7; ba (newblock); delay but that requires
            // an extra instruction.  So (flipping a coin...) we go with the
            // former approach, despite the unspeakable thing it does to the RAS.
            em.emitCall(outliningLocations::getBlockLabel(fallThruBlockId));
	    instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
	    em.emit(dsInsn_copy);
         }
      }
      else {
         // Delay slot always resided in some other block.
         assert(false && "nyi");
      }
   }
   else {
      // This isn't even the last code object in the block, so we don't need to
      // worry about emitting code to get to the "fallee".  Also, we can assert that
      // the delay instruction was always in this code object.
      assert(delayInsnInSameBlock);

      // TODO: if tryToKeepOriginalCode is set, then call <+8>; nop
      instr_t *i = (instr_t*) new sparc_instr(sparc_instr::readstatereg, 0x5, // 5 --> %pc
					      sparc_reg::o7);
      em.emit(i);
      if (! dsInsn->isNop() ) {
	 instr_t *dsInsn_copy = instr_t::getInstr(*dsInsn);
	 em.emit(dsInsn_copy);
      }
   }
}

