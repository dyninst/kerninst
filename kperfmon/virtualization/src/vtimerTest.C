// vtimerTest.C
// I couldn't stand the size of
// vtimerMgr::test_virtualization_full_ensemble cluttering up vtimerMgr.C, so
// I moved it into this file.  Same with some other test methods

#include "abi.h"
#include "vtimerMgr.h"
#include "instrumenter.h"
#include "EmitPic0.h"
#include "all_vtimers.h"
#include "stacklist.h"
#include "hash_table.h"

#include "countedVEvents.h" // simple metric

#include "all_vtimers_emit.h"
#include "stacklist_emit.h"
#include "hash_table_emit.h"
#include "cswitchout_emit.h"
#include "cswitchin_emit.h"

#include "dataHeap.h"
//#include "vtimerPrimitive.h" old & deprecated
#include "vtimerCountedPrimitive.h"

#include "vtimerSupport.h"

extern instrumenter *theGlobalInstrumenter;
extern dataHeap *theGlobalDataHeap;


static void switchout(dptr_t cswitchout_kerninstdAddr, uinthash_t thr_id) {
   tempBufferEmitter em(theGlobalInstrumenter->getKerninstdABI());
   em.emitImmAddr(thr_id, sparc_reg::g7); // set curr thread
   em.emitImmAddr(cswitchout_kerninstdAddr, sparc_reg::o0);
   em.emit(sparc_instr(sparc_instr::jump, sparc_reg::o0)); // tail call
   em.emit(sparc_instr(sparc_instr::nop));
   em.emit(sparc_instr(sparc_instr::trap, 0x34)); // should not return here
   em.complete();

   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);
}

static void switchin(dptr_t cswitchin_kerninstdAddr, uinthash_t thr_id) {
   tempBufferEmitter em(theGlobalInstrumenter->getKerninstdABI());
   em.emitImmAddr(thr_id, sparc_reg::g7); // set curr thread
   em.emitImmAddr(cswitchin_kerninstdAddr, sparc_reg::o0);
   em.emit(sparc_instr(sparc_instr::jump, sparc_reg::o0)); // tail call
   em.emit(sparc_instr(sparc_instr::nop));
   em.emit(sparc_instr(sparc_instr::trap, 0x34)); // should not return here
   em.complete();

   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);
}

new_vtimer *getNewVTimerV(dptr_t vtimerAddrInKerninstd) {
   // the final "V" in the name stands for "volatile", since we (yikes) return
   // a pointer to static memory.  That means just 1 call to this routine at
   // a time, please, and use the results before the next call!  Not thread-safe!
   static new_vtimer theNewVTimer;

   pdvector<uint32_t> timer_raw_data = theGlobalInstrumenter->peek_kerninstd_contig(vtimerAddrInKerninstd, sizeof_vtimer);
   new_vtimer *theNewVTimerPtr = (new_vtimer*)timer_raw_data.begin();
   theNewVTimer.total_field = theNewVTimerPtr->total_field; 
   theNewVTimer.start_field.rcount = theNewVTimerPtr->start_field.rcount;
   theNewVTimer.start_field.start = theNewVTimerPtr->start_field.start;
   theNewVTimer.stop_routine = theNewVTimerPtr->stop_routine;
   theNewVTimer.restart_routine = theNewVTimerPtr->restart_routine;
   
   return &theNewVTimer;
}

static void assertNewVTimerAll(dptr_t vtimerAddrInKerninstd,
                               uint64_t total,
                               unsigned rcount,
                               uint64_t start,
                               kptr_t stop_routine,
                               kptr_t restart_routine) {
   pdvector<uint32_t> timer_raw_data = theGlobalInstrumenter->peek_kerninstd_contig(vtimerAddrInKerninstd, sizeof_vtimer);
   new_vtimer *theVTimerPtr = (new_vtimer*)timer_raw_data.begin();

   assert(theVTimerPtr->total_field == total);
   assert(theVTimerPtr->start_field.rcount == rcount);
   assert(theVTimerPtr->start_field.start == start);
   assert(theVTimerPtr->stop_routine == stop_routine);
   assert(theVTimerPtr->restart_routine == restart_routine);
}

static void assertNewVTimerStarted(dptr_t vtimerAddrInKerninstd,
                                   uint64_t total,
                                   unsigned rcount,
                                   kptr_t stop_routine,
                                   kptr_t restart_routine) {
   pdvector<dptr_t> timer_raw_data = theGlobalInstrumenter->peek_kerninstd_contig(vtimerAddrInKerninstd, sizeof_vtimer);
   new_vtimer *theVTimerPtr = (new_vtimer*)timer_raw_data.begin();

   assert(theVTimerPtr->total_field == total);
   assert(theVTimerPtr->start_field.rcount == rcount);
   assert(theVTimerPtr->start_field.start != 0);
   assert(theVTimerPtr->stop_routine == stop_routine);
   assert(theVTimerPtr->restart_routine == restart_routine);
}

static void assertNewVTimerStopped(dptr_t vtimerAddrInKerninstd,
                                   kptr_t stop_routine,
                                   kptr_t restart_routine) {
   pdvector<uint32_t> timer_raw_data = theGlobalInstrumenter->peek_kerninstd_contig(vtimerAddrInKerninstd, sizeof_vtimer);
   new_vtimer *theVTimerPtr = (new_vtimer*)timer_raw_data.begin();

   assert(theVTimerPtr->total_field > 0);
   assert(theVTimerPtr->start_field.rcount == 0);
   
   // We'd like to assert that the start field is nonzero, but present how-to-stop
   // does indeed set it to zero (which is correct, since it's undefined, so any
   // value is correct.)
//assert(theVTimerPtr->start_field.start != 0);
//      // actually, it's undefined, but this should be safe

   assert(theVTimerPtr->stop_routine == stop_routine);
   assert(theVTimerPtr->restart_routine == restart_routine);
}

//  static void assertOldVTimer(uint32_t vtimerAddrInKerninstd,
//                              short state,
//                              uint64_t total,
//                              uint64_t start,
//                              uint32_t stop_routine,
//                              uint32_t restart_routine) {
//     pdvector<unsigned> timer_raw_data = theGlobalInstrumenter->peek_kerninstd(vtimerAddrInKerninstd, sizeof_vtimer);
//     old_vtimer *theVTimerPtr = (old_vtimer*)timer_raw_data.begin();

//     assert(theVTimerPtr->total_field.state == state);
//     assert(theVTimerPtr->total_field.total == total);
//     assert(theVTimerPtr->start_field == start);
//     assert((uint32_t)theVTimerPtr->stop_routine == stop_routine);
//     assert((uint32_t)theVTimerPtr->restart_routine == restart_routine);
//  }




void vtimerMgr::
test_virtualization_full_ensemble(const sparc_reg_set &freeRegsAtSwitchOut,
                                  const sparc_reg_set &freeRegsAtSwitchIn) {
   // --------------------------------------------------------------------------------
   // ---------------- Install all_vtimers part of the Ensemble ----------------------
   // ----------------- (stolen from test_all_vtimers_stuff()) -----------------------
   // --------------------------------------------------------------------------------

   pair<kptr_t, dptr_t> all_vtimers_addrs = 
      theGlobalInstrumenter->allocateMappedKernelMemory(sizeof_allvtimers,
                                                        false);
      // false --> don't try to allocate w/in kernel's nucleus text
   const dptr_t all_vtimers_kerninstdAddr = all_vtimers_addrs.second;

   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   
   // initialize
   tempBufferEmitter init_vtimers_em(kerninstdABI);
   emit_all_vtimers_initialize(init_vtimers_em, all_vtimers_kerninstdAddr);
   init_vtimers_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_vtimers_em);
   
   // download add and remove routines
   tempBufferEmitter add_vtimers_em(kerninstdABI);
   emit_all_vtimers_add(add_vtimers_em);
   add_vtimers_em.complete();
   const unsigned all_vtimers_add_reqid =
      theGlobalInstrumenter->downloadToKerninstd(add_vtimers_em);
   pair<pdvector<dptr_t>,unsigned> theAddrs = theGlobalInstrumenter->
      queryDownloadedToKerninstdAddresses(all_vtimers_add_reqid);
   const dptr_t all_vtimers_add_kerninstdAddr = theAddrs.first[theAddrs.second];

//     tempBufferEmitter remove_vtimers_em(kerninstdABI);
//     emit_all_vtimers_remove(remove_vtimers_em);
//     remove_vtimers_em.complete();
//     const unsigned all_vtimers_remove_reqid =
//        theGlobalInstrumenter->downloadToKerninstd(remove_vtimers_em);
//     const uint32_t all_vtimers_remove_kerninstdAddr = theGlobalInstrumenter->
//        queryDownloadedToKerninstdAddresses(all_vtimers_remove_reqid);
   
   

   // --------------------------------------------------------------------------------
   // ---------------- Install StackList part of the Ensemble ------------------------
   // ----------------- (stolen from test_stacklist_stuff()) -------------------------
   // --------------------------------------------------------------------------------

   // Allocate & initialize the stacklist:
   pair<kptr_t, dptr_t> stacklist_addrs =
      theGlobalInstrumenter->allocateMappedKernelMemory(stacklist_nbytes,
                                                        false);
      // false --> don't try to allocate w/in kernel's nucleus text
   const dptr_t stacklist_kerninstdAddr = stacklist_addrs.second;
   cout << "allocated stacklist @ kerninstd " << addr2hex(stacklist_kerninstdAddr)
        << endl;
   
   tempBufferEmitter init_stacklist_em(kerninstdABI);
   emit_stacklist_initialize(init_stacklist_em,
                             stacklist_kerninstdAddr,
                             stacklist_kerninstdAddr);
   init_stacklist_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_stacklist_em);

   // download stacklist_get() and stacklist_free into kerninstd:
   tempBufferEmitter get_stacklist_em(kerninstdABI);
   emit_stacklist_get(get_stacklist_em);
   get_stacklist_em.complete();
   const unsigned downloaded_stacklist_get_reqid =
      theGlobalInstrumenter->downloadToKerninstd(get_stacklist_em);
   theAddrs = theGlobalInstrumenter->
      queryDownloadedToKerninstdAddresses(downloaded_stacklist_get_reqid);
   const dptr_t stacklist_get_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "stacklist test: stacklist_get downloaded to kerninstd "
        << addr2hex(stacklist_get_kerninstdAddr) << endl;
   
   tempBufferEmitter free_stacklist_em(kerninstdABI);
   emit_stacklist_free(free_stacklist_em);
   free_stacklist_em.complete();
   const unsigned downloaded_stacklist_free_reqid =
      theGlobalInstrumenter->downloadToKerninstd(free_stacklist_em);
   theAddrs = theGlobalInstrumenter->
      queryDownloadedToKerninstdAddresses(downloaded_stacklist_free_reqid);
   const dptr_t stacklist_free_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "stacklist test: stacklist_free downloaded to kerninstd "
        << addr2hex(stacklist_free_kerninstdAddr) << endl;

   // --------------------------------------------------------------------------------
   // ---------------- Install Hash Table part of the Ensemble -----------------------
   // ----------------- (stolen from test_hashtable_stuff()) -------------------------
   // --------------------------------------------------------------------------------
   // hash table: allocate & initialize to empty
   pair<kptr_t, dptr_t> hashTableAddrs =
      theGlobalInstrumenter->allocateMappedKernelMemory(hash_table_nbytes, false);
      // false --> don't try to allocate w/in kernel's nucleus text
   const dptr_t hashtable_kerninstdAddr = hashTableAddrs.second;
   cout << "hash table allocated at " << addr2hex(hashtable_kerninstdAddr) << endl;

   tempBufferEmitter init_hashtable_em(kerninstdABI);
   emit_initialize_hashtable(init_hashtable_em, hashtable_kerninstdAddr);
   init_hashtable_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_hashtable_em);
   
   // don't assert that the hash table got properly initialized...the hash table
   // test does that.  Here, we assume the hash table stuff is pretty much bug-free.

   // ----------------------------------------------------------------------
   // cswitchout and cswitchin routines.  They'll need to make calls to
   // the hash table add and hash table remove routines, which haven't
   // yet been emitted, but that's OK, we can resolve a symAddr later...
   // ----------------------------------------------------------------------

   // ----------------------------------------------------------------------
   // cswitchout and its helper routines: hash table add/pack:
   // ----------------------------------------------------------------------
   tempBufferEmitter cswitchout_em(kerninstdABI);
   const pair<fnRegInfo, fnRegInfo> cswitchOutHelperRoutineFnRegInfos =
      // how-to-stop and add fnRegInfo's, respectively
      emit_cswitchout_instrumentation(cswitchout_em,
                                      true, // in kerninstd
                                      fnRegInfo(false, // not inlined
                                                fnRegInfo::regsAvailAtCallTo,
                                                freeRegsAtSwitchOut,
                                                fnRegInfo::blankParamInfo,
                                                // no params
                                                sparc_reg::g0
                                                // no return value
                                                ),
                                      NULL, // no trace buffer
                                      1, // trace op for begin
                                      2, // trace op for finish
                                      all_vtimers_kerninstdAddr,
                                      stacklist_kerninstdAddr,
                                      stacklist_get_kerninstdAddr,
                                      stacklist_free_kerninstdAddr);

   // hash_table_add() and hash_table_pack():

   tempBufferEmitter add_hash_em(kerninstdABI);
   const fnRegInfo fnRegInfoForHashTablePack =
      emit_hashtable_add(add_hash_em,
                         cswitchOutHelperRoutineFnRegInfos.second,
                         hashtable_kerninstdAddr);
   
   tempBufferEmitter pack_hash_em(kerninstdABI);
   emit_hashtable_pack(pack_hash_em,
                       hashtable_kerninstdAddr,
                       fnRegInfoForHashTablePack);
   pack_hash_em.complete();
   const unsigned hashtable_pack_reqid =
      theGlobalInstrumenter->downloadToKerninstd(pack_hash_em);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(hashtable_pack_reqid);
   const dptr_t hashtable_pack_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "hashtable_pack() installed at kerninstd "
        << addr2hex(hashtable_pack_kerninstdAddr) << endl;

   // Pack has been emitted, can now finish up add:
   add_hash_em.makeSymAddrKnown("hashTablePackAddr", 
				hashtable_pack_kerninstdAddr);
   add_hash_em.complete();

   const unsigned hashtable_add_reqid =
      theGlobalInstrumenter->downloadToKerninstd(add_hash_em);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(hashtable_add_reqid);
   const dptr_t hashtable_add_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "hashtable_add() installed at kerninstd "
        << addr2hex(hashtable_add_kerninstdAddr) << endl;

   // Add has been emitted, can now finish up cswitchout:
   cswitchout_em.makeSymAddrKnown("hashTableAddAddr", hashtable_add_kerninstdAddr);
   cswitchout_em.complete();
   const unsigned cswitchout_reqid =
      theGlobalInstrumenter->downloadToKerninstd(cswitchout_em);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(cswitchout_reqid);
   const dptr_t cswitchout_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "cswitchout code installed at kerninstd "
        << addr2hex(cswitchout_kerninstdAddr) << endl;
   
   // ----------------------------------------------------------------------
   // cswitchin and its helper routine: hash table remove:
   // ----------------------------------------------------------------------
   tempBufferEmitter cswitch_in_em(kerninstdABI);
   const pair<fnRegInfo, fnRegInfo> cswitchinHelperRoutineFnRegInfos =
      // for hash table remove, how-to-restart, respectively
   emit_cswitchin_instrumentation(cswitch_in_em,
                                  true, // just kerninstd
                                  fnRegInfo(false, // not inlined,
                                            fnRegInfo::regsAvailAtCallTo,
                                            freeRegsAtSwitchIn,
                                            fnRegInfo::blankParamInfo,
                                            // no params
                                            sparc_reg::g0
                                            // no return value
                                            ),
                                  NULL, //&theTraceBuffer,
                                  3, // trace op code for begin
                                  4, // trace op code for finish
                                  stacklist_kerninstdAddr,
                                  stacklist_free_kerninstdAddr);

   // hash_table_remove():
   tempBufferEmitter remove_hash_em(kerninstdABI);
   emit_hashtable_remove(remove_hash_em,
                         hashtable_kerninstdAddr,
                         cswitchinHelperRoutineFnRegInfos.first);
   remove_hash_em.complete();
   const unsigned hashtable_remove_reqid =
      theGlobalInstrumenter->downloadToKerninstd(remove_hash_em);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(hashtable_remove_reqid);
   const dptr_t hashtable_remove_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "hashtable_remove() installed at kerninstd "
        << addr2hex(hashtable_remove_kerninstdAddr) << endl;

   // now that remove() has been emitted, we're about ready to complete cswitchin:
   cswitch_in_em.makeSymAddrKnown("hashTableRemoveAddr",
				  hashtable_remove_kerninstdAddr);
   cswitch_in_em.complete();
   const unsigned cswitchin_reqid =
      theGlobalInstrumenter->downloadToKerninstd(cswitch_in_em);
   theAddrs = theGlobalInstrumenter->
      queryDownloadedToKerninstdAddresses(cswitchin_reqid);
   const dptr_t cswitchin_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "cswitchin code installed at kerninstd "
        << addr2hex(cswitchin_kerninstdAddr) << endl;
   
   // --------------------------------------------------------------------------------
   // ------------------- Stopping & Restarting Pic0CountedVTimers -------------------
   // --------------------------------------------------------------------------------

   // Obtain vevents
   const countedVCycles theCountedVCyclesSimpleMetric(0); // simple metric id

   const countedVEvents *theCountedVEventsMetric = &theCountedVCyclesSimpleMetric;
   assert(theCountedVEventsMetric);
   
   // new vtimer how-to-stop:
   tempBufferEmitter stop_vmetric_em(kerninstdABI);
   // const fnRegInfo &howToStopFnRegInfo = cswitchOutHelperRoutineFnRegInfos.first;
#if 0
   theCountedVEventsMetric->emitMetricSpecificVStopIfNeeded
      (stop_vmetric_em,
       howToStopFnRegInfo.getParamRegByName("vtimer_addr_reg"),
       howToStopFnRegInfo.getParamRegByName("vtimer_start_reg"),
       howToStopFnRegInfo.getParamRegByName("vtimer_iter_reg"),
       howToStopFnRegInfo.getAvailRegsForFn());
#endif
   stop_vmetric_em.complete();
   const unsigned stop_vmetric_reqid =
      theGlobalInstrumenter->downloadToKerninstd(stop_vmetric_em);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(stop_vmetric_reqid);
   const dptr_t stop_vmetric_kerninstdAddr = theAddrs.first[theAddrs.second];

   // new vtimer how-to-restart:
   tempBufferEmitter restart_vmetric_em(kerninstdABI);
   // const fnRegInfo &howToRestartFnRegInfo = cswitchinHelperRoutineFnRegInfos.second;
#if 0
   theCountedVEventsMetric->emitMetricSpecificVRestart
      (restart_vmetric_em,
       howToRestartFnRegInfo.getParamRegByName("vtimer_addr_reg"),
       howToRestartFnRegInfo.getParamRegByName("vtimer_auxdata_reg"),
       howToRestartFnRegInfo.getAvailRegsForFn());
#endif
   restart_vmetric_em.complete();
   const unsigned restart_vmetric_reqid =
      theGlobalInstrumenter->downloadToKerninstd(restart_vmetric_em);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(restart_vmetric_reqid);
   const dptr_t restart_vmetric_kerninstdAddr = theAddrs.first[theAddrs.second];
   
   cout << "how-to-stop and restart vmetric installed at kerninstd "
        << addr2hex(stop_vmetric_kerninstdAddr) << " and "
        << addr2hex(restart_vmetric_kerninstdAddr) << endl;
   
   // --------------------------------------------------------------------------------
   // --------------------- Allocate & initialize some vtimers -----------------------
   // --------------------------------------------------------------------------------

   // Allocate & initialize new vtimers & add to all_vtimers[]:
   const unsigned num_newvtimers_in_test = 10; // this many needed for test5, at least
   kptr_t newvtimer_addrs_in_kernel[num_newvtimers_in_test];
   dptr_t newvtimer_addrs_in_kerninstd[num_newvtimers_in_test];
   try {
      for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
         newvtimer_addrs_in_kernel[lcv] = theGlobalDataHeap->allocVT();
         newvtimer_addrs_in_kerninstd[lcv] = 
            theGlobalDataHeap->kernelAddr2KerninstdAddrVT(newvtimer_addrs_in_kernel[lcv]);
         cout << "allocated newvtimer" << lcv << " at kerninstd "
              << addr2hex(newvtimer_addrs_in_kerninstd[lcv]) << endl;
         assert(newvtimer_addrs_in_kerninstd[lcv] % 8 == 0);

         tempBufferEmitter init_vtimer_em(kerninstdABI);
#if 0
         countedVEvents::emit_initialization(init_vtimer_em,
                                             theGlobalInstrumenter->getAvailRegsForDownloadedToKerninstdCode(false),
                                                // false --> don't ignore the client-id param
					     theGlobalDataHeap->getElemsPerVectorVT(),
					     theGlobalDataHeap->getBytesPerStrideVT(),
                                             newvtimer_addrs_in_kerninstd[lcv],
                                             stop_vmetric_kerninstdAddr,
                                             restart_vmetric_kerninstdAddr);
#endif
         init_vtimer_em.complete();
         theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_vtimer_em);

         assertNewVTimerAll(newvtimer_addrs_in_kerninstd[lcv], 0, 0, 0,
                            stop_vmetric_kerninstdAddr,
                            restart_vmetric_kerninstdAddr);

         tempBufferEmitter addToAllVTimers_em(kerninstdABI);
         emit_addNewEntryToAllVTimers(addToAllVTimers_em,
                                      newvtimer_addrs_in_kerninstd[lcv],
                                      all_vtimers_kerninstdAddr,
                                      all_vtimers_add_kerninstdAddr);
         addToAllVTimers_em.complete();
         theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(addToAllVTimers_em);
      }
   }
   catch (...) {
      assert(false);
   }

   // --------------------------------------------------------------------------------
   // --------------------- start & stop pic0vtimer (old & new) ----------------------
   // --------------------------------------------------------------------------------

   const sparc_reg_set freeRegsForPrimitives = sparc_reg_set::allLs +
      sparc_reg_set::allOs - sparc_reg::o7 - sparc_reg::sp;

   // new start primitives:
   pdvector<primitive*> theNewStartPrimitives(num_newvtimers_in_test);
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      theNewStartPrimitives[lcv] = new vtimerCountedStartPrimitive(
	  newvtimer_addrs_in_kerninstd[lcv],
	  theCountedVEventsMetric->getRawCtrExpr());
   }

   unsigned newstartprimitive_reqids[num_newvtimers_in_test];
   dptr_t newstartprimitive_kerninstdAddrs[num_newvtimers_in_test];
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      tempBufferEmitter start_primitive_em(kerninstdABI);
      // theNewStartPrimitives[lcv]->generateCode(start_primitive_em,
      //                                          freeRegsForPrimitives);
      start_primitive_em.emit(sparc_instr(sparc_instr::retl));
      start_primitive_em.emit(sparc_instr(sparc_instr::nop));
      start_primitive_em.complete();

      unsigned reqid = theGlobalInstrumenter->downloadToKerninstd(start_primitive_em);
      newstartprimitive_reqids[lcv] = reqid;
      theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(reqid);
      newstartprimitive_kerninstdAddrs[lcv] = theAddrs.first[theAddrs.second];
   }

   // new stop primitives:

   pdvector<primitive*> theNewStopPrimitives(num_newvtimers_in_test);
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      theNewStopPrimitives[lcv] = new vtimerCountedStopPrimitive(
	  newvtimer_addrs_in_kerninstd[lcv],
	  theCountedVEventsMetric->getRawCtrExpr());
   }
   
   unsigned newstopprimitive_reqids[num_newvtimers_in_test];
   dptr_t newstopprimitive_kerninstdAddrs[num_newvtimers_in_test];
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      tempBufferEmitter stop_primitive_em(kerninstdABI);
      // theNewStopPrimitives[lcv]->generateCode(stop_primitive_em,
      //                                         freeRegsForPrimitives);
      stop_primitive_em.emit(sparc_instr(sparc_instr::retl));
      stop_primitive_em.emit(sparc_instr(sparc_instr::nop));
      stop_primitive_em.complete();

      unsigned reqid = theGlobalInstrumenter->downloadToKerninstd(stop_primitive_em);
      newstopprimitive_reqids[lcv] = reqid;
      theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(reqid);
      newstopprimitive_kerninstdAddrs[lcv] = theAddrs.first[theAddrs.second];
   }

   // --------------------------------------------------------------------------------
   // ------------------------ Now do the actual tests -------------------------------
   // --------------------------------------------------------------------------------
   
   // Test 1: switch out thread 10, switch in thread 11, switch out thread 11,
   // switch in thread 10.  We do all of this by setting %g7 at strategic times.
   // This should be permissible in user code.
   // No vtimers are active, so nothing much should happen.

   // assert that the timer state is as we expect.
   assertNewVTimerAll(newvtimer_addrs_in_kerninstd[0], 0, 0, 0,
                      stop_vmetric_kerninstdAddr,
                      restart_vmetric_kerninstdAddr);

   switchout(cswitchout_kerninstdAddr, 10);

   assertNewVTimerAll(newvtimer_addrs_in_kerninstd[0], 0, 0, 0,
                      stop_vmetric_kerninstdAddr,
                      restart_vmetric_kerninstdAddr);

   switchin(cswitchin_kerninstdAddr, 11);

   assertNewVTimerAll(newvtimer_addrs_in_kerninstd[0], 0, 0, 0,
                      stop_vmetric_kerninstdAddr,
                      restart_vmetric_kerninstdAddr);

   switchout(cswitchout_kerninstdAddr, 11);

   assertNewVTimerAll(newvtimer_addrs_in_kerninstd[0], 0, 0, 0,
                      stop_vmetric_kerninstdAddr,
                      restart_vmetric_kerninstdAddr);
   
   switchin(cswitchin_kerninstdAddr, 10);

   assertNewVTimerAll(newvtimer_addrs_in_kerninstd[0], 0, 0, 0,
                      stop_vmetric_kerninstdAddr,
                      restart_vmetric_kerninstdAddr);

   // Test 2: similar to test 1, except we (fully) start and (fully) stop a vtimer
   // (without preemption) somewhere along the way.
   
   tempBufferEmitter *newvtimers_start_em[num_newvtimers_in_test];
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      newvtimers_start_em[lcv] = new tempBufferEmitter(kerninstdABI);
      tempBufferEmitter &em = *newvtimers_start_em[lcv];
      em.emitImmAddr(newstartprimitive_kerninstdAddrs[lcv], sparc_reg::o0);
      em.emit(sparc_instr(sparc_instr::jump, sparc_reg::o0)); // tail call
      em.emit(sparc_instr(sparc_instr::nop));
      em.emit(sparc_instr(sparc_instr::trap, 0x34)); // shouldn't return
      em.complete();
   }
   
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_start_em[0]);

   assertNewVTimerStarted(newvtimer_addrs_in_kerninstd[0],
                          0, 1, // total, rcount
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);

   tempBufferEmitter *newvtimers_stop_em[num_newvtimers_in_test];
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      newvtimers_stop_em[lcv] = new tempBufferEmitter(kerninstdABI);
      tempBufferEmitter &em = *newvtimers_stop_em[lcv];
      em.emitImmAddr(newstopprimitive_kerninstdAddrs[lcv], sparc_reg::o0);
      em.emit(sparc_instr(sparc_instr::jump, sparc_reg::o0)); // tail call
      em.emit(sparc_instr(sparc_instr::nop));
      em.emit(sparc_instr(sparc_instr::trap, 0x34)); // shouldn't return
      em.complete();
   }
   
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_stop_em[0]);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);

   switchout(cswitchout_kerninstdAddr, 10);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);

   switchin(cswitchin_kerninstdAddr, 11);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);

   switchout(cswitchout_kerninstdAddr, 11);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);

   switchin(cswitchin_kerninstdAddr, 10);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);
   new_vtimer *theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
   const uint64_t test2_total_atend = theNewVTimerPtr->total_field;

   // Test 3: Still working with just a single vtimer, but this time we'll give the
   // cswitch code something to do: start vtimer, switchout 10 [should stop the
   // vtimer], switchin 11 [should do nothing], switchout 11 [should do nothing],
   // and switchin 10 [should re-start the vtimer], stop vtimer.

   // test 3 -- start vtimer
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_start_em[0]);

   // test 3 -- verify that start vtimer worked OK
   assertNewVTimerStarted(newvtimer_addrs_in_kerninstd[0],
                          test2_total_atend, 1, // total, rcount
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);

   // test 3 -- switchout cur thread (id 10): this should stop the vtimer (adding
   // something to the total field and changing the state to 0x0)
   switchout(cswitchout_kerninstdAddr, 10);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);
   theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
   assert(theNewVTimerPtr->total_field > test2_total_atend);
   const uint64_t test3_total_after_switchout10 = theNewVTimerPtr->total_field;
   // const uint64_t test3_start_after_switchout10 = theNewVTimerPtr->start_field.start;

   // test3 -- switchin thread 11: this should do nothing
   switchin(cswitchin_kerninstdAddr, 11);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);
   theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
   assert(theNewVTimerPtr->total_field == test3_total_after_switchout10);

   // test 3 -- switch out thread 11 (should do nothing)
   switchout(cswitchout_kerninstdAddr, 11);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);
   theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
   assert(theNewVTimerPtr->total_field == test3_total_after_switchout10);

   // test 3 -- resume thread 10 (this should re-start the timer, updating start but
   // not total)
   switchin(cswitchin_kerninstdAddr, 10);

   assertNewVTimerStarted(newvtimer_addrs_in_kerninstd[0],
                          test3_total_after_switchout10, 1, // total, rcount
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);

   // test 3 -- stop the vtimer
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_stop_em[0]);

   assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[0],
                          stop_vmetric_kerninstdAddr,
                          restart_vmetric_kerninstdAddr);
   theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
   assert(theNewVTimerPtr->total_field > test3_total_after_switchout10);

   // Test 4: Multiple vtimers, but otherwise the same as test 3, in that those
   // vtimers are each started & stopped by the same kthread.  Start by re-initializing
   // the first vtimer.
   tempBufferEmitter init_vtimer_em(kerninstdABI);
#if 0
   countedVEvents::emit_initialization(init_vtimer_em,
                                       theGlobalInstrumenter->getAvailRegsForDownloadedToKerninstdCode(false),
                                          // false --> don't overwrite the client-id param
				       theGlobalDataHeap->getElemsPerVectorVT(),
				       theGlobalDataHeap->getBytesPerStrideVT(),
                                       newvtimer_addrs_in_kerninstd[0],
                                       stop_vmetric_kerninstdAddr,
                                       restart_vmetric_kerninstdAddr);
#endif
   init_vtimer_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_vtimer_em);

   // test 4 -- start a bunch of vtimers
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv)
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_start_em[lcv]);

   // test 4 -- verify that the start vtimers each worked OK
   uint64_t test4_last_seen_start = 0;
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      assertNewVTimerStarted(newvtimer_addrs_in_kerninstd[lcv],
                             0, 1, // total, rcount
                             stop_vmetric_kerninstdAddr,
                             restart_vmetric_kerninstdAddr);
      theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[lcv]);
      assert(theNewVTimerPtr->start_field.start > test4_last_seen_start);
      test4_last_seen_start = theNewVTimerPtr->start_field.start;
   }

   // test 4 -- switchout kthread 10 (should stop a bunch of vtimers, adding something
   // to the total fields)
   switchout(cswitchout_kerninstdAddr, 10);

   uint64_t total_fields_test4_after_switchout10[num_newvtimers_in_test];
   uint64_t start_fields_test4_after_switchout10[num_newvtimers_in_test];
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[lcv],
                             stop_vmetric_kerninstdAddr,
                             restart_vmetric_kerninstdAddr);
      theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[lcv]);

      total_fields_test4_after_switchout10[lcv] = theNewVTimerPtr->total_field;
      start_fields_test4_after_switchout10[lcv] = theNewVTimerPtr->start_field.start;
   }

   // test 4 -- switchin kthread 11 (should do nothing interesting)
   switchin(cswitchin_kerninstdAddr, 11);

   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[lcv],
                             stop_vmetric_kerninstdAddr,
                             restart_vmetric_kerninstdAddr);

      theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[lcv]);
      assert(theNewVTimerPtr->total_field == total_fields_test4_after_switchout10[lcv]);
      assert(theNewVTimerPtr->start_field.start == start_fields_test4_after_switchout10[lcv]);
   }

   // test 4 -- resume kthread 10 (should re-start a bunch of vtimers, updating start
   // but not adding to total)
   switchin(cswitchin_kerninstdAddr, 10);
   
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      assertNewVTimerStarted(newvtimer_addrs_in_kerninstd[lcv],
                             total_fields_test4_after_switchout10[lcv],
                             1, // rcount
                             stop_vmetric_kerninstdAddr,
                             restart_vmetric_kerninstdAddr);
      
      theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[lcv]);
      assert(theNewVTimerPtr->start_field.start > start_fields_test4_after_switchout10[lcv]);
   }

   // test 4 -- manually stop the vtimers
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv)
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_stop_em[lcv]);

   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[lcv],
                             stop_vmetric_kerninstdAddr,
                             restart_vmetric_kerninstdAddr);
      theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[lcv]);
      assert(theNewVTimerPtr->total_field > total_fields_test4_after_switchout10[lcv]);
   }
   
   // Test 5 -- multiple vtimers, started by differing threads.
   // The pattern will be:
   // [kthread 10+n] starts vtimer n
   // That thread then gets switched out, and thread+1 gets switched in
   // We cycle through ten threads this way (so that each vtimer is started once).
   // Then a new pattern: we cycle through the threads in the same way, resuming
   // kthread 10+n (this should re-start the vtimer) and then switching that kthread
   // out in favor of kthread 10+n+1 (this should re-stop the vtimer).
   // Finally we cycle again: switching in kthread
   
   // test 5 -- start by reinitializing all of the vtimers
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      tempBufferEmitter init_vtimer_em(kerninstdABI);
#if 0
      countedVEvents::emit_initialization(init_vtimer_em,
                                          theGlobalInstrumenter->getAvailRegsForDownloadedToKerninstdCode(false),
                                             // false --> don't ignore the client-id param
					  theGlobalDataHeap->getElemsPerVectorVT(),
					  theGlobalDataHeap->getBytesPerStrideVT(),
                                          newvtimer_addrs_in_kerninstd[lcv],
                                          stop_vmetric_kerninstdAddr,
                                          restart_vmetric_kerninstdAddr);
#endif
      init_vtimer_em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_vtimer_em);
   }

   const unsigned num_threads_in_test5 = 10;
   for (uinthash_t threadid=10; threadid < 10+num_threads_in_test5; ++threadid) {
      const bool lastTimeThruLoop = (threadid == num_threads_in_test5 + 10 - 1);
      
      const unsigned vtimernum = threadid - 10;
      
      // thread [threadid] starts the heretofore stopped vtimer [vtimernum]

      // assert that vtimer [vtimernum] was heretofore stopped
      assertNewVTimerAll(newvtimer_addrs_in_kerninstd[vtimernum],
                         0, 0, // total, rcount
                         0,
                         stop_vmetric_kerninstdAddr,
                         restart_vmetric_kerninstdAddr);

      // start vtimer [vtimernum].  Not kthread-specific; specific only to the vtimer
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_start_em[vtimernum]);
      
      // verify that the timer got started
      assertNewVTimerStarted(newvtimer_addrs_in_kerninstd[vtimernum], 
                             0, 1, // total, rcount
                             stop_vmetric_kerninstdAddr,
                             restart_vmetric_kerninstdAddr);

      // switch out kthread [threadid] and switch in kthread [threadid+1];
      // this should stop the vtimer
      switchout(cswitchout_kerninstdAddr, threadid);

      switchin(cswitchin_kerninstdAddr, !lastTimeThruLoop ? threadid+1 : 10);

      assertNewVTimerStopped(newvtimer_addrs_in_kerninstd[vtimernum],
                             stop_vmetric_kerninstdAddr,
                             restart_vmetric_kerninstdAddr);
   }

   // test5 -- kthread 10 is presently active.
   // Next cycle: loop thru all kthreads as before, switching in & out, with no
   // explicit starts or stops.
   for (unsigned threadid=10; threadid < 10+num_threads_in_test5; ++threadid) {
      const bool lastTimeThruLoop = (threadid == num_threads_in_test5 + 10 - 1);
      
      const unsigned vtimernum = threadid - 10;
      
      // assert that vtimer [vtimernum] is active (it should be because all vtimers
      // were explicitly started without being explicitly stopped, so as long as the
      // correct kthread is running, a given vtimer should be active).  Along similar
      // lines of thought, assert that all other vtimers are NOT active.
      for (unsigned vtimerlcv=0; vtimerlcv < num_threads_in_test5; ++vtimerlcv) {
         theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[vtimerlcv]);
         assert(theNewVTimerPtr->stop_routine == stop_vmetric_kerninstdAddr);
         assert(theNewVTimerPtr->restart_routine == restart_vmetric_kerninstdAddr);

         if (vtimerlcv == vtimernum) {
            // this vtimer should be active
            assert(theNewVTimerPtr->start_field.rcount == 1);
            assert(theNewVTimerPtr->start_field.start > 0);
         }
         else
            // this vtimer should NOT be active
            assert(theNewVTimerPtr->start_field.rcount == 0);
      }

      // Now switch out kthread (threadid) and switch in kthread (threadid+1)
      // (except for last time thru loop, in which case kthread 10 should be the
      // one that's resumed).  After the switch-out, all vtimers should be off.
      switchout(cswitchout_kerninstdAddr, threadid);

      for (unsigned vtimerlcv=0; vtimerlcv < num_threads_in_test5; ++vtimerlcv) {
         // can't use assertNewVTimerStopped() because we're not sure that the total
         // field will be > 0.
         theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[vtimerlcv]);
         assert(theNewVTimerPtr->start_field.rcount == 0);
      }
      
      // Now switch in kthread (threadid+1) (or kthread 10, if last time thru loop).
      switchin(cswitchin_kerninstdAddr, !lastTimeThruLoop ? threadid+1 : 10);
   }
   
   // test 5 -- for the final loop, explicitly stop the vtimers
   for (unsigned threadid=10; threadid < 10+num_threads_in_test5; ++threadid) {
      const bool lastTimeThruLoop = (threadid == num_threads_in_test5 + 10 - 1);
      
      const unsigned vtimernum = threadid - 10;
      
      // assert that vtimer [vtimernum] is active (it should be because all vtimers
      // were explicitly started without being explicitly stopped, so as long as the
      // correct kthread is running, a given vtimer should be active).  Along similar
      // lines of thought, assert that all other vtimers are NOT active.
      for (unsigned vtimerlcv=0; vtimerlcv < num_threads_in_test5; ++vtimerlcv) {
         theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[vtimerlcv]);
         assert(theNewVTimerPtr->stop_routine == stop_vmetric_kerninstdAddr);
         assert(theNewVTimerPtr->restart_routine == restart_vmetric_kerninstdAddr);

         if (vtimerlcv == vtimernum) {
            // this vtimer should be active
            assert(theNewVTimerPtr->start_field.rcount == 1);
            assert(theNewVTimerPtr->start_field.start != 0);
         }
         else
            // this vtimer should NOT be active
            assert(theNewVTimerPtr->start_field.rcount == 0);
      }

      // explicitly stop this vtimer
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_stop_em[vtimernum]);
      
      // Now switch out kthread (threadid) and switch in kthread (threadid+1)
      // (except for last time thru loop, in which case kthread 10 should be the
      // one that's resumed).  After the switch-out, all vtimers should be off.
      switchout(cswitchout_kerninstdAddr, threadid);

      for (unsigned vtimerlcv=0; vtimerlcv < num_threads_in_test5; ++vtimerlcv) {
         theNewVTimerPtr = getNewVTimerV(newvtimer_addrs_in_kerninstd[vtimerlcv]);
         assert(theNewVTimerPtr->start_field.rcount == 0x0);
      }
      
      // Now switch in kthread (threadid+1) (or kthread 10, if last time thru loop).
      switchin(cswitchin_kerninstdAddr, !lastTimeThruLoop ? threadid+1 : 10);
   }
      

   // Test 6 -- Just one vtimer, but it's started by a whole bunch of kthreads.
   // We then stress test the on-switch-out and on-switch-in by doing a ton of
   // context switches.

   // test 6 -- reinitialize all of the vtimers
   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      tempBufferEmitter init_vtimer_em(kerninstdABI);
#if 0
      countedVEvents::emit_initialization(init_vtimer_em,
                                          theGlobalInstrumenter->getAvailRegsForDownloadedToKerninstdCode(false),
                                             // false --> don't ignore the client-id param
					  theGlobalDataHeap->getElemsPerVectorVT(),
					  theGlobalDataHeap->getBytesPerStrideVT(),
                                          newvtimer_addrs_in_kerninstd[lcv],
                                          stop_vmetric_kerninstdAddr,
                                          restart_vmetric_kerninstdAddr);
#endif
      init_vtimer_em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_vtimer_em);
   }

   const unsigned num_kthreads_in_test6 = 20;
   
   // test 6 -- have a bunch of kthreads start the same vtimer
   for (unsigned threadlcv=0; threadlcv < num_kthreads_in_test6; ++threadlcv) {
      unsigned kthread = threadlcv + 10;

      new_vtimer theNewVTimer1 = *getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
      assert(theNewVTimer1.start_field.rcount == 0x0);

      // switch in [kthread]
      switchin(cswitchin_kerninstdAddr, kthread);

      new_vtimer theNewVTimer2 = *getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
      assert(theNewVTimer2.start_field.rcount == 0);
      assert(theNewVTimer2.total_field == theNewVTimer1.total_field);
      assert(theNewVTimer2.start_field.start == theNewVTimer1.start_field.start);
      
      // have [kthread] start the vtimer
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(*newvtimers_start_em[0]);

      new_vtimer theNewVTimer3 = *getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
      assert(theNewVTimer3.start_field.rcount == 1);
      assert(theNewVTimer3.total_field == theNewVTimer2.total_field);
      assert(theNewVTimer3.start_field.start != theNewVTimer2.start_field.start);

      // switch out [kthread]
      switchout(cswitchout_kerninstdAddr, kthread);

      new_vtimer theNewVTimer4 = *getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
      assert(theNewVTimer4.start_field.rcount == 0x0);
      assert(theNewVTimer4.total_field > theNewVTimer3.total_field);
   }

   // test 6 -- no stops or starts, just do a bunch of context switches.  Some of
   // the kthreads are interesting ones, some are not.

   const unsigned test6_num_cswitches = 100;
   for (unsigned cswitchlcv = 0; cswitchlcv < test6_num_cswitches; ++cswitchlcv) {
      unsigned kthread = ((unsigned)random() % (2*num_kthreads_in_test6)) + 10;

      const bool kthread_is_interesting = (kthread-10 < num_kthreads_in_test6);

      new_vtimer theNewVTimer1 = *getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
      assert(theNewVTimer1.start_field.rcount == 0x0);
      
      // switch in [kthread]
      switchin(cswitchin_kerninstdAddr, kthread);

      new_vtimer theNewVTimer2 = *getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
      
      if (kthread_is_interesting) {
         assert(theNewVTimer2.start_field.rcount == 1);
         assert(theNewVTimer2.start_field.start != theNewVTimer1.start_field.start);
         assert(theNewVTimer2.total_field == theNewVTimer1.total_field);
      }
      else {
         // assert that nothing has changed
         assert(theNewVTimer2.start_field.rcount == theNewVTimer1.start_field.rcount);
         assert(theNewVTimer2.total_field == theNewVTimer1.total_field);
         assert(theNewVTimer2.start_field.start == theNewVTimer1.start_field.start);
      }

      // switch out [kthread]
      switchout(cswitchout_kerninstdAddr, kthread);

      new_vtimer theNewVTimer3 = *getNewVTimerV(newvtimer_addrs_in_kerninstd[0]);
      
      if (kthread_is_interesting) {
         assert(theNewVTimer3.start_field.rcount == 0x0);
         assert(theNewVTimer3.total_field > theNewVTimer2.total_field);
      }
      else {
         assert(theNewVTimer3.start_field.rcount == theNewVTimer2.start_field.rcount);
         assert(theNewVTimer3.total_field == theNewVTimer2.total_field);
         assert(theNewVTimer3.start_field.start == theNewVTimer2.start_field.start);
      }
   }

   

   

   // --------------------------------------------------------------------------------
   // ------------------------------ Cleanup -----------------------------------------
   // --------------------------------------------------------------------------------

   for (unsigned lcv=0; lcv < num_newvtimers_in_test; ++lcv) {
      delete theNewStartPrimitives[lcv];
      delete theNewStopPrimitives[lcv];
      delete newvtimers_start_em[lcv];
      delete newvtimers_stop_em[lcv];
      
      theGlobalInstrumenter->removeDownloadedToKerninstd(newstartprimitive_reqids[lcv]);
      theGlobalInstrumenter->removeDownloadedToKerninstd(newstopprimitive_reqids[lcv]);

      theGlobalDataHeap->unAllocVT(newvtimer_addrs_in_kernel[lcv]);
   }
   
//     for (unsigned lcv=0; lcv < num_oldvtimers_in_test; ++lcv) {
//        theGlobalInstrumenter->removeDownloadedToKerninstd(oldstartprimitive_reqids[lcv]);
//        theGlobalInstrumenter->removeDownloadedToKerninstd(oldstopprimitive_reqids[lcv]);

//        theGlobalDataHeap->unAlloc_vtimer24(oldvtimer_ids[lcv]);
//     }
   
   theGlobalInstrumenter->removeDownloadedToKerninstd(stop_vmetric_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(restart_vmetric_reqid);

//     theGlobalInstrumenter->removeDownloadedToKerninstd(stop_pic0VTimer_reqid);
//     theGlobalInstrumenter->removeDownloadedToKerninstd(restart_pic0VTimer_reqid);

   theGlobalInstrumenter->removeDownloadedToKerninstd(cswitchin_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(cswitchout_reqid);

   theGlobalInstrumenter->removeDownloadedToKerninstd(hashtable_remove_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(hashtable_add_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(hashtable_pack_reqid);
   //theGlobalInstrumenter->removeDownloadedToKerninstd(hashtable_keyandbin2elem_reqid);
   //theGlobalInstrumenter->removeDownloadedToKerninstd(hashtable_hashfunc_reqid);
   theGlobalInstrumenter->freeMappedKernelMemory(hashTableAddrs.first,
                                                 hashTableAddrs.second,
                                                 hash_table_nbytes);
   
   theGlobalInstrumenter->removeDownloadedToKerninstd(downloaded_stacklist_free_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(downloaded_stacklist_get_reqid);
   theGlobalInstrumenter->freeMappedKernelMemory(stacklist_addrs.first,
                                                 stacklist_addrs.second,
                                                 stacklist_nbytes);
   
//   theGlobalInstrumenter->removeDownloadedToKerninstd(all_vtimers_remove_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(all_vtimers_add_reqid);
   theGlobalInstrumenter->freeMappedKernelMemory(all_vtimers_addrs.first,
                                                 all_vtimers_addrs.second,
                                                 sizeof_allvtimers);
}

// --------------------------------------------------------------------------------

void vtimerMgr::test_all_vtimers_stuff() {
   // assuming all_vtimers has been allocated and the 3 routines (init, all,
   // remove) have been downloaded into kerninstd, test 'em!

   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   
   pair<kptr_t, dptr_t> addrs = 
     theGlobalInstrumenter->allocateMappedKernelMemory(sizeof_allvtimers,
                                                       false);
      // false --> don't try to allocate from kernel's nucleus text
   const dptr_t all_vtimers_kerninstdAddr = addrs.second;
   
   // initialize
   tempBufferEmitter init_em(kerninstdABI);
   emit_all_vtimers_initialize(init_em, all_vtimers_kerninstdAddr);
   init_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_em);
   
   // verify initialization worked properly
   pdvector<uint32_t> raw_all_vtimers_data = theGlobalInstrumenter->peek_kerninstd_contig(all_vtimers_kerninstdAddr, sizeof_allvtimers);
   const all_vtimers *theAllVTimersPtr = (all_vtimers*)raw_all_vtimers_data.begin();
   assert(theAllVTimersPtr->vtimer_addresses[0] == 0);
   assert(theAllVTimersPtr->finish == (all_vtimers_kerninstdAddr + 0));

   // download add and remove routines
   tempBufferEmitter add_em(kerninstdABI);
   emit_all_vtimers_add(add_em);
   add_em.complete();
   const unsigned all_vtimers_add_reqid =
      theGlobalInstrumenter->downloadToKerninstd(add_em);
   pair<pdvector<dptr_t>,unsigned> theAddrs = theGlobalInstrumenter->
      queryDownloadedToKerninstdAddresses(all_vtimers_add_reqid);
   const dptr_t all_vtimers_add_kerninstdAddr = theAddrs.first[theAddrs.second];

   tempBufferEmitter remove_em(kerninstdABI);
   emit_all_vtimers_remove(remove_em);
   remove_em.complete();
   const unsigned all_vtimers_remove_reqid =
      theGlobalInstrumenter->downloadToKerninstd(remove_em);
   theAddrs = theGlobalInstrumenter->
      queryDownloadedToKerninstdAddresses(all_vtimers_remove_reqid);
   const dptr_t all_vtimers_remove_kerninstdAddr = theAddrs.first[theAddrs.second];

   
   
   for (unsigned lcv=0; lcv < max_num_vtimers+1; ++lcv) {
      // add.  2 params needed: all_vtimers; address of a new vtimer in kernel space
      tempBufferEmitter em(kerninstdABI);
      em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));
      // needed because we make a non-tail function call, and we don't have
      // a register in which to save %o7.

      em.emitImmAddr(all_vtimers_kerninstdAddr, sparc_reg::o0);
      em.emitImmAddr(0xe0000000, sparc_reg::o1);
      em.emitImmAddr(all_vtimers_add_kerninstdAddr, sparc_reg::o2);

      em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o2));
      em.emit(sparc_instr(sparc_instr::nop));

      // result should be 1 (success), not 0.  assert it
      em.emitCondBranchToForwardLabel(sparc_instr(sparc_instr::bpr,
                                                  sparc_instr::regZero,
                                                  false, // not annulled
                                                  false, // predict not taken
                                                  sparc_reg::o0,
                                                  0),
                                      "failure_0");
      em.emit(sparc_instr(sparc_instr::nop));
      // success, part 1.  It's not zero.  Make sure it's 1.
      em.emit(sparc_instr(sparc_instr::sub, sparc_reg::o0, 0x1, sparc_reg::o0));
      em.emitCondBranchToForwardLabel(sparc_instr(sparc_instr::bpr,
                                                  sparc_instr::regNotZero,
                                                  false, false,
                                                  sparc_reg::o0,
                                                  0),
                                      "failure_unknown");
      em.emit(sparc_instr(sparc_instr::nop));

      // success, part 2 [final success]
      em.emit(sparc_instr(sparc_instr::ret));
      em.emit(sparc_instr(sparc_instr::restore));

      em.emitLabel("failure_0");
      em.emitLabel("failure_unknown");
      em.emit(sparc_instr(sparc_instr::trap, 0x34));
      em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);

      // verify add made the expected changes
      raw_all_vtimers_data = theGlobalInstrumenter->peek_kerninstd_contig(all_vtimers_kerninstdAddr, sizeof_allvtimers);
      theAllVTimersPtr = (all_vtimers*)raw_all_vtimers_data.begin();
      assert(theAllVTimersPtr->vtimer_addresses[0] == 0xe0000000);
      assert(theAllVTimersPtr->vtimer_addresses[1] == 0);
      assert(theAllVTimersPtr->finish == (all_vtimers_kerninstdAddr + 
					  sizeof(kptr_t)));
   
      // remove. 2 params: all_vtimers; address of vtimer in kernel space to remove
      em.reset();
      em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));
      // needed because we make a non-tail function call
      em.emitImmAddr(all_vtimers_kerninstdAddr, sparc_reg::o0);
      em.emitImmAddr(0xe0000000, sparc_reg::o1);
      em.emitImmAddr(all_vtimers_remove_kerninstdAddr, sparc_reg::o2);
      // make the call
      em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o2));
      em.emit(sparc_instr(sparc_instr::nop));
      // assert that the result is 1
      em.emit(sparc_instr(sparc_instr::sub, true, // set cc
                          false, // not carry
                          sparc_reg::o0, // result
                          sparc_reg::o0, 0x1));
      em.emit(sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                          true, // xcc (shouldn't matter)
                          0x34));
      // return
      em.emit(sparc_instr(sparc_instr::ret));
      em.emit(sparc_instr(sparc_instr::restore));
      em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);

      // verify remove worked properly
      raw_all_vtimers_data = theGlobalInstrumenter->peek_kerninstd_contig(all_vtimers_kerninstdAddr, sizeof_allvtimers);
      theAllVTimersPtr = (all_vtimers*)raw_all_vtimers_data.begin();
      assert(theAllVTimersPtr->vtimer_addresses[0] == 0);
      assert(theAllVTimersPtr->finish == (all_vtimers_kerninstdAddr + 0));
   }

   // Torture test, phase 2: add [max_num_vtimers-1] vtimers, with increasing
   // addresses (or something like that), asserting that everything succeeds.
   // Then try to add 1 more, asserting that it fails.
   for (unsigned lcv=0; lcv < max_num_vtimers-1; ++lcv) {
      // add.  2 params needed: all_vtimers; address of a new vtimer in kernel space
      tempBufferEmitter em(kerninstdABI);
      em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));
      // needed because we make a non-tail function call, and we don't have
      // a register in which to save %o7.

      em.emitImmAddr(all_vtimers_kerninstdAddr, sparc_reg::o0);
      em.emitImmAddr(0xe0000000 + (lcv * sizeof_vtimer), sparc_reg::o1);
      em.emitImmAddr(all_vtimers_add_kerninstdAddr, sparc_reg::o2);

      em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o2));
      em.emit(sparc_instr(sparc_instr::nop));

      // result should be 1 (success), not 0.  assert it
      em.emitCondBranchToForwardLabel(sparc_instr(sparc_instr::bpr,
                                                  sparc_instr::regZero,
                                                  false, // not annulled
                                                  false, // predict not taken
                                                  sparc_reg::o0,
                                                  0),
                                      "failure_0");
      em.emit(sparc_instr(sparc_instr::nop));
      // success, part 1.  It's not zero.  Make sure it's 1.
      em.emit(sparc_instr(sparc_instr::sub, sparc_reg::o0, 0x1, sparc_reg::o0));
      em.emitCondBranchToForwardLabel(sparc_instr(sparc_instr::bpr,
                                                  sparc_instr::regNotZero,
                                                  false, false,
                                                  sparc_reg::o0,
                                                  0),
                                      "failure_unknown");
      em.emit(sparc_instr(sparc_instr::nop));

      // success, part 2 [final success]
      em.emit(sparc_instr(sparc_instr::ret));
      em.emit(sparc_instr(sparc_instr::restore));

      em.emitLabel("failure_0");
      em.emitLabel("failure_unknown");
      em.emit(sparc_instr(sparc_instr::trap, 0x34));
      em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);

      raw_all_vtimers_data = theGlobalInstrumenter->peek_kerninstd_contig(all_vtimers_kerninstdAddr, sizeof_allvtimers);
      theAllVTimersPtr = (all_vtimers*)raw_all_vtimers_data.begin();
      for (unsigned lcv2=0; lcv2 <= lcv; lcv2++)
         assert(theAllVTimersPtr->vtimer_addresses[lcv2] == (0xe0000000 + (sizeof_vtimer*lcv2)));
      assert(theAllVTimersPtr->finish == (all_vtimers_kerninstdAddr +
					  sizeof(kptr_t) * (lcv+1)));
   }

   for (unsigned failurelcv=0; failurelcv < 10; ++failurelcv) {
      // verify add made the expected changes
      raw_all_vtimers_data = theGlobalInstrumenter->peek_kerninstd_contig(all_vtimers_kerninstdAddr, sizeof_allvtimers);
      theAllVTimersPtr = (all_vtimers*)raw_all_vtimers_data.begin();
      for (unsigned lcv=0; lcv < max_num_vtimers-1; lcv++)
         assert(theAllVTimersPtr->vtimer_addresses[lcv] == (0xe0000000 + (sizeof_vtimer*lcv)));
      assert(theAllVTimersPtr->finish == (all_vtimers_kerninstdAddr +
					  sizeof(kptr_t)*(max_num_vtimers-1)));
      // max_num_vtimers-1 because that's how many actual vtimers are allowed.

      // One more add should fail
      // add.  2 params needed: all_vtimers; address of a new vtimer in kernel space
      tempBufferEmitter em(kerninstdABI);
      em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));
      // needed because we make a non-tail function call, and we don't have
      // a register in which to save %o7.

      em.emitImmAddr(all_vtimers_kerninstdAddr, sparc_reg::o0);
      em.emitImmAddr(0xf0000000, sparc_reg::o1);
      em.emitImmAddr(all_vtimers_add_kerninstdAddr, sparc_reg::o2);

      em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o2));
      em.emit(sparc_instr(sparc_instr::nop));

      // result should be 0 (failure) this time.  Assert it.
      em.emitCondBranchToForwardLabel(sparc_instr(sparc_instr::bpr,
                                                  sparc_instr::regZero,
                                                  false, // not annulled
                                                  false, // predict not taken
                                                  sparc_reg::o0,
                                                  0),
                                      "OK");
      em.emit(sparc_instr(sparc_instr::nop));

      em.emit(sparc_instr(sparc_instr::trap, 0x34));

      em.emitLabel("OK");

      // success, part 2 [final success]
      em.emit(sparc_instr(sparc_instr::ret));
      em.emit(sparc_instr(sparc_instr::restore));

      em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);
   }

   // Now delete all of the entries, one at a time
   for (unsigned removelcv=0; removelcv < max_num_vtimers-1; ++removelcv) {
      tempBufferEmitter em(kerninstdABI);
      em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));
      // needed because we make a non-tail function call
      em.emitImmAddr(all_vtimers_kerninstdAddr, sparc_reg::o0);
      em.emitImmAddr(0xe0000000 + (sizeof_vtimer * removelcv), sparc_reg::o1);
      em.emitImmAddr(all_vtimers_remove_kerninstdAddr, sparc_reg::o2);
      // make the call
      em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o2));
      em.emit(sparc_instr(sparc_instr::nop));
      // assert that the result is 1
      em.emit(sparc_instr(sparc_instr::sub, true, // set cc
                          false, // not carry
                          sparc_reg::o0, // result
                          sparc_reg::o0, 0x1));
      em.emit(sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                          true, // xcc (shouldn't matter)
                          0x34));
      // return
      em.emit(sparc_instr(sparc_instr::ret));
      em.emit(sparc_instr(sparc_instr::restore));
      em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);

      // verify remove worked properly.  There should be [max_num_vtimers-1-removelcv+1]
      // or [max_num_vtimers-removelcv] valid items remaining, and each of them should
      // be (0xe0000000 + (sizeof_vtimer * (removelcv+1))) or higher.
      raw_all_vtimers_data = theGlobalInstrumenter->peek_kerninstd_contig(all_vtimers_kerninstdAddr, sizeof_allvtimers);
      theAllVTimersPtr = (all_vtimers*)raw_all_vtimers_data.begin();
      unsigned num_remaining = max_num_vtimers - 1 - (removelcv +1);
      for (unsigned checklcv=0; checklcv < num_remaining; ++checklcv)
         assert(theAllVTimersPtr->vtimer_addresses[checklcv] >
                (0xe0000000 + (sizeof_vtimer * removelcv)));

      assert(theAllVTimersPtr->finish == (all_vtimers_kerninstdAddr + 
					  sizeof(kptr_t) * num_remaining));
   }
   
   // clean up:
   theGlobalInstrumenter->removeDownloadedToKerninstd(all_vtimers_remove_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(all_vtimers_add_reqid);
   theGlobalInstrumenter->freeMappedKernelMemory(addrs.first, addrs.second,
                                                 sizeof_allvtimers);

   cout << "note: all_vtimers test passed OK" << endl;
}

// --------------------------------------------------------------------------------

void vtimerMgr::test_stacklist_stuff() {
   // unlike testing all_vtimers, this routine cannot assume that the desired
   // routines have already been downloaded into kerninstd, because in fact
   // they'll get downloaded into the kernel.  For this test, we want everything
   // to, of course, take place entirely in kerninstd.  So this method function
   // is stand-alone.

   // allocate the stacklist heap:
   pair<kptr_t, dptr_t> stacklist_addrs =
      theGlobalInstrumenter->allocateMappedKernelMemory(stacklist_nbytes, false);
      // false --> don't try to allocate from w/in kernel's nucleus text
   const dptr_t stacklist_kerninstdAddr = stacklist_addrs.second;
   
   cout << "test: allocated stacklist at kerninstd "
        << addr2hex(stacklist_kerninstdAddr) << endl;

   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   
   // download and execute "stacklist_initialize" to kerninstd:
   tempBufferEmitter init_em(kerninstdABI);
   emit_stacklist_initialize(init_em, stacklist_kerninstdAddr, stacklist_kerninstdAddr);
   init_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_em);

   // verify that stacklist_initialize worked as expected:
   pdvector<uint32_t> raw_data =
      theGlobalInstrumenter->peek_kerninstd_contig(stacklist_kerninstdAddr, stacklist_nbytes);
   const stacklist *theStackList = (const stacklist*)raw_data.begin();
   assert(theStackList->head.ptr == stacklist_kerninstdAddr);
   for (unsigned lcv=0; lcv < max_num_threads-1; ++lcv) {
      const stacklistitem &item = theStackList->theheap[lcv];
      assert(item.next.ptr == (stacklist_kerninstdAddr + (lcv+1)*stacklistitem_nbytes));
   }
   assert(theStackList->theheap[max_num_threads-1].next.ptr == 0);

   // download stacklist_get() and stacklist_free into kerninstd:
   tempBufferEmitter get_em(kerninstdABI);
   emit_stacklist_get(get_em);
   get_em.complete();
   const unsigned downloaded_stacklist_get_reqid =
      theGlobalInstrumenter->downloadToKerninstd(get_em);
   pair<pdvector<dptr_t>,unsigned> theAddrs = theGlobalInstrumenter->
      queryDownloadedToKerninstdAddresses(downloaded_stacklist_get_reqid);
   const dptr_t stacklist_get_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "stacklist test: stacklist_get downloaded to kerninstd "
        << addr2hex(stacklist_get_kerninstdAddr) << endl;
   
   tempBufferEmitter free_em(kerninstdABI);
   emit_stacklist_free(free_em);
   free_em.complete();
   const unsigned downloaded_stacklist_free_reqid =
      theGlobalInstrumenter->downloadToKerninstd(free_em);
   theAddrs = theGlobalInstrumenter->
      queryDownloadedToKerninstdAddresses(downloaded_stacklist_free_reqid);
   const dptr_t stacklist_free_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "stacklist test: stacklist_free downloaded to kerninstd "
        << addr2hex(stacklist_free_kerninstdAddr) << endl;

   // call get/free a bunch of times (more than the total number of entries, for it
   // to be much of a test)
   for (unsigned lcv=0; lcv < max_num_threads+1; ++lcv) {
      tempBufferEmitter em(kerninstdABI);
      em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));

      em.emitImmAddr(stacklist_kerninstdAddr, sparc_reg::o0);
      em.emitImmAddr(stacklist_get_kerninstdAddr, sparc_reg::o1);
      em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o1));
      em.emit(sparc_instr(sparc_instr::nop));
      
      // result is in %o0.  assert that it's not null
      em.emit(sparc_instr(sparc_instr::tst, sparc_reg::o0));
      em.emit(sparc_instr(sparc_instr::trap,
                          sparc_instr::iccEq, // trap if equal to zero
                          true, // extended (probably doesn't matter)
                          0x34));

      // result in %o0: the address of what was allocated
      em.emit(sparc_instr(sparc_instr::mov, sparc_reg::o0, sparc_reg::o1));
      em.emitImmAddr(stacklist_kerninstdAddr, sparc_reg::o0);
      em.emitImmAddr(stacklist_free_kerninstdAddr, sparc_reg::o2);
      em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o2));
      em.emit(sparc_instr(sparc_instr::nop));

      em.emit(sparc_instr(sparc_instr::ret));
      em.emit(sparc_instr(sparc_instr::restore));
      em.complete();

      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);
   }

   // second test: allocate [max_num_threads] times, successfully...then 1 more
   // time unsuccessfully (check return value)
   for (unsigned lcv=0; lcv < max_num_threads; ++lcv) {
      tempBufferEmitter em(kerninstdABI);
      em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));

      em.emitImmAddr(stacklist_kerninstdAddr, sparc_reg::o0);
      em.emitImmAddr(stacklist_get_kerninstdAddr, sparc_reg::o1);
      em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o1));
      em.emit(sparc_instr(sparc_instr::nop));
      
      // result is in %o0.  assert that it's not null
      em.emit(sparc_instr(sparc_instr::tst, sparc_reg::o0));
      em.emit(sparc_instr(sparc_instr::trap,
                          sparc_instr::iccEq, // equal to zero
                          true, // extended (probably doesn't matter)
                          0x34));

      em.emit(sparc_instr(sparc_instr::ret));
      em.emit(sparc_instr(sparc_instr::restore));
      em.complete();

      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);
   }
   
   tempBufferEmitter em(kerninstdABI);
   em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));

   em.emitImmAddr(stacklist_kerninstdAddr, sparc_reg::o0);
   em.emitImmAddr(stacklist_get_kerninstdAddr, sparc_reg::o1);
   em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o1));
   em.emit(sparc_instr(sparc_instr::nop));
      
   // result is in %o0.  assert that it's null
   em.emit(sparc_instr(sparc_instr::tst, sparc_reg::o0));
   em.emit(sparc_instr(sparc_instr::trap,
                       sparc_instr::iccNotEq, // equal to zero
                       true, // extended (probably doesn't matter)
                       0x34));

   em.emit(sparc_instr(sparc_instr::ret));
   em.emit(sparc_instr(sparc_instr::restore));
   em.complete();

   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);
   
   
   theGlobalInstrumenter->removeDownloadedToKerninstd(downloaded_stacklist_free_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(downloaded_stacklist_get_reqid);
   theGlobalInstrumenter->freeMappedKernelMemory(stacklist_addrs.first,
                                                 stacklist_nbytes);

   cout << "stacklist test: passed OK" << endl;
}

// --------------------------------------------------------------------------------

static void emit_call_to_hashtable_add(tempBufferEmitter &em,
                                       uinthash_t key,
                                       kptr_t dataptr,
                                       kptr_t hashtable_add_kernelAddr,
                                       bool assert_success,
                                       bool assert_failure) {
   assert(!assert_success || !assert_failure);

   // It may seem silly, but we really do need to emit a save here.
   // The reason: we're making a call, and it can't be made into a tail call.

   em.emit(sparc_instr(sparc_instr::save,
                       -em.getABI().getMinFrameAlignedNumBytes()));
   
   em.emitImmAddr(key, sparc_reg::o0);
   em.emitImmAddr(dataptr, sparc_reg::o1);
   em.emitImmAddr(hashtable_add_kernelAddr, sparc_reg::o2);
   em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o2));
   em.emit(sparc_instr(sparc_instr::nop));

   if (assert_success || assert_failure) {
      em.emitImm32(assert_success ? 0x1 : 0x0, sparc_reg::o1);
      em.emit(sparc_instr(sparc_instr::sub, true, false, // cc but no carry
                          sparc_reg::g0, // dest (we only care about cc)
                          sparc_reg::o1, sparc_reg::o0));

      em.emit(sparc_instr(sparc_instr::trap,
                          sparc_instr::iccNotEq,
                          false, // not extended
                          0x34));
   }
   
   em.emit(sparc_instr(sparc_instr::ret));
   em.emit(sparc_instr(sparc_instr::restore));
}

static void emit_call_to_hashtable_remove(tempBufferEmitter &em,
                                          uinthash_t key,
                                          kptr_t expected_val,
                                          kptr_t hashtable_remove_kernelAddr) {
   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   em.emit(sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes()));

   em.emitImmAddr(key, sparc_reg::o0);
   em.emitImmAddr(hashtable_remove_kernelAddr, sparc_reg::o1);
   em.emit(sparc_instr(sparc_instr::callandlink, sparc_reg::o1));
   em.emit(sparc_instr(sparc_instr::nop));
      
   // assert that the return value equals expected_val
   em.emitImmAddr(expected_val, sparc_reg::o1);
   em.emit(sparc_instr(sparc_instr::sub, true, false, // cc but no carry
                       sparc_reg::g0, // dest (care only about cc)
                       sparc_reg::o1, sparc_reg::o0));
   em.emit(sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                       false, // not extended
                       0x34));

   em.emit(sparc_instr(sparc_instr::ret));
   em.emit(sparc_instr(sparc_instr::restore));
}

static const hash_table_elem *
elem_ptr_raw2elem_ptr(const hash_table_elem *rawInKernel,
                      kptr_t hashTableStartAddrInKernel,
                      dptr_t hashTableStartAddrInHere) {
   if (rawInKernel == 0)
      return NULL;
   
   const dptr_t result = hashTableStartAddrInHere + 
	   (dptr_t)((kptr_t)rawInKernel - hashTableStartAddrInKernel);
   return (const hash_table_elem*)result;
}

static void verify_hashtable_validity(const hash_table &theHashTable,
                                      const dictionary_hash<uinthash_t, kptr_t> &cmp,
                                      kptr_t hashTableStartAddrInKernel,
                                      bool assertNoRemovedElems)
{
   const dptr_t hashTableStartAddrInHere = (dptr_t)&theHashTable;
   const unsigned theHashTableSize_incl_removed = theHashTable.num_elems;
   unsigned theHashTableSize_NonRemoved = 0;
   
   dictionary_hash<uinthash_t, kptr_t > dwindling_cmp(cmp);
      // for every non-removed entry that we come across, we'll remove
      // a value from this dictionary (asserting it already existed).
      // Then, when all done, we'll assert that this dictionary is empty.

   // can't assert that theHashTableSize_incl_removed == cmp.size(), due to removed
   // elems in theHashTable

   // loop thru all_elems[].  A non-removed entry should definitely be
   // in cmp[].  A removed entry is a bit trickier, because there could be
   // a non-removed entry with this same key later on representing the "real" entry.
   for (unsigned lcv=0; lcv < theHashTableSize_incl_removed; ++lcv) {
      const hash_table_elem &e = theHashTable.all_elems[lcv];
      const uinthash_t key = e.the_key;
      const kptr_t data = e.the_data;

      if (assertNoRemovedElems)
         assert(!e.removed && "found an unexpected removed element");

      if (e.removed)
         //assert(!cmp.defines(key)); can't assert this; see above
         ;
      else {
         assert(cmp.get(key) == data);
         theHashTableSize_NonRemoved++;

         assert(dwindling_cmp.defines(key));
         dwindling_cmp.undef(key);
      }
   }

   assert(theHashTableSize_NonRemoved == cmp.size());
   assert(dwindling_cmp.size() == 0);
   
   // loop through the elements via bins[], not via all_elems[]
   theHashTableSize_NonRemoved = 0;
   dwindling_cmp = cmp;
   
   for (unsigned binlcv=0; binlcv < hash_table_num_bins; ++binlcv) {
      const hash_table_elem *elem_ptr_raw_inkernel = (const hash_table_elem *)theHashTable.bins[binlcv];
      const hash_table_elem *elem_ptr =
         elem_ptr_raw2elem_ptr(elem_ptr_raw_inkernel,
                               hashTableStartAddrInKernel,
                               hashTableStartAddrInHere);
      if (elem_ptr == NULL)
         continue;
      
      while (elem_ptr != NULL) {
         const uinthash_t key = elem_ptr->the_key;
         const kptr_t data = elem_ptr->the_data;
         const bool removed = elem_ptr->removed;

         if (assertNoRemovedElems)
            assert(!elem_ptr->removed && "found an unexpected removed element, and only when traversing bins!");

         // as above, we can't really assert anything if the removed flag
         // is set, because a non-removed entry with the same key could still
         // exist later, representing the "real" value.
         if (removed)
            //assert(!cmp.defines(key));
            ;
         else {
            assert(cmp.get(key) == data);
            theHashTableSize_NonRemoved++;

            assert(dwindling_cmp.defines(key));
            dwindling_cmp.undef(key);
         }

         elem_ptr = elem_ptr_raw2elem_ptr((const hash_table_elem *)elem_ptr->next, // raw in-kernel address
                                          hashTableStartAddrInKernel,
                                          hashTableStartAddrInHere);
      }
   }

   assert(theHashTableSize_NonRemoved == cmp.size());
   assert(dwindling_cmp.size() == 0);
}

static hash_table *readHashTable(dptr_t hashtable_kerninstdAddr,
                                 pdvector<uint32_t> &buffer) {
   buffer = theGlobalInstrumenter->peek_kerninstd_contig(hashtable_kerninstdAddr,
                                                  hash_table_nbytes);
   hash_table &theHashTable = *(hash_table*)buffer.begin();

   return &theHashTable;
}


static unsigned
read_and_verify_hashtable_validity(dptr_t hashtable_kerninstdAddr,
                                   const dictionary_hash<uinthash_t, kptr_t > &match_me,
                                   bool assertNoRemovedElems
                                   ) {
   pdvector<uint32_t> buffer;
   hash_table *theHashTablePtr = readHashTable(hashtable_kerninstdAddr,
                                               buffer);

   verify_hashtable_validity(*theHashTablePtr, match_me, hashtable_kerninstdAddr,
                             assertNoRemovedElems);

   return theHashTablePtr->num_elems;
}

void vtimerMgr::test_hashtable_stuff(const sparc_reg_set & /* avail for downloaded */,
                                     const abi &kerninstdABI) {
   pair<kptr_t, dptr_t> hashTableAddrs =
      theGlobalInstrumenter->allocateMappedKernelMemory(hash_table_nbytes, false);
      // false --> don't try to allocate from w/in kernel's nucleus text
   const dptr_t hashtable_kerninstdAddr = hashTableAddrs.second;

   cout << "allocated hash table at kerninstd " << addr2hex(hashtable_kerninstdAddr)
        << endl;

   // initialize hash table to empty
   tempBufferEmitter init_em(kerninstdABI);
   emit_initialize_hashtable(init_em, hashtable_kerninstdAddr);
   init_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_em);
   
   // assert that the hash table got properly initialized
   pdvector<uint32_t> raw_data =
      theGlobalInstrumenter->peek_kerninstd_contig(hashtable_kerninstdAddr, hash_table_nbytes);
   const hash_table &theHashTable = *(hash_table*)raw_data.begin();
   assert(theHashTable.num_elems == 0);
   for (unsigned binlcv=0; binlcv < hash_table_num_bins; ++binlcv)
      assert(theHashTable.bins[binlcv] == 0);
   
   // download a bunch of hash table routines into kerninstd

   // ********************************************************************************
   // hash_table_add() and hash_table_pack():
   // ********************************************************************************

   const sparc_reg hashTableAdd_keyreg = sparc_reg::o0;
   const sparc_reg hashTableAdd_valuereg = sparc_reg::o1;
   const sparc_reg hashTableAdd_result_reg = sparc_reg::o0;
      // Make sure to put the input params in %o regs, or else we may not be able to
      // find them when/if add() needs to emit a save.

   const sparc_reg_set availRegsAtCallToHashTableAdd =
      sparc_reg_set::allOs + sparc_reg_set::allLs + sparc_reg_set::allIs + sparc_reg::g1 - sparc_reg::sp - sparc_reg::fp - sparc_reg::i7 - sparc_reg::o7;
      // explanation: hash table add is invoked from code generated by
      // emit_call_to_hashtable_add(), which itself has to do a save (for misc
      // reasons that we won't go into here).  The stub code which invoked "call to
      // hash table add" writes to %o7 (which is now %i7), so %i7 ain't free.
      // Then we remove the usual %o7 (since "call to hash table add" code made a
      // call to hash_table_add()) and the even more usual %fp/%sp.

   const fnRegInfo hashTableAddFnInfo(false, // not inlined
                                      fnRegInfo::regsAvailAtCallTo,
                                      availRegsAtCallToHashTableAdd,
                                      fnRegInfo::blankParamInfo +
                                      make_pair(pdstring("key"), hashTableAdd_keyreg) +
                                      make_pair(pdstring("value"), hashTableAdd_valuereg),
                                      hashTableAdd_result_reg);

   tempBufferEmitter add_em(kerninstdABI);
   const fnRegInfo fnRegInfoForHashTablePack =
      emit_hashtable_add(add_em,
                         hashTableAddFnInfo, // params, result reg contained here
                            // note that we'll probably emit a save
                         hashtable_kerninstdAddr);

   tempBufferEmitter pack_em(kerninstdABI);
   emit_hashtable_pack(pack_em,
                       hashtable_kerninstdAddr,
                       fnRegInfoForHashTablePack);
   pack_em.complete();
   const unsigned hashtable_pack_reqid =
      theGlobalInstrumenter->downloadToKerninstd(pack_em);
   pair<pdvector<dptr_t>,unsigned> theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(hashtable_pack_reqid);
   const dptr_t hashtable_pack_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "hashtable_pack() installed at kerninstd "
        << addr2hex(hashtable_pack_kerninstdAddr) << endl;

   // Now that we know the address of the pack routine, we can resolve the
   // call to it from the add routine(), and then complete add:
   add_em.makeSymAddrKnown("hashTablePackAddr", hashtable_pack_kerninstdAddr);
   add_em.complete();

   const unsigned hashtable_add_reqid =
      theGlobalInstrumenter->downloadToKerninstd(add_em);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(hashtable_add_reqid);
   const dptr_t hashtable_add_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "hashtable_add() installed at kerninstd "
        << addr2hex(hashtable_add_kerninstdAddr) << endl;

   // hash_table_remove():
   tempBufferEmitter remove_em(kerninstdABI);
   const sparc_reg hashtableremove_key_reg = sparc_reg::o0;
   const sparc_reg hashtableremove_result_reg = sparc_reg::o0;
   const sparc_reg_set availRegsAtCallToHashTableRemove =
      availRegsAtCallToHashTableAdd; // see that variable for an explanation...

   const fnRegInfo hashTableRemoveFnInfo(false, // not inlined
                                         fnRegInfo::regsAvailAtCallTo,
                                         availRegsAtCallToHashTableRemove,
                                         fnRegInfo::blankParamInfo +
                                         make_pair(pdstring("key"), hashtableremove_key_reg),
                                         hashtableremove_result_reg);
                                         
   emit_hashtable_remove(remove_em,
                         hashtable_kerninstdAddr,
                         hashTableRemoveFnInfo);

   remove_em.complete();
   const unsigned hashtable_remove_reqid =
      theGlobalInstrumenter->downloadToKerninstd(remove_em);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKerninstdAddresses(hashtable_remove_reqid);
   const dptr_t hashtable_remove_kerninstdAddr = theAddrs.first[theAddrs.second];
   cout << "hashtable_remove() installed at kerninstd "
        << addr2hex(hashtable_remove_kerninstdAddr) << endl;

   // Now we are ready to perform some testing.

   // Test: add [hash_table_max_num_elems] elements.  We'll use a dictionary_hash
   // of our own to keep track of what's been added so that we can later delete it.
   dictionary_hash<uinthash_t, kptr_t> tid2data(addrHash4);

   for (unsigned outer_loop_lcv=0; outer_loop_lcv < 1; ++outer_loop_lcv) {
      cout << "outer loop" << endl;

      for (unsigned lcv=0; lcv < hash_table_max_num_elems; ++lcv) {
         // make up a thread id/%g7 value.
         uinthash_t tid;
         while (true) {
            tid = (sizeof(kptr_t) * (unsigned)random()) & 0xffffffff;
            if (!tid2data.defines(tid))
               // ok; this key hasn't been used; you may therefore use it.
               break;
         }
         assert(!tid2data.defines(tid));
      
         // make up a data value
         kptr_t dataptr = (kptr_t)((sizeof(kptr_t) * (unsigned)random()) & 
				   0xffffffff);
         // doesn't really matter if this dataptr already exists.
      
         tid2data.set(tid, dataptr);
      
         // download & execute code to add this item to the hash table.  assert success
         // (no overflow)
         tempBufferEmitter em(kerninstdABI);
         emit_call_to_hashtable_add(em, tid, dataptr, hashtable_add_kerninstdAddr,
                                    true, // assert success
                                    false // do not assert failure
                                    );
         em.complete();
         theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);

         read_and_verify_hashtable_validity(hashtable_kerninstdAddr, tid2data,
                                            false // don't assert nothing removed
                                            );
      }
      // Test: one more add should fail (full hash table)
      tempBufferEmitter em(kerninstdABI);
      emit_call_to_hashtable_add(em,
                                 0xff, // dummy tid (not divisible by 4 so not in use)
                                 (kptr_t)0xffffffff, // dummy data
                                 hashtable_add_kerninstdAddr,
                                 false, // do not assert success
                                 true // assert failure
                                 );
      em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);
      read_and_verify_hashtable_validity(hashtable_kerninstdAddr, tid2data,
                                         false // don't assert nothing removed
                                         );

      // Test: remove all of the elements in tid2data[]
      while (tid2data.size()) {
         dictionary_hash<uinthash_t, kptr_t>::const_iterator dict_iter =
            tid2data.begin();
         const uinthash_t key = dict_iter.currkey();
         kptr_t expected_data = dict_iter.currval();

         tempBufferEmitter em(kerninstdABI);
         emit_call_to_hashtable_remove(em, key, expected_data,
                                       hashtable_remove_kerninstdAddr);
         em.complete();
         theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);

         tid2data.undef(key);

         read_and_verify_hashtable_validity(hashtable_kerninstdAddr, tid2data,
                                            false // don't assert nothing removed
                                            );
      }

      // Test: one more remove should fail (underflow)
      em.reset();
      emit_call_to_hashtable_remove(em,
                                    0xff, // dummy key
                                    0, // expected value (0 is important; asserted)
                                    hashtable_remove_kerninstdAddr);
      em.complete();
      theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);

      assert(tid2data.size() == 0);

      read_and_verify_hashtable_validity(hashtable_kerninstdAddr, tid2data,
                                         false // don't assert nothing removed
                                         );
   }
   
   // Second hash table test
   cout << "begin second hash table test" << endl;

   tid2data.clear();
   pdvector<uinthash_t> threadIds;
   dictionary_hash<uinthash_t, bool> threadIdsAsDict(addrHash4);
   const unsigned maxNumThreads = hash_table_max_num_elems*sizeof(kptr_t); // ?
   for (unsigned lcv=0; lcv < maxNumThreads; ++lcv) {
      uinthash_t threadId;
      while (true) {
         threadId = random() % maxNumThreads;
         if (threadIdsAsDict.defines(threadId))
            continue;
         else
            break;
      }
      threadIdsAsDict.set(threadId, true);
      threadIds += threadId;
   }

   const unsigned numops = 1000;
   for (unsigned oplcv=0; oplcv < numops; ++oplcv) {
      unsigned n=read_and_verify_hashtable_validity(hashtable_kerninstdAddr, tid2data,
                                                    false // don't assert nothing removed
                                                    );
      cout << "[" << oplcv << ", sz=" << n << ", logicsz=" << tid2data.size() << "]: ";
      
      // simulate switch-in & preempt/switchout of a certain thread, chosen
      // at random.
      const uinthash_t threadId = threadIds[random() % threadIds.size()];

      // try to remove & get [threadId].  This is switchin.  OK if not found.
      if (tid2data.defines(threadId)) {
         cout << "remov " << (void*)threadId << ' ';
         cout << flush;
         tempBufferEmitter em(kerninstdABI);
         emit_call_to_hashtable_remove(em, threadId, threadId, // expected data
                                       hashtable_remove_kerninstdAddr);
         em.complete();
         theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);
      
         // update the shadow dictionary
         tid2data.undef(threadId);

         n=read_and_verify_hashtable_validity(hashtable_kerninstdAddr, tid2data,
                                              false // don't assert nothing removed
                                              );
         cout << "[sz=" << n << "] " << flush;
      }
      
      // with a 50% probability, add threadId to the hash table (switchout w/
      // active timers).  But, don't do the add if the **logical** size of the
      // hash table is equal to (hash table max size)
      if (random() % 8 < 4 && tid2data.size() < hash_table_max_num_elems) {
         cout << "add " << (void*)threadId;
         cout << flush;

         // Before doing the add, let's take a peek at the hash table manually, and
         // determine a couple of things: (a) whether numelems is at maximum, and
         // (b) whether the removed flag was set on this element.
         pdvector<uint32_t> buffer;
         hash_table *theHashTablePtr = readHashTable(hashtable_kerninstdAddr,
                                                     buffer);
         n = theHashTablePtr->num_elems;

         bool itemExistedAndWasRemoved = false;
         for (unsigned lcv=0; lcv < n; ++lcv) {
            const hash_table_elem &e = theHashTablePtr->all_elems[lcv];
            if (e.the_key == threadId && e.removed) {
               itemExistedAndWasRemoved = true;
               break;
            }
         }

         const bool packWillBeNeeded = (n==hash_table_max_num_elems &&
                                        !itemExistedAndWasRemoved);

         tempBufferEmitter em(kerninstdABI);
         emit_call_to_hashtable_add(em,
                                    threadId,
                                    threadId, // data
                                    hashtable_add_kerninstdAddr,
                                    true, // assert success
                                    false // do not assert failure
            );
         em.complete();
         theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(em);

         // add to the shadow dictionary, too:
         tid2data.set(threadId, threadId);

         // If we had to pack (pre-add), then we can assert no removed items, and
         // can assert that hash table's size which includes removed items) will equal
         // the logical size, tid2data.size().

         n=read_and_verify_hashtable_validity(hashtable_kerninstdAddr, tid2data,
                                              packWillBeNeeded);

         if (packWillBeNeeded)
            assert(n == tid2data.size());

         cout << "[sz=" << n << "] " << flush;
      }
      cout << endl;
   }

   cout << "second hash table test passed OK" << endl;

   // verify contents with the shadow dictionary:
   cout << flush;
   read_and_verify_hashtable_validity(hashtable_kerninstdAddr, tid2data,
                                      false // don't assert nothing removed
                                      );

   // clean up

   theGlobalInstrumenter->removeDownloadedToKerninstd(hashtable_remove_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(hashtable_pack_reqid);
   theGlobalInstrumenter->removeDownloadedToKerninstd(hashtable_add_reqid);

   theGlobalInstrumenter->freeMappedKernelMemory(hashTableAddrs.first,
                                                 hashTableAddrs.second,
                                                 hash_table_nbytes);
   
   cout << "hash table tests passed OK" << endl;
}

int test_virtualization_all_vtimers() {
   cout << "test virtualization all_vtimers: welcome" << endl;

   vtimerMgr::test_all_vtimers_stuff();
   
   cout << "test virtualization all_vtimers: done" << endl;

   return 0;
}

int test_virtualization_stacklist() {
   cout << "test virtualization stacklist: welcome" << endl;

   vtimerMgr::test_stacklist_stuff();
   
   cout << "test virtualization stacklist: done" << endl;
   return 0;
}

int test_virtualization_hashtable() {
   cout << "test virtualization hashtable: welcome" << endl;

   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   const sparc_reg_set availRegs =
      theGlobalInstrumenter->getAvailRegsForDownloadedToKerninstdCode(true);
         // true --> ignore clientId param

   vtimerMgr::test_hashtable_stuff(availRegs, kerninstdABI);

   cout << "test virtualization hashtable: done" << endl;

   return 0;
}

int test_virtualization_full_ensemble() {
   cout << "test virtualization full ensemble: welcome" << endl;

   const sparc_reg_set availRegs =
      theGlobalInstrumenter->getAvailRegsForDownloadedToKerninstdCode(true);
      // true --> ignore clientId param
   
   vtimerMgr::test_virtualization_full_ensemble(availRegs, // at switchout
                                                availRegs // at switchin
                                                );

   cout << "test virtualization full ensemble: done" << endl;

   return 0;
}
