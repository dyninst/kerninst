// cswitchout_emit.C

#include "util/h/out_streams.h"
#include "cswitchout_emit.h"
#include "all_vtimers.h" // max_num_vtimers
#include "vtimer.h" // xxx_routine_offset
#include "regSetManager.h"
#include "sparc_instr.h"
#include "globalLock.h"
#include "machineInfo.h"

pair<fnRegInfo, fnRegInfo> // for how-to-stop routines, hash table add, respectively
emit_cswitchout_instrumentation(tempBufferEmitter &em,
                                bool just_in_kerninstd,
                                const fnRegInfo &theFnRegInfo,
                                traceBuffer *theTraceBuffer, // can be NULL
                                unsigned trace_op_begin, // und'd if not tracing
                                unsigned trace_op_finish, // und'd if not tracing
                                kptr_t allvtimers_kernelAddr,
                                kptr_t stacklist_kernelAddr,
                                kptr_t stacklist_getfn_kernelAddr,
                                kptr_t stacklist_freefn_kernelAddr) {
   // returns fnRegInfo describing params, avail regs, etc. for how-to-stop routines.

   // Here's what we're going to do, and what we need to know:
   // 1) allocate an entry off a heap [stacklist] of vector-of-vtimerptrs.  We're going
   //    to allocate from that heap, so we also need to know the address of the
   //    heap allocation function.
   // 2) loop thru all vtimerptrs, so we need to know the address of the vector
   //    of all vtimers.  REMINDER: there can be a duplicate entry in this
   //    vector at a given time.
   // 3) add to the working vector-of-timerptrs.
   // 4) When done, if nonempty, set hash[threadid] = vector-of-timerptrs
   //    Otherwise, return vector-of-timerptrs to the stacklist heap now.

   // Register usage is a real mess currently; thank goodness for that utility
   // class regSetManager.  Anyway, here's what we need.
   // 1) %o0 and %o1 explicitly used (I hope this is temporary)
   // 2) six 32-bit robust integer registers
   //    (robust_save_link_reg, robust_saveccrs_reg, stopped_vtimers_start_reg,
   //     stopped_vtimers_iter_reg, vtimer_iter_reg, robust_vtimer_reg)
   // 3) two volatile 64-bit registers (vreg0 and vreg64_1)

   instr_t *i;

   const abi &theABI = em.getABI();

   const regSetManager theRegSetManager(theFnRegInfo.getAvailRegsForFn(),
                                        true, // do a save, if needed
                                        2, // this many vreg64's
                                        1, // this many vreg32's
                                        6, // this many robust (non-v(olatile)) reg32's
                                        sparc_reg_set(sparc_reg::o0) + sparc_reg::o1,
                                        // these are the regs that we explicitly need
                                        theABI);
   if (theRegSetManager.needsSaveRestore()) {
      dout << "cswitchout: emitted a save" << endl;
      i = new sparc_instr(sparc_instr::save, 
			  -theABI.getMinFrameAlignedNumBytes());
      em.emit(i);
   }
   else
      dout << "cswitchout: didn't need a save" << endl;

   sparc_reg robust_save_link_reg = theRegSetManager.getNthRobustReg32(0);
   // note that saving the link reg (%o7) isn't really needed when we're doing
   // a save/restore, but what the heck.
//   cout << "robust save_link_reg is " << robust_save_link_reg.disass() << endl;

   sparc_reg robust_saveccrs_reg = theRegSetManager.getNthRobustReg32(1);
//   cout << "robust_saveccrs_reg is " << robust_saveccrs_reg.disass() << endl;

   sparc_reg stopped_vtimers_start_reg = theRegSetManager.getNthRobustReg32(2);
//   cout << "robust stopped_vtimers_start_reg is " << stopped_vtimers_start_reg.disass()
//        << endl;
   
   sparc_reg stopped_vtimers_iter_reg = theRegSetManager.getNthRobustReg32(3);
//   cout << "robust stopped_vtimers_iter_reg is " << stopped_vtimers_iter_reg.disass()
//        << endl;
   
   sparc_reg vtimer_iter_reg = theRegSetManager.getNthRobustReg32(4);
//   cout << "vtimer_iter_reg is " << vtimer_iter_reg.disass() << endl;
   
   sparc_reg robust_vtimer_reg = theRegSetManager.getNthRobustReg32(5);
//   cout << "robust_vtimer_reg is " << robust_vtimer_reg.disass() << endl;

   // Pick vreg64_0 and vreg64_1: 64-bit-safe registers.
   sparc_reg vreg64_0 = theRegSetManager.getNthVReg64(0);
//   cout << "vreg64_0 is " << vreg64_0.disass() << endl;
   
   sparc_reg vreg64_1 = theRegSetManager.getNthVReg64(1);
//   cout << "vreg64_1 is " << vreg64_1.disass() << endl;

   // Pick vreg32_0: 32-bit-safe register.
   sparc_reg vreg32_0 = theRegSetManager.getNthVReg32(0);
//   cout << "vreg32_0 is " << vreg32_0.disass() << endl;

   // Emit:

   // save %o7 and %ccr into robust registers
   i = new sparc_instr(sparc_instr::mov, sparc_reg::o7, robust_save_link_reg);
   em.emit(i);
   i = new sparc_instr(sparc_instr::readstatereg, 0x2, robust_saveccrs_reg);
   em.emit(i);
   // asr2 --> %ccr

   // assert that we are executing at least at DISP_LEVEL
   // we do not need it anymore, but let's check for it
   if (!just_in_kerninstd) {
      extern machineInfo *theGlobalKerninstdMachineInfo;
      unsigned dispLevel = theGlobalKerninstdMachineInfo->getDispatcherLevel();
      i = new sparc_instr(sparc_instr::readprivreg, 0x8 /* PIL */, vreg32_0);
      em.emit(i);
      i = new sparc_instr(sparc_instr::cmp, vreg32_0, dispLevel);
      em.emit(i);
      i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                          false, // not extended
                          0x34);
      em.emit(i);
   }

   emit_global_lock(em, vreg32_0);

   // TRACE-BUFFER: log a switch-out-begin of the current thread (%g7)
   if (theTraceBuffer) {
      cout << "emitting trace buffer entry: cswitchout_begin_skiptrace" << endl;
      
      em.emitImm32(trace_op_begin, stopped_vtimers_start_reg);
      theTraceBuffer->emit_appendx(em,
                                   just_in_kerninstd,
                                   stopped_vtimers_start_reg, // reg containing op data
                                   vreg32_0, // 32-bit scratch reg
                                   vreg64_0, // 32-bit scratch reg
                                   vreg64_1, // 64-bit scratch reg
                                   "cswitchout_begin_skiptrace");
   }
   
   // call stacklist_get() with 1 arg: stacklist_kernelAddr
   // TODO: remove the hard-coded use of %o0, in both (a) passing an argument *to*
   // stacklist_get(), and in (b) getting a return value back *from* stacklist_get().
   em.emitImmAddr(stacklist_kernelAddr, sparc_reg::o0); // argument
   em.emitImmAddr(stacklist_getfn_kernelAddr, vreg64_0);
   i = new sparc_instr(sparc_instr::callandlink, vreg64_0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // after the call, restore %o7
   i = new sparc_instr(sparc_instr::mov, robust_save_link_reg, sparc_reg::o7);
   em.emit(i);
   
   // If return value from stacklist_get() is 0 then crash
   // TODO: remove the hard-coded assumption that the return value is in %o0.
   i = new sparc_instr(sparc_instr::tst, sparc_reg::o0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccEq,
                       false, // not extended
                       0x34);
   em.emit(i);

   // the vector of vtimers that we'll write to is still in %o0.
   // Let's move it to a more robust register: stopped_vtimers_start_reg
   i = new sparc_instr(sparc_instr::mov, sparc_reg::o0,
                       stopped_vtimers_start_reg);
   em.emit(i);

   // initialize stopped_vtimers_iter to stopped_vtimers_start_reg:
   i = new sparc_instr(sparc_instr::mov, stopped_vtimers_start_reg,
                       stopped_vtimers_iter_reg);
   em.emit(i);

   // start iterating through all_vtimers
   em.emitImmAddr(allvtimers_kernelAddr, vtimer_iter_reg);
   
   const tempBufferEmitter::insnOffset loopbeginoffset = em.currInsnOffset();
   
   // Given the vtimer iterator, obtain a vtimer address.  If that address turns
   // out to be 0, then we're all done (end of the vtimer iteration)
   em.emitLoadKernelNative(vtimer_iter_reg, 0,
			   robust_vtimer_reg);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       robust_vtimer_reg,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i,"done_iter");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // OK, we've got a non-null vtimer pointer.  Time to call the metric-specific
   // how-to-stop routine.  Note that we always obtain that routine by loading
   // 4 bytes starting at 16 bytes past the vtimer start.  True, that's a distasteful
   // hard-coding, but I don't know how to get around it.  Besides, it seems to
   // work for both the old 2-bit-state vtimers and the new rcount vtimers.
   // 
   // Arguments to the how-to-stop routine (pay attention; these have changed
   // recently!).  Note that when I say arguments, I don't necessarily mean starting
   // with %o0; I just mean "defined registers that the how-to-stop callee routine
   // needs to know about".
   // 1) robust_vtimer_reg
   // 2) stopped_vtimers_start_reg (if desired, callee can use this, along with
   //    the global variable max_num_vtimers, to check for overflow)
   // 3) stopped_vtimers_iter_reg (callee should append to this vector if turned off)
   const fnRegInfo howToStopRoutineFnRegInfo(false, // not inlined
                                             fnRegInfo::regsAvailAtCallTo,
                                             theRegSetManager.getAvailRegsForACallee() +
                                             // will exclude robust regs
                                             sparc_reg::reg_xcc + sparc_reg::reg_icc,
                                             // since we explicitly save/restore them
                                             fnRegInfo::blankParamInfo +
                                             make_pair(pdstring("vtimer_addr_reg"), robust_vtimer_reg) +
                                             make_pair(pdstring("vtimer_start_reg"), stopped_vtimers_start_reg) +
                                             make_pair(pdstring("vtimer_iter_reg"), stopped_vtimers_iter_reg),
                                             sparc_reg::g0 // result reg
      );
                                             
   em.emitLoadKernelNative(robust_vtimer_reg, stop_routine_offset,
			   vreg32_0);
   i = new sparc_instr(sparc_instr::callandlink, vreg32_0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   // after the call, restore %o7
   i = new sparc_instr(sparc_instr::mov, robust_save_link_reg, sparc_reg::o7);
   em.emit(i);

   em.emitLabel("gotonext");

   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
                       false, // NOT annulled
                       true, // predict taken
                       true, // xcc (matters?)
                       -(em.currInsnOffset() - loopbeginoffset));
   em.emit(i);
   
   i = new sparc_instr(sparc_instr::add, vtimer_iter_reg, sizeof(kptr_t), 
		       vtimer_iter_reg);
   em.emit(i);
      // delay slot -- move to the next vtimer in the vector of all vtimers.  It may
      // now point to a NULL element, in which case we're done.

   em.emitLabel("done_iter");
   
   // store "stopped_vtimers_start_reg" into the hash table, if it's anything
   // more than an empty vector.
   i = new sparc_instr(sparc_instr::sub,
                       stopped_vtimers_iter_reg,
                       stopped_vtimers_start_reg,
                       vreg64_0);
   em.emit(i);

   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       true, // predict taken
		       vreg64_0,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "skip_hashstore");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // First thing's first; we need to put a NULL sentinel at the end of the
   // vector of stopped vtimers
   em.emitStoreKernelNative(sparc_reg::g0, // src value
			    stopped_vtimers_iter_reg, 0);

   // call hash_table_add() with args:
   // %g7 [key], and stopped_vtimers_start_reg [value].
   // We don't yet know the address of hash table add because it hasn't yet been
   // emitted.  Not a problem, we'll just use a symAddr() for now.

   em.emitSymAddr("hashTableAddAddr", vreg64_0);
   i = new sparc_instr(sparc_instr::callandlink, vreg64_0);
   em.emit(i);
   // can't be a tail call since we still need to restore %ccr
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   const fnRegInfo hashTableAddFnRegInfo(false, // not inlined
                                         fnRegInfo::regsAvailAtCallTo,
                                         theRegSetManager.getAdjustedParentSet() -
                                         robust_save_link_reg -
                                         robust_saveccrs_reg,
                                         // that's all we need to subtract: other
                                         // robust regs (like vtimer_iter_reg)
                                         // don't need to be saved any more.
                                         // stopped_vtimers_start_reg *does*
                                         // need to be saved, since it's used below.
                                         // By making it a parameter to hash
                                         // table add, it won't get clobbered.
                                         fnRegInfo::blankParamInfo +
                                         make_pair(pdstring("key"), sparc_reg::g7) +
                                         make_pair(pdstring("value"), stopped_vtimers_start_reg),
                                         vreg64_0 // put result here
      );

   // assert that the return value was "success" (1)
   i = new sparc_instr(sparc_instr::sub, vreg64_0, 1, vreg64_0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::tst, vreg64_0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                       false, // not extended
                       0x34);
   em.emit(i);

   // after the call, restore %o7
   i = new sparc_instr(sparc_instr::mov, robust_save_link_reg, sparc_reg::o7);
   em.emit(i);

   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
		       true, // annulled
		       true, // predict taken
		       true, // xcc (matters?)
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "restore_ccr_and_return");

   em.emitLabel("skip_hashstore");

   assert(stopped_vtimers_start_reg != vreg64_0);
   
   // since we're not going to store anything to the hash table, we should at least
   // return the unused stacklist heap element (vector of timerptrs) to the stacklist
   // heap.  There are 2 args to pass: the stacklist heap, and the vector being
   // freed.
   em.emitImmAddr(stacklist_kernelAddr, sparc_reg::o0); // argument
   em.emitImmAddr(stacklist_freefn_kernelAddr, vreg64_0);
   i = new sparc_instr(sparc_instr::callandlink, vreg64_0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::mov, stopped_vtimers_start_reg, 
		       sparc_reg::o1);
   em.emit(i);
   // delay slot -- second argument

   // after the call, restore %o7 and %ccr
   i = new sparc_instr(sparc_instr::mov, robust_save_link_reg, sparc_reg::o7);
   em.emit(i);

   em.emitLabel("restore_ccr_and_return");

// Sorry, but because there are now old vs. new vtimer formats, the following
// assertion code is no longer always valid.
//     // One final assert: we have completed context switching out, so ALL vtimers should
//     // now be stopped, period.  This is a nice, strong assert to make.
//     emit_assert_all_vtimers_are_stopped(em, allvtimers_kernelAddr,
//                                         vtimer_iter_reg,
//                                         sparc_reg::o1 // an avail 64-bit register
//                                         );

   i = new sparc_instr(sparc_instr::writestatereg, robust_saveccrs_reg, 0x2);
   em.emit(i);

   // TRACE-BUFFER: log a switch-out-finish of the current thread (%g7)
   if (theTraceBuffer) {
      cout << "emitting trace buffer entry: cswitchout_finish_skiptrace" << endl;
      em.emitImm32(trace_op_finish, robust_saveccrs_reg);
      // OK to use this register since we're done with it
      theTraceBuffer->emit_appendx(em,
                                   just_in_kerninstd,
                                   robust_saveccrs_reg, // reg containing op data
                                   vreg32_0, // reg32 scratch
                                   vreg64_0, // reg32 scratch
                                   vreg64_1, // reg64 scratch
                                   "cswitchout_finish_skiptrace");
   }

   emit_global_unlock(em, vreg32_0);

   if (theRegSetManager.needsSaveRestore()) {
      i = new sparc_instr(sparc_instr::ret);
      em.emit(i);
      i = new sparc_instr(sparc_instr::restore);
      em.emit(i);
   }
   else {
      i = new sparc_instr(sparc_instr::retl);
      em.emit(i);
      i = new sparc_instr(sparc_instr::nop);
      em.emit(i);
   }

   return make_pair(howToStopRoutineFnRegInfo, hashTableAddFnRegInfo);
}
