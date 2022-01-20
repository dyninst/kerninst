// stacklist_emit.C

#include "stacklist_emit.h"
#include "stacklist.h" // e.g. for stacklistitem_nbytes
#include "sparc_instr.h"

void emit_stacklist_initialize(tempBufferEmitter &init_em,
                               kptr_t stacklist_kernelAddr,
                               dptr_t stacklist_kerninstdAddr) {
   // %o0: stack of stacklist in kernel space
   // %o1: stack of stacklist in kerninstd space
   init_em.emitImmAddr(stacklist_kernelAddr, sparc_reg::o0);
   init_em.emitImmAddr(stacklist_kerninstdAddr, sparc_reg::o1);

   // set the_head->head to the start of the stacklist
   // to accomplish this, write the value (%o0 + 0) to address (%o1 + offset_to_head)
   init_em.emitImm32(stacklist_offset_to_head, sparc_reg::o2);
   init_em.emitStoreKernelNative(sparc_reg::o0, // src value (must be a kernel
				                // address)
				 sparc_reg::o1, sparc_reg::o2);

   // %o2: iterator, as a byte offset
   // %o3: last, as a byte offset
   instr_t *i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, sparc_reg::o2);
   init_em.emit(i);
   init_em.emitImm32(stacklist_offset_to_head - stacklistitem_nbytes, sparc_reg::o3);

   // if iter >= last then goto "after_loop"
   const tempBufferEmitter::insnOffset loopstart_offset = init_em.currInsnOffset();

   i = new sparc_instr(sparc_instr::sub, sparc_reg::o2, sparc_reg::o3,
		       sparc_reg::o4);
   init_em.emit(i);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regGrOrEqZero,
		       false, // not annulled,
		       false, // predict not taken
		       sparc_reg::o4,
		       0 // displacement, for now
		       );
   init_em.emitCondBranchToForwardLabel(i, "after_loop");
   i = new sparc_instr(sparc_instr::nop);
   init_em.emit(i);

   // Set %o4 to next_iter (as a byte offset)
   i = new sparc_instr(sparc_instr::add, sparc_reg::o2,
		       stacklistitem_nbytes,
		       sparc_reg::o4 // dest
		       );
   init_em.emit(i);

   // write the value (next_iter + stacklist_kernelAddr) [%o4 + %o0] to
   // (iter + stacklist_kerninstdAddr) [%o2 + %o1]
   i = new sparc_instr(sparc_instr::add, sparc_reg::o0, sparc_reg::o4, 
		       sparc_reg::o5);
   init_em.emit(i);
   init_em.emitStoreKernelNative(sparc_reg::o5, // src value to store
				 sparc_reg::o1, sparc_reg::o2);

   // back to start of loop
   i = new sparc_instr(sparc_instr::bicc, sparc_instr::iccAlways,
		       false, // not annulled
		       -(init_em.currInsnOffset() - loopstart_offset)
		       );
   init_em.emit(i);
   i = new sparc_instr(sparc_instr::mov, sparc_reg::o4, sparc_reg::o2);
   init_em.emit(i);
      // delay slot: iter = next_iter

   // After loop: iter->next = NULL is the final thing to do
   init_em.emitLabel("after_loop");

   init_em.emitStoreKernelNative(sparc_reg::g0, // src value
				 sparc_reg::o1, sparc_reg::o2); // &iter->next
   // [same as iter], in kerninstd space


   i = new sparc_instr(sparc_instr::retl);
   init_em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   init_em.emit(i);
}

void emit_stacklist_get(tempBufferEmitter &get_em) {
   // arguments:
   //    %o0: struct stacklist *
   // return value:
   //    %o0: data that you can now write to, or NULL if overflowed
   // NOTE: Writes to condition codes without saving them.  This should be OK in
   // practice.

   get_em.emitImm32(stacklist_offset_to_head, sparc_reg::o2);

   // %o1 <- oldhead
   get_em.emitLoadKernelNative(sparc_reg::o0, sparc_reg::o2,
			       sparc_reg::o1); // dest

   // if oldhead NULL then return NULL.
   instr_t *i = new sparc_instr(sparc_instr::bpr, sparc_instr::regNotZero,
				false, // not annulled
				true, // predict taken
				sparc_reg::o1,
				0 // displacement for now
				);
   get_em.emitCondBranchToForwardLabel(i, "nonnull_oldhead");
   i = new sparc_instr(sparc_instr::nop);
   get_em.emit(i);

   // null oldhead, so return NULL
   i = new sparc_instr(sparc_instr::retl);
   get_em.emit(i);
   i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, sparc_reg::o0);
   get_em.emit(i);
      // delay slot: set return value (%o0) to NULL

   get_em.emitLabel("nonnull_oldhead");
   
   // result = &oldhead->timers (result'll be %o3 for now)
   i = new sparc_instr(sparc_instr::add, sparc_reg::o1, 
		       stacklistitem_timers_offset, sparc_reg::o3);
   get_em.emit(i);
      // oldhead->timers is located 8 bytes past oldhead

   // thestacklist->head = oldhead->next
   get_em.emitLoadKernelNative(sparc_reg::o1, 0, // &oldhead->next is same 
                                                 // as oldhead
			       sparc_reg::o4);
   
   get_em.emitStoreKernelNative(sparc_reg::o4, // src value
				sparc_reg::o0, sparc_reg::o2); 
                                // thestacklist->head

   // return result (%o3).
   // But first, some assertions: %o3 >= %o0 and %o3 <= %o0 + %o2
   i = new sparc_instr(sparc_instr::sub, true, // cc
		       false, // no carry
		       sparc_reg::g0, // dest (we're just interested in cc)
		       sparc_reg::o3, sparc_reg::o0);
   get_em.emit(i);

   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNegative,
		       false, // not extended
		       0x34);
   get_em.emit(i);
   i = new sparc_instr(sparc_instr::add, sparc_reg::o0, sparc_reg::o2,
		       sparc_reg::o4);
   get_em.emit(i);
   i = new sparc_instr(sparc_instr::sub, true, // cc
		       false, // no carry
		       sparc_reg::g0, // dest (just interested in cond codes)
		       sparc_reg::o4, sparc_reg::o3);
   get_em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNegative,
		       false, // not extended
		       0x34);
   get_em.emit(i);
   
   i = new sparc_instr(sparc_instr::retl);
   get_em.emit(i);
   i = new sparc_instr(sparc_instr::mov, sparc_reg::o3, sparc_reg::o0);
   get_em.emit(i);
      // delay slot: set return value
}

void emit_stacklist_free(tempBufferEmitter &free_em) {
   // stacklist_free:
   // arguments:
   // %o0: struct stacklist *
   // %o1: vector being freed

   free_em.emitImm32(stacklist_offset_to_head, sparc_reg::o2);

   // translate argument %o1 into a proper stacklistitem*
   instr_t *i = new sparc_instr(sparc_instr::sub, sparc_reg::o1, 
				stacklistitem_timers_offset, sparc_reg::o1); 
   free_em.emit(i);
   // now %o1 is "newhead"

   // some assertions:
   // assert (%o1 >= %o0) and assert (%o1 < %o0 + %o2)
   i = new sparc_instr(sparc_instr::sub, true, // cc
		       false, // no carry
		       sparc_reg::g0, // only care about setting cc
		       sparc_reg::o1, sparc_reg::o0);
   free_em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNegative,
		       false, // not extended
		       0x34);
   free_em.emit(i);
   i = new sparc_instr(sparc_instr::add, sparc_reg::o0, sparc_reg::o2,
		       sparc_reg::o3);
   free_em.emit(i);
   i = new sparc_instr(sparc_instr::sub, true, // cc
		       false, // no carry
		       sparc_reg::g0, // only care about setting cc
		       sparc_reg::o3, sparc_reg::o1);

   free_em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccLessOrEq,
		       false, // not extended
		       0x34);
   free_em.emit(i);
   
   // item->next = thestacklist->head
   free_em.emitLoadKernelNative(sparc_reg::o0, sparc_reg::o2, // address
				sparc_reg::o3);
   free_em.emitStoreKernelNative(sparc_reg::o3, // src value (old head)
				 sparc_reg::o1, 0); // addr: newhead->next 
                                                    // (same as newhead)

   // thestacklist->head = item
   free_em.emitStoreKernelNative(sparc_reg::o1, // value: newhead
				 sparc_reg::o0, sparc_reg::o2); //theheap->head
   
   i = new sparc_instr(sparc_instr::retl);
   free_em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   free_em.emit(i);
}
