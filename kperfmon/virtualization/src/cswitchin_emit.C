// cswitchin_emit.C

#include "vtimer.h" // xxx_routine_offset
#include "stacklist.h"
#include "cswitchin_emit.h"
#include "vtimer_asserts.h"
#include "regSetManager.h"
#include "sparc_instr.h"
#include "globalLock.h"
#include "machineInfo.h"

pair<fnRegInfo, fnRegInfo> // for hash table remove, how-to-restart, respectively
emit_cswitchin_instrumentation(tempBufferEmitter &em,
                               bool just_kerninstd,
                               const fnRegInfo &theFnRegInfo,
                                  // params [none], avail regs, result reg [none]
                               traceBuffer *theTraceBuffer, // can be NULL
                               unsigned trace_op_begin, // undef'd if not tracing
                               unsigned trace_op_finish, // undef'd if not tracing
                               kptr_t stacklist_kernelAddr,
                               kptr_t stacklist_freefn_kernelAddr) {
   // Register usage is a real mess currently; thank goodness for that utility
   // class regSetManager.  Anyway, here's what we need.
   // 1) %o0 and %o1 explicitly used (I hope this is temporary)
   // 2) four 32-bit robust integer registers
   //    (save_link_reg, save_ccrs_reg, vtimerptrs_start_reg, vtimerptrs_iter_reg)
   // 3) two volatile 64-bit registers (vreg0 and vreg1)

   instr_t *i;

   const abi &theABI = em.getABI();

   regSetManager theRegSetManager(theFnRegInfo.getAvailRegsForFn(),
                                  true, // do a save, if needed
                                  2, // this many vreg64's
                                  0, // this many vreg32's
                                  5, // this many robust (non-v(olatile)) reg32's
                                  sparc_reg_set(sparc_reg::o0) + sparc_reg::o1,
                                  // these are the regs that we explicitly need
                                  theABI);

   if (theRegSetManager.needsSaveRestore()) {
      i = new sparc_instr(sparc_instr::save, -theABI.getMinFrameAlignedNumBytes());
      em.emit(i);
   }
   sparc_reg save_link_reg = theRegSetManager.getNthRobustReg32(0);
   // note that saving the link reg (%o7) isn't really needed when we're going
   // a save/restore, but what the heck.
   sparc_reg robust_saveccrs_reg = theRegSetManager.getNthRobustReg32(1);

   sparc_reg vtimerptrs_start_reg = theRegSetManager.getNthRobustReg32(2);
   sparc_reg vtimerptrs_iter_reg = theRegSetManager.getNthRobustReg32(3);
   
   // FIXME: aux_data_reg should be a 64-bit register! Does it matter in 
   // the context switch code ?
   sparc_reg aux_data_reg = theRegSetManager.getNthRobustReg32(4);
   
   // Pick vreg0 and vreg1, which must be 64-bit-safe
   sparc_reg vreg0 = theRegSetManager.getNthVReg64(0);
   sparc_reg vreg1 = theRegSetManager.getNthVReg64(1);

   // Ready to start emitting:

   // save %o7 and %ccr into robust registers
   i = new sparc_instr(sparc_instr::mov, sparc_reg::o7, save_link_reg);
   em.emit(i);
   i = new sparc_instr(sparc_instr::readstatereg, 0x2, robust_saveccrs_reg);
   em.emit(i);

   // assert that we are executing at least at LOCK_LEVEL ( = 10)
   // we do not need it anymore, but let's check for it
   if (!just_kerninstd) {
      extern machineInfo *theGlobalKerninstdMachineInfo;
      unsigned dispLevel = theGlobalKerninstdMachineInfo->getDispatcherLevel();

      i = new sparc_instr(sparc_instr::readprivreg, 0x8 /* PIL */, vreg0);
      em.emit(i);
      i = new sparc_instr(sparc_instr::cmp, vreg0, dispLevel);
      em.emit(i);
      i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                          false, // not extended
                          0x34);
      em.emit(i);
   }
   
   emit_global_lock(em, vreg0);

// Sorry, due to the old vs. new format of vtimers, this assert is no longer
// so simple, so we have to remove it, at least for now.
//     // First, a nice strong assert: ALL vtimers should be off at the moment
//     emit_assert_all_vtimers_are_stopped(em, allvtimers_kernelAddr,
//                                         vtimerptrs_iter_reg, // a reg32
//                                         vreg0 // a reg64
//                                         );
//        // Will overwrite %ccr but we've already saved it

   // TRACE-BUFFER: log a switch-in of the current thread (%g7)
   if (theTraceBuffer) {
      em.emitImm32(trace_op_begin, vreg0);
      theTraceBuffer->emit_appendx(em,
                                   just_kerninstd,
                                   vreg0, // reg containing op data
                                   // 3 scratch regs follow:
                                   vtimerptrs_start_reg,
                                   vtimerptrs_iter_reg,
                                   vreg1, // this one needs to be 64-bit-safe
                                   "cswitchin_begin_skiptrace");
   }
   
   // call hash_table_remove with 1 arg: the key (%g7).
   // The address of hash table remove isn't yet known, but that's OK; we can use
   // an unresolved symAddr
   em.emitSymAddr("hashTableRemoveAddr", vreg0);
   i = new sparc_instr(sparc_instr::callandlink, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   const fnRegInfo hashTableRemoveFnRegInfo(false, // no inlined
                                            fnRegInfo::regsAvailAtCallTo,
                                            theRegSetManager.getAdjustedParentSet() -
                                            save_link_reg - robust_saveccrs_reg,
                                            // other robust regs, like aux_data_reg,
                                            // aren't yet written and thus are free
                                            // for use.
                                            fnRegInfo::blankParamInfo +
                                            make_pair(pdstring("key"), sparc_reg::g7),
                                            vreg0 // result reg
      );

   // we'll restore %o7 in the delay slot below

   // result of call to hash_table_remove: returns the data of the removed
   // element if found (stacklist data item), or NULL (0) if not found.

   // If the result is 0 (not found) then return right away.
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       vreg0,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "restore_ccrs_and_return");
   i = new sparc_instr(sparc_instr::mov, save_link_reg, sparc_reg::o7);
   em.emit(i);
   // delay slot -- restore %o7

   // set vtimerptrs_start_reg (it's the result, vreg0)
   i = new sparc_instr(sparc_instr::mov, vreg0, vtimerptrs_start_reg);
   em.emit(i);

   // ...and while we're at it, also set vtimerptrs_iter_reg with that same value
   i = new sparc_instr(sparc_instr::mov, vtimerptrs_start_reg, 
		       vtimerptrs_iter_reg);
   em.emit(i);

   const tempBufferEmitter::insnOffset loopstartoffset = em.currInsnOffset();
   
   // obtain the vtimerptr: load vtimerptrs_iter_reg into %o0
   em.emitLoadKernelNative(vtimerptrs_iter_reg, saved_timer_address_offset,
			   sparc_reg::o0);
   
   // If the vtimerptr is NULL, then iteration is done
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       sparc_reg::o0,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "after_loop");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // load the aux-data field: 64 bits even on the 32-bit platform !
   i = new sparc_instr(sparc_instr::load, sparc_instr::ldExtendedWord,
                       vtimerptrs_iter_reg, saved_timer_auxdata_offset, 
		       aux_data_reg);
   em.emit(i);

   // load the "restart function" field
   em.emitLoadKernelNative(sparc_reg::o0, restart_routine_offset,
			   vreg0);

   // How-to-restart routine:
   const fnRegInfo howToRestartFnRegInfo(false, // not inlined,
                                         fnRegInfo::regsAvailAtCallTo,
                                         theRegSetManager.getAvailRegsForACallee() +
                                         sparc_reg::reg_xcc + sparc_reg::reg_icc,
                                         fnRegInfo::blankParamInfo +
                                         make_pair(pdstring("vtimer_addr_reg"),
                                                   sparc_reg::o0) +
                                         make_pair(pdstring("vtimer_auxdata_reg"),
                                                   aux_data_reg),
                                         sparc_reg::g0);

   i = new sparc_instr(sparc_instr::callandlink, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // restore the link reg after the call
   i = new sparc_instr(sparc_instr::mov, save_link_reg, sparc_reg::o7);
   em.emit(i);

   // jump back to the start of the loop, advancing the iterator in delay slot
   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
                       false, // NOT annulled
                       true, // predict taken
                       true, // xcc (matters?)
                       -(em.currInsnOffset() - loopstartoffset));
   em.emit(i);
   i = new sparc_instr(sparc_instr::add, vtimerptrs_iter_reg, 
		       sizeof_saved_timer, vtimerptrs_iter_reg);
   em.emit(i);
      // delay slot

   em.emitLabel("after_loop");

   // return vtimerptrs_start_reg to free list.
   em.emitImmAddr(stacklist_freefn_kernelAddr, vreg0);
   em.emitImmAddr(stacklist_kernelAddr, sparc_reg::o0);
   i = new sparc_instr(sparc_instr::callandlink, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::mov, vtimerptrs_start_reg, sparc_reg::o1);
   em.emit(i);
   // delay slot -- set the second argument

   // restore o7 after the call
   i = new sparc_instr(sparc_instr::mov, save_link_reg, sparc_reg::o7);
   em.emit(i);

   em.emitLabel("restore_ccrs_and_return");

   // TRACE-BUFFER: log a switch-in (operation code 2) of the current thread (%g7)
   if (theTraceBuffer) {
      em.emitImm32(trace_op_finish, vreg0);
      theTraceBuffer->emit_appendx(em,
                                   just_kerninstd,
                                   vreg0, // reg containing op data
                                   // 3 scratch regs follow:
                                   vtimerptrs_start_reg,
                                   vtimerptrs_iter_reg,
                                   vreg1, // needs to be 64-bit safe
                                   "cswitchin_finish_skiptrace");
   }

   i = new sparc_instr(sparc_instr::writestatereg, robust_saveccrs_reg, 0x2);
   em.emit(i);

   emit_global_unlock(em, vreg0);

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

   return make_pair(hashTableRemoveFnRegInfo, howToRestartFnRegInfo);
}
