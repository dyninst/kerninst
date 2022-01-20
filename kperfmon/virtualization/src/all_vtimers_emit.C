// all_vtimers_emit.C

#include "all_vtimers_emit.h"
#include "sparc_instr.h"

void emit_all_vtimers_initialize(tempBufferEmitter &em,
                                 dptr_t all_vtimers_kerninstdAddr) {
   // ------------------------------------
   // Params: none
   // ------------------------------------

   em.emitImmAddr(all_vtimers_kerninstdAddr, sparc_reg::o0);

   // situation: %o0 contains address of all_vtimers structure...uninitialized memory
   // that we need to initialize.
   
   // To conform to the sparcv8plus abi, it's important that we only use %g and %o
   // registers to hold 64-bit values.  Since we're an optimized leaf fn, that
   // turns out to be easy to keep.

   // set %o1 <-- iterator.  Initialize to %o0, which is the start of the vector.
   instr_t *i = new sparc_instr(sparc_instr::mov, sparc_reg::o0, sparc_reg::o1);
   em.emit(i);

   // set %o3 <-- offset_to_finish
   em.emitImm32(all_vtimers_offset_to_finish, sparc_reg::o3);

   // set %o2 <-- address of finish (start + total num data bytes)
   i = new sparc_instr(sparc_instr::add,
                       sparc_reg::o1, sparc_reg::o3,
                       sparc_reg::o2 // dest
                       ); 
   em.emit(i);

   // the_vtimers->finish = start
   // %o3 still contains all_vtimers_offset_to_finish, and we'll make use of it.
   em.emitStoreDaemonNative(sparc_reg::o0, // src value: start ptr
			    sparc_reg::o0, // first part of address
			    sparc_reg::o3); // second part of address

   // Loop: while (iter != &finish) *iter++ = NULL;
   // Reminder: %o1 is the iterator.  %o2 is address of finish.
   const tempBufferEmitter::insnOffset loopstart = em.currInsnOffset();

   i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_xor, 
		       false, // no cc
                       sparc_reg::o4, // dest
                       sparc_reg::o1, sparc_reg::o2);
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       sparc_reg::o4,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "loop_end");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i); // delay slot

   em.emitStoreKernelNative(sparc_reg::g0, // src value
			    sparc_reg::o1, 0); // address (the iterator)

   i = new sparc_instr(sparc_instr::add,
                       sparc_reg::o1, sizeof(kptr_t), // value
                       sparc_reg::o1 // dest
                       );
   em.emit(i);
   i = new sparc_instr(sparc_instr::bicc, sparc_instr::iccAlways, 
		       true, // annulled
                       -(em.currInsnOffset() - loopstart));
   em.emit(i);

   em.emitLabel("loop_end");
   
   i = new sparc_instr(sparc_instr::retl);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
}

void emit_all_vtimers_destroy(tempBufferEmitter &em) {
   
   // ------------------------------------
   // Params:
   //    %o0: ptr to all_vtimers structure
   // ------------------------------------

   // set %o3 <-- all_vtimers_offset_to_finish
   em.emitImm32(all_vtimers_offset_to_finish, sparc_reg::o3);

   // set finish to start
   em.emitStoreDaemonNative(sparc_reg::o0, // src value (start)
			    sparc_reg::o0, // address, part 1:start
			    sparc_reg::o3); // address, part 2:offset to finish

   // set first vtimer ptr to NULL
   em.emitStoreKernelNative(sparc_reg::g0, // src value
			    sparc_reg::o0, 0); // address
   
   instr_t *i = new sparc_instr(sparc_instr::retl);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
}

void emit_all_vtimers_add(tempBufferEmitter &em) {
   // ---------------------------------------------------------
   // Params:
   //    %o0: ptr to all_vtimers structure (in kerninstd space)
   //    %o1: address of new vtimer (in kernel space)
   // Result:
   //    %o0: 1 iff successful; 0 if not [overflow]
   // ---------------------------------------------------------

   // set %o3 <-- offset_to_finish:
   em.emitImm32(all_vtimers_offset_to_finish, sparc_reg::o3);

   // set %o2 to the old value of finish (a ptr to w/in the vector of vtimer ptrs)
   em.emitLoadDaemonNative(sparc_reg::o0, sparc_reg::o3, // src address
			   sparc_reg::o2); // dest

   // assert that the old value of finish is >= the start of the all_vtimers
   // structure
   instr_t *i = new sparc_instr(sparc_instr::sub, sparc_reg::o2, sparc_reg::o0,
				sparc_reg::o4);
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regGrOrEqZero,
		       false, // not annulled
		       true, // predict taken
		       sparc_reg::o4,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "OK_1");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   i = new sparc_instr(sparc_instr::trap, 0x34);
   em.emit(i);

   em.emitLabel("OK_1");
   
   // %o4 is presently the byte offset, within all_vtimers, of the value of the
   // finish field.
   i = new sparc_instr(sparc_instr::sub, sparc_reg::o4, 
		       all_vtimers_offset_to_finish,
                       sparc_reg::o4);
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regLessZero,
		       false, // not annulled
		       true, // predict taken
		       sparc_reg::o4,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "OK_2");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   i = new sparc_instr(sparc_instr::trap, 0x34);
   em.emit(i);

   em.emitLabel("OK_2");

   // increment finish to the next item
   // In particular, set %o4 to the new value of finish.
   i = new sparc_instr(sparc_instr::add, sparc_reg::o2, sizeof(kptr_t), 
		       sparc_reg::o4);
   em.emit(i);

   // check for overflow; return 0 in such a case
   i = new sparc_instr(sparc_instr::add, sparc_reg::o0, 
		       sparc_reg::o3, sparc_reg::o5);
   em.emit(i);
      // %o5: address of finish field

   i = new sparc_instr(sparc_instr::sub,
                       sparc_reg::o5, sparc_reg::o4, sparc_reg::o5);
   em.emit(i);
      // %o5: if positive, then no overflow

   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regGrZero,
		       false, // not annulled
		       true, // predict taken
		       sparc_reg::o5,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "OK_3");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   // overflow; return 0
   i = new sparc_instr(sparc_instr::retl);
   em.emit(i);
   i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, sparc_reg::o0);
   em.emit(i);
   
   em.emitLabel("OK_3");

   // set *new_finish to NULL
   em.emitStoreKernelNative(sparc_reg::g0, // src value
			    sparc_reg::o4, 0); // address

   // set *old_finish to the new vtimer ptr
   // (*old_finish used to be NULL, by definition)
   em.emitStoreKernelNative(sparc_reg::o1, // src value
			    sparc_reg::o2, 0); // address

   // lest we forget, write new_finish to memory (heretofore it's only been
   // updated in a register)
   em.emitStoreDaemonNative(sparc_reg::o4, // src value
			    sparc_reg::o0, sparc_reg::o3); // address of finish
   
   i = new sparc_instr(sparc_instr::retl);
   em.emit(i);
   i = new sparc_instr(sparc_instr::mov, 0x1, sparc_reg::o0);
   em.emit(i);
      // delay slot -- return 1 (success)
}

void emit_all_vtimers_remove(tempBufferEmitter &em) {
   // ------------------------------------------
   // Params:
   //    %o0: ptr to all_vtimers structure
   //    %o1: address of vtimer to find & remove
   // Result
   //    %o0: 1 iff successful, 0 if not [underflow]
   // ------------------------------------------

   // iterator will be %o3; initialize it
   instr_t *i = new sparc_instr(sparc_instr::mov, sparc_reg::o0, sparc_reg::o3);
   em.emit(i);

   // set %o2 to all_vtimers_offset_to_finish
   em.emitImm32(all_vtimers_offset_to_finish, sparc_reg::o2);

   // set %o4 to value of finish field
   em.emitLoadDaemonNative(sparc_reg::o0, sparc_reg::o2, // addr part 2
			   sparc_reg::o4); // dest

   // set %o5 to "last" (finish - sizeof(kptr_t))
   i = new sparc_instr(sparc_instr::sub, false, false, // no cc or carry
                       sparc_reg::o5, // dest
                       sparc_reg::o4, sizeof(kptr_t));
   em.emit(i);

   // start of loop
   tempBufferEmitter::insnOffset loopstart_offset = em.currInsnOffset();
   
   // done? (does iter == finish?)
   i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_xor, false, // no cc
                       sparc_reg::g1, // dest
                       sparc_reg::o3, // iterator
                       sparc_reg::o4 // finish
                       );
   em.emit(i);
   
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       sparc_reg::g1,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "failure");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i); // delay slot

   // does *iter equal vtimer_addr?
   // set %g1 to *iter, and compare it to %o1
   em.emitLoadKernelNative(sparc_reg::o3, 0, // address
			   sparc_reg::g1); // dest

   i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_xor, 
		       false, // no cc
                       sparc_reg::g1, // dest
                       sparc_reg::g1, sparc_reg::o1
                       );
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       sparc_reg::g1,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "equal");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   // not a match.  Bump up iter and jump back to start of loop
   i = new sparc_instr(sparc_instr::add, sparc_reg::o3, sizeof(kptr_t), 
		       sparc_reg::o3);
   em.emit(i);

   i = new sparc_instr(sparc_instr::bicc, sparc_instr::iccAlways,
                       true, // annulled
                       -(em.currInsnOffset() - loopstart_offset)
                       );
   em.emit(i);

   em.emitLabel("equal");
   
   // To do "*iter = *last": ld last, st to iter
   em.emitLoadKernelNative(sparc_reg::o5, 0, // address
			   sparc_reg::g1); // dest

   em.emitStoreKernelNative(sparc_reg::g1, // src value
			    sparc_reg::o3, 0); // address

   // "*last = NULL":
   em.emitStoreKernelNative(sparc_reg::g0, // src value
			    sparc_reg::o5, 0); // address

   // "--the_vtimers->finish"
   i = new sparc_instr(sparc_instr::sub, sparc_reg::o4, sizeof(kptr_t), 
		       sparc_reg::g1);
   em.emit(i);
   em.emitStoreDaemonNative(sparc_reg::g1, // src value
			    sparc_reg::o0, // part 1 of address
			    sparc_reg::o2); // part 2 of address (offset 
                                            // to finish field)

   // success -- return 1
   i = new sparc_instr(sparc_instr::retl);
   em.emit(i);
   i = new sparc_instr(sparc_instr::mov, 0x1, sparc_reg::o0);
   em.emit(i);

   em.emitLabel("failure");

   i = new sparc_instr(sparc_instr::retl); 
   em.emit(i);
   i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, sparc_reg::o0);
   em.emit(i);
}

