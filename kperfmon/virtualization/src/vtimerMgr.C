// vtimerMgr.C

#include "util/h/out_streams.h"
#include "vtimerMgr.h"
#include "instrumenter.h"
#include "moduleMgr.h"
#include "machineInfo.h"

#ifdef sparc_sun_solaris2_7
#include "all_vtimers.h"
#include "stacklist.h"
#include "hash_table.h"
#include "stacklist_emit.h"
#include "hash_table_emit.h"
#include "cswitchout_emit.h"
#include "cswitchin_emit.h"
#include "vtimerSupport.h"
#include "globalLock.h"
#endif

extern instrumenter *theGlobalInstrumenter;
extern moduleMgr *global_moduleMgr;
extern machineInfo *theGlobalKerninstdMachineInfo;

extern bool daemonIsSameEndian(const machineInfo &/*mi*/);

vtimerMgr::vtimerMgr(traceBuffer *iTraceBuffer,
                     unsigned iswitchout_begin_traceop,
                     unsigned iswitchout_finish_traceop,
                     unsigned iswitchin_begin_traceop,
                     unsigned iswitchin_finish_traceop)
{
   numReferences = 0;

   freeRegsAtSwitchOut = regset_t::getRegSet(regset_t::empty);
   freeRegsAtSwitchIn = regset_t::getRegSet(regset_t::empty);
   
#ifdef sparc_sun_solaris2_7
   howToStopRoutineFnRegInfo = NULL;
   howToRestartRoutineFnRegInfo = NULL;

   // stack list stuff:
   stacklist_kernelAddr = 0;
   downloaded_stacklist_get_reqid = downloaded_stacklist_free_reqid = UINT_MAX;
   stacklist_get_kernelAddr = stacklist_free_kernelAddr = 0;

   // hash table stuff:
   hashtable_kernelAddr = 0;
   hashtable_pack_reqid = hashtable_add_reqid =
       hashtable_remove_reqid = UINT_MAX;
   hashtable_pack_kernelAddr = hashtable_add_kernelAddr =
      hashtable_remove_kernelAddr = 0;

   // cswitchout & in stuff:
   cswitchout_instrumentation_reqid = cswitchin_instrumentation_reqid = UINT_MAX;
   cswitchout_instrumentation_kernelAddr = cswitchin_instrumentation_kernelAddr = 0;

   // cswitch splice points:
   cswitchout_splicepoint_reqids.clear();
   cswitchin_splicepoint_reqids.clear();

   assert(!installedStacklistHeapStuff());
   assert(!installedHashTableStructure());
   assert(!installedHashTableCode());
   assert(!installedCSwitchOutHandlerDownloadedStuff());
   assert(!installedCSwitchInHandlerDownloadedStuff());

#endif // sparc_solaris

   theTraceBuffer = iTraceBuffer; // could be NULL, in which case we won't trace
   switchout_begin_traceop = iswitchout_begin_traceop;
   switchout_finish_traceop = iswitchout_finish_traceop;
   switchin_begin_traceop = iswitchin_begin_traceop;
   switchin_finish_traceop = iswitchin_finish_traceop;
}

vtimerMgr::~vtimerMgr() {
   theTraceBuffer = NULL; // we don't call delete, since we didn't call new either

#ifdef sparc_sun_solaris2_7   
   assert(!installedStacklistHeapStuff());
   assert(!installedHashTableStructure());
   assert(!installedHashTableCode());
   assert(!installedCSwitchOutHandlerDownloadedStuff());
   assert(!installedCSwitchInHandlerDownloadedStuff());
#endif

   if(freeRegsAtSwitchOut) {
      delete freeRegsAtSwitchOut;
      freeRegsAtSwitchOut = NULL;
   }
   if(freeRegsAtSwitchIn) {
      delete freeRegsAtSwitchIn;
      freeRegsAtSwitchIn = NULL;
   }
}

#ifdef sparc_sun_solaris2_7
//---------------------------------------------------------------------------
//----------------------- Stacklist Heap Stuff ------------------------------
//---------------------------------------------------------------------------

bool vtimerMgr::installedStacklistHeapStuff() const {
   if (stacklist_kernelAddr == 0 &&
       downloaded_stacklist_get_reqid == UINT_MAX &&
       downloaded_stacklist_free_reqid == UINT_MAX &&
       stacklist_get_kernelAddr == 0 && stacklist_free_kernelAddr == 0)
      return false;
   else if (stacklist_kernelAddr != 0 &&
            downloaded_stacklist_get_reqid != UINT_MAX &&
            downloaded_stacklist_free_reqid != UINT_MAX &&
            stacklist_get_kernelAddr != 0 && stacklist_free_kernelAddr != 0)
      return true;
   else
      assert(false);
}

void vtimerMgr::installStackListHeapAndRoutines() {
   // STEP 1: allocate stacklist in the kernel, and mapped into kerninstd, too.
   assert(!installedStacklistHeapStuff());

   dout << "About to allocate stacklist heap: "
        << stacklist_nbytes << " bytes" << endl;
   
   std::pair<kptr_t, dptr_t> stacklist_addrs =
      theGlobalInstrumenter->allocateMappedKernelMemory(stacklist_nbytes, false);
      // false --> won't contain important code; don't try to allocate w/in nucleus text

   stacklist_kernelAddr = stacklist_addrs.first;

   dout << "Allocated stacklist: kerneladdr="
        << addr2hex(stacklist_kernelAddr)
        << " kerninstdaddr="
        << addr2hex(stacklist_addrs.second) << endl;

   // STEP 2) Initialize the already-allocated stacklist by downloading & executing
   // "stacklist_initialize" in kerninstd.

   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   sparc_tempBufferEmitter init_em(kerninstdABI);
   emit_stacklist_initialize(init_em, stacklist_kernelAddr, stacklist_addrs.second);
      // we need to pass both the kernel and kerninstd addresses for proper
      // initialization
   init_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_em);

   if (daemonIsSameEndian(*theGlobalKerninstdMachineInfo)) {
       // assert that the initialization worked as expected
       pdvector<uint32_t> raw_data = theGlobalInstrumenter->
	   peek_kerninstd_contig(stacklist_addrs.second, stacklist_nbytes);
       stacklist *theStackListPtr = (stacklist*)raw_data.begin();
       assert(theStackListPtr->head.ptr == stacklist_kernelAddr);
       for (unsigned lcv=0; lcv < max_num_threads-1; ++lcv)
	   assert(theStackListPtr->theheap[lcv].next.ptr == 
		  (stacklist_kernelAddr + (lcv+1)*stacklistitem_nbytes));
       assert(theStackListPtr->theheap[max_num_threads-1].next.ptr == 0);
   }

   // STEP 3) download stacklist_get() and stacklist_free() into the kernel (not
   //         into kerninstd)

   const abi &kernelABI = theGlobalInstrumenter->getKernelABI();
   sparc_tempBufferEmitter get_em(kernelABI);
   emit_stacklist_get(get_em);
   get_em.complete();
   downloaded_stacklist_get_reqid = theGlobalInstrumenter->downloadToKernel(get_em,
                                                                            true);
   std::pair<pdvector<kptr_t>, unsigned> theAddrs = theGlobalInstrumenter->
      queryDownloadedToKernelAddresses1Fn(downloaded_stacklist_get_reqid);
   stacklist_get_kernelAddr = theAddrs.first[theAddrs.second];
   dout << "stacklist_get() installed at kernel addr "
        << addr2hex(stacklist_get_kernelAddr) << endl;

   sparc_tempBufferEmitter free_em(kernelABI);
   emit_stacklist_free(free_em);
   free_em.complete();
   downloaded_stacklist_free_reqid = theGlobalInstrumenter->downloadToKernel(free_em,
                                                                             true);
   theAddrs = theGlobalInstrumenter->
      queryDownloadedToKernelAddresses1Fn(downloaded_stacklist_free_reqid);
   stacklist_free_kernelAddr = theAddrs.first[theAddrs.second];
   dout << "stacklist_free() installed at kernel addr "
        << addr2hex(stacklist_free_kernelAddr) << endl;

   assert(installedStacklistHeapStuff());
}

void vtimerMgr::uninstallStackListHeapAndRoutines() {
   assert(installedStacklistHeapStuff());

   theGlobalInstrumenter->removeDownloadedToKernel(downloaded_stacklist_free_reqid);
   downloaded_stacklist_free_reqid = UINT_MAX;
   stacklist_free_kernelAddr = 0;
   
   theGlobalInstrumenter->removeDownloadedToKernel(downloaded_stacklist_get_reqid);
   downloaded_stacklist_get_reqid = UINT_MAX;
   stacklist_get_kernelAddr = 0;

   theGlobalInstrumenter->freeMappedKernelMemory(stacklist_kernelAddr,
                                                 stacklist_nbytes);
   stacklist_kernelAddr = 0;
   
   assert(!installedStacklistHeapStuff());
}


//------------------------------------------------------------------------
//-------------------- Hash Table Stuff ----------------------------------
//------------------------------------------------------------------------

bool vtimerMgr::installedHashTableStructure() const {
   if (hashtable_kernelAddr == 0)
      return false;
   else
      return true;
}

bool vtimerMgr::installedHashTableCode() const {
   if (hashtable_pack_reqid == UINT_MAX &&
       hashtable_pack_kernelAddr == 0 &&
       hashtable_add_reqid == UINT_MAX &&
       hashtable_add_kernelAddr == 0 &&
       hashtable_remove_reqid == UINT_MAX &&
       hashtable_remove_kernelAddr == 0)
      return false;
   else if (hashtable_pack_reqid != UINT_MAX &&
            hashtable_pack_kernelAddr != 0 &&
            hashtable_add_reqid != UINT_MAX &&
            hashtable_add_kernelAddr != 0 &&
            hashtable_remove_reqid != UINT_MAX &&
            hashtable_remove_kernelAddr != 0)
      return true;
   else
      assert(false);
}

void vtimerMgr::
installHashTableStructure() {
   // STEP 1: allocate the hash table in the kernel.  Map it into kerninstd, too.

   assert(!installedHashTableStructure());

   std::pair<kptr_t, dptr_t> hashTableAddrs =
      theGlobalInstrumenter->allocateMappedKernelMemory(hash_table_nbytes, false);
      // false --> don't try to allocate w/in nucleus text

   hashtable_kernelAddr = hashTableAddrs.first;
   dout << "allocated hash table at kernel "
        << addr2hex(hashtable_kernelAddr) << " and kerninstd "
        << addr2hex(hashTableAddrs.second) << endl;

   // STEP 2: initialize the hash table (to "empty") by downloading
   // hash_table_initialize() into kerninstd (not the kernel) and executing it once
   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();

   sparc_tempBufferEmitter init_em(kerninstdABI);
   emit_initialize_hashtable(init_em, hashTableAddrs.second);
   init_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_em);

   assert(installedHashTableStructure());
}

void vtimerMgr::uninstallHashTableAndRoutines() {
   assert(installedHashTableStructure());
   assert(installedHashTableCode());

   theGlobalInstrumenter->removeDownloadedToKernel(hashtable_remove_reqid);
   hashtable_remove_reqid = UINT_MAX;
   hashtable_remove_kernelAddr = 0;
   
   theGlobalInstrumenter->removeDownloadedToKernel(hashtable_add_reqid);
   hashtable_add_reqid = UINT_MAX;
   hashtable_add_kernelAddr = 0;

   theGlobalInstrumenter->removeDownloadedToKernel(hashtable_pack_reqid);
   hashtable_pack_reqid = UINT_MAX;
   hashtable_pack_kernelAddr = 0;

   theGlobalInstrumenter->freeMappedKernelMemory(hashtable_kernelAddr,
                                                 hash_table_nbytes);
   hashtable_kernelAddr = 0;

   assert(!installedHashTableStructure());
   assert(!installedHashTableCode());
}

//----------------------------------------------------------------------

static sparc_reg_set regset_intersection(const pdvector<kptr_t> &points) {
   sparc_reg_set result(sparc_reg_set::allIntRegs);
   
   for (pdvector<kptr_t>::const_iterator iter = points.begin();
        iter != points.end(); ++iter) {
      const kptr_t addr = *iter;

      const regset_t *this_point_result = theGlobalInstrumenter->
         getFreeRegsAtPoint(addr, false); // false --> preinsn, not prereturn
      
      //      cout << "regset_intersection low-level: free regs at point "
      //           << addr2hex(addr) << " are reported as:" << endl;
      //      cout << this_point_result.sprintf(false) << endl;

      result &= *(const sparc_reg_set*)this_point_result;

      assert(this_point_result->exists(result));
      delete this_point_result;
   }

   return result;
}

#endif // sparc_solaris

// --------------------------------------------------------------------------
// --------------- Virtualization: generic structures setup -----------------
// --------------- Step 1 of 5 for virtualization (see .h file) -------------
// --------------------------------------------------------------------------

std::pair<unsigned,unsigned> vtimerMgr::referenceVirtualizationCode(
    kptr_t all_vtimers_kernelAddr) {
   // If reference count was zero, installs a lot of cswitch-related stuff
   // into the kernel (cswitchout, cswitchin, blank hash table, stacklist heap)
   // This is the routine to call first, in part because it initializes
   // necessary private vrbles that keep track of which regs are free for
   // the metric-specific how-to-stop and how-to-restart routines.

   const bool installing = (numReferences++ == 0);
   if (installing) {
#ifdef sparc_sun_solaris2_7
      create_global_lock();
      assert(!installedStacklistHeapStuff());
      assert(!installedHashTableStructure());
      assert(!installedHashTableCode());
      assert(!installedCSwitchOutHandlerDownloadedStuff());
      assert(!installedCSwitchInHandlerDownloadedStuff());
#endif

      std::pair< std::pair< pdvector<kptr_t>, pdvector<kptr_t> >, // category 1
         std::pair< pdvector<kptr_t>, pdvector<kptr_t> > > // category 2
         switchout_and_switchin_splice_points = obtainCSwitchOutAndInSplicePoints();
      cswitchout_cat1_points = switchout_and_switchin_splice_points.first.first;
      cswitchin_cat1_points = switchout_and_switchin_splice_points.first.second;
      cswitchout_cat2_points = switchout_and_switchin_splice_points.second.first;
      cswitchin_cat2_points = switchout_and_switchin_splice_points.second.second;

#ifdef sparc_sun_solaris2_7
      // Find registers available to the context switch code. We assume
      // that we reach it with the call instruction, which is
      // wrapped with save/restore (guaranteed by the API). Furthermore,
      // the API saves/restores all G registers around the call.
      const sparc_reg_set freeRegsAtSwitchOut1 = 
	  regset_intersection(cswitchout_cat1_points);
      const sparc_reg_set freeRegsAtSwitchOut2 = 
	  regset_intersection(cswitchout_cat2_points);
      *(sparc_reg_set*)freeRegsAtSwitchOut =
	  sparc_reg_set(sparc_reg_set::save,
		       freeRegsAtSwitchOut1 & freeRegsAtSwitchOut2, true) +
	  sparc_reg_set::allGs - sparc_reg::fp - sparc_reg::sp -
	  sparc_reg::o7 - sparc_reg::g0 - sparc_reg::g7;
      
      const sparc_reg_set freeRegsAtSwitchIn1 = 
	  regset_intersection(cswitchin_cat1_points);
      const sparc_reg_set freeRegsAtSwitchIn2 = 
	  regset_intersection(cswitchin_cat2_points);
      *(sparc_reg_set*)freeRegsAtSwitchIn = 
	  sparc_reg_set(sparc_reg_set::save,
			freeRegsAtSwitchIn1 & freeRegsAtSwitchIn2, true) +
	  sparc_reg_set::allGs - sparc_reg::fp - sparc_reg::sp -
	  sparc_reg::o7 - sparc_reg::g0 - sparc_reg::g7;

      dout << "reported free regs at switch-out:" << endl;
      dout << "   " << freeRegsAtSwitchOut->disassx() << endl;
      dout << "reported free regs at switch-in:" << endl;
      dout << "   " << freeRegsAtSwitchIn->disassx() << endl;
      
      // must install stack list stuff before cswitch handlers, because installing
      // cswitch handlers requires, e.g. stacklist_kernelAddr, to be defined.
      installStackListHeapAndRoutines();

      // install & initialize the hash table now:
      installHashTableStructure();

      // install cswitchout routines & its helpers (e.g. hash table add/pack)
      // (doesn't splice them into the kernel yet, though)
      installContextSwitchOutRoutineAndHelpers(*freeRegsAtSwitchOut,
					       all_vtimers_kernelAddr);

      // install cswitchin routines & its helpers (e.g. hash table remove)
      // (doesn't splice them into the kernel yet, though)
      installContextSwitchInRoutineAndHelpers(*freeRegsAtSwitchIn);
   }
   
   assert(installedStacklistHeapStuff());
   assert(installedHashTableStructure());
   assert(installedHashTableCode());
   assert(installedCSwitchOutHandlerDownloadedStuff());
   assert(installedCSwitchInHandlerDownloadedStuff());

#endif // sparc_solaris

   
   return std::pair<unsigned,unsigned>(cswitchin_instrumentation_reqid,
				  cswitchout_instrumentation_reqid);
}

// --------------------------------------------------------------------------
// --------------- UnVirtualization: generic structures teardown ------------
// --------------- Step 5 of 5 for unvirtualization (see .h file) -----------
// --------------------------------------------------------------------------

void vtimerMgr::dereferenceVirtualizationCode() {
   const bool uninstalling = (--numReferences == 0);
   if (uninstalling) {
      removeContextSwitchHandlers();
      uninstallHashTableAndRoutines();
      uninstallStackListHeapAndRoutines();
      delete_global_lock();

      assert(!installedStacklistHeapStuff());
      assert(!installedHashTableStructure());
      assert(!installedHashTableCode());
      assert(!installedCSwitchOutHandlerDownloadedStuff());
      assert(!installedCSwitchInHandlerDownloadedStuff());
   }
   else {
      // not uninstalling
      assert(installedStacklistHeapStuff());
      assert(installedHashTableStructure());
      assert(installedHashTableCode());
      assert(installedCSwitchOutHandlerDownloadedStuff());
      assert(installedCSwitchInHandlerDownloadedStuff());
   }
}

// --------------------------------------------------------------------------------
// -------------------------- Context Switch Handlers Stuff -----------------------
// --------------------------------------------------------------------------------

bool vtimerMgr::installedCSwitchOutHandlerDownloadedStuff() const {
   if (cswitchout_instrumentation_reqid == UINT_MAX &&
       cswitchout_instrumentation_kernelAddr == 0 &&
       howToStopRoutineFnRegInfo == NULL)
      return false;
   else if (cswitchout_instrumentation_reqid != UINT_MAX &&
            cswitchout_instrumentation_kernelAddr != 0 &&
            howToStopRoutineFnRegInfo != NULL)
      return true;
   else
      assert(false);
}

bool vtimerMgr::installedCSwitchInHandlerDownloadedStuff() const {
   if (cswitchin_instrumentation_reqid == UINT_MAX &&
       cswitchin_instrumentation_kernelAddr == 0 &&
       howToRestartRoutineFnRegInfo == NULL)
      return false;
   else if (cswitchin_instrumentation_reqid != UINT_MAX &&
            cswitchin_instrumentation_kernelAddr != 0 &&
            howToRestartRoutineFnRegInfo != NULL)
      return true;
   else
      assert(false);
}

void vtimerMgr::
installContextSwitchOutRoutineAndHelpers(const regset_t &avail_regs_at_switchout, kptr_t all_vtimers_kernelAddr) {
   // installs cswitchout routine and its helpers (hash table add/pack), but doesn't
   // yet splice them, so they're not yet invoked.

   assert(installedHashTableStructure());
   assert(stacklist_get_kernelAddr != 0);
   assert(hashtable_add_kernelAddr == 0);
   assert(hashtable_pack_kernelAddr == 0);
   assert(!installedCSwitchOutHandlerDownloadedStuff());

   const abi &kernelABI = theGlobalInstrumenter->getKernelABI();
   
#ifdef sparc_sun_solaris2_7
   sparc_tempBufferEmitter out_em(kernelABI);
#elif defined(i386_unknown_linux2_4)
   x86_tempBufferEmitter out_em(kernelABI);
#endif
   const std::pair<fnRegInfo, fnRegInfo> helperFnRegInfos =
      emit_cswitchout_instrumentation(out_em,
                                      false, // not kerninstd
                                      fnRegInfo(false, // not inlined
                                                fnRegInfo::regsAvailAtCallTo,
                                                avail_regs_at_switchout,
                                                fnRegInfo::blankParamInfo,
                                                // no args
                                                sparc_reg::g0
                                                // no return value
                                                ),
                                      theTraceBuffer, // could be null
                                      switchout_begin_traceop,
                                      switchout_finish_traceop,
                                      all_vtimers_kernelAddr,
                                      stacklist_kernelAddr,
                                      stacklist_get_kernelAddr,
                                      stacklist_free_kernelAddr);
   assert(this->howToStopRoutineFnRegInfo == NULL);
   this->howToStopRoutineFnRegInfo = new fnRegInfo(helperFnRegInfos.first);

   const fnRegInfo &hashTableAddRoutineFnRegInfo = helperFnRegInfos.second;

   // Now emit the hash table add/pack routines.
#ifdef sparc_sun_solaris2_7
   sparc_tempBufferEmitter hashtable_add_em(kernelABI);
#elif defined(i386_unknown_linux2_4)
   x86_tempBufferEmitter hashtable_add_em(kernelABI);
#endif
   const fnRegInfo hashTablePackFnRegInfo =
      emit_hashtable_add(hashtable_add_em,
                         hashTableAddRoutineFnRegInfo,
                         hashtable_kernelAddr);

#ifdef sparc_sun_solaris2_7
   sparc_tempBufferEmitter hashtable_pack_em(kernelABI);
#elif defined(i386_unknown_linux2_4)
   x86_tempBufferEmitter hashtable_pack_em(kernelABI);
#endif
   emit_hashtable_pack(hashtable_pack_em,
                       hashtable_kernelAddr,
                       hashTablePackFnRegInfo);
   hashtable_pack_em.complete();
   hashtable_pack_reqid = theGlobalInstrumenter->downloadToKernel(hashtable_pack_em,
                                                                  true);
   std::pair<pdvector<kptr_t>,unsigned> theAddrs = theGlobalInstrumenter->queryDownloadedToKernelAddresses1Fn(hashtable_pack_reqid);
   hashtable_pack_kernelAddr = theAddrs.first[theAddrs.second];

   dout << "hashtable_pack() installed at kernel "
        << addr2hex(hashtable_pack_kernelAddr) << endl;

   // pack's address is finally known, so we're about ready to complete add:
   hashtable_add_em.makeSymAddrKnown("hashTablePackAddr", 
				     hashtable_pack_kernelAddr);
   hashtable_add_em.complete();
   hashtable_add_reqid = theGlobalInstrumenter->downloadToKernel(hashtable_add_em,
                                                                 true);
   theAddrs = theGlobalInstrumenter->queryDownloadedToKernelAddresses1Fn(hashtable_add_reqid);
   hashtable_add_kernelAddr = theAddrs.first[theAddrs.second];

   dout << "hashtable_add() installed at kernel "
        << addr2hex(hashtable_add_kernelAddr) << endl;

   // add's address is finally known, so we're about ready to complete cswitchout:
   out_em.makeSymAddrKnown("hashTableAddAddr", hashtable_add_kernelAddr);
   out_em.complete();
   //cswitchout_instrumentation_reqid = theGlobalInstrumenter->downloadToKernelAndParse(out_em, "cswitchout", "!", true);
   cswitchout_instrumentation_reqid = theGlobalInstrumenter->downloadToKernel(out_em,
                                                                              true);
   theAddrs = theGlobalInstrumenter->
      queryDownloadedToKernelAddresses1Fn(cswitchout_instrumentation_reqid);
   cswitchout_instrumentation_kernelAddr = theAddrs.first[theAddrs.second];

   dout << "cswitchout routine has been installed at kernel addr "
        << addr2hex(cswitchout_instrumentation_kernelAddr) << endl;

   assert(installedCSwitchOutHandlerDownloadedStuff());
   assert(hashtable_add_kernelAddr != 0);
   assert(hashtable_pack_kernelAddr != 0);
}

void vtimerMgr::
installContextSwitchInRoutineAndHelpers(const regset_t &avail_regs_at_switchin) {
   // installs cswitchin routine and its helpers (hash table remove) but doesn't
   // yet splice them, so they're not yet invoked.

   assert(installedHashTableStructure());
   assert(stacklist_free_kernelAddr != 0);
   assert(hashtable_remove_kernelAddr == 0);
   assert(!installedCSwitchInHandlerDownloadedStuff());

   const abi &kernelABI = theGlobalInstrumenter->getKernelABI();

#ifdef sparc_sun_solaris2_7
   sparc_tempBufferEmitter in_em(kernelABI);
#elif defined(i386_unknown_linux2_4)
   x86_tempBufferEmitter in_em(kernelABI);
#endif
   const std::pair<fnRegInfo, fnRegInfo> helperRoutineFnRegInfos =
      emit_cswitchin_instrumentation(in_em,
                                     false, // not just kerninstd
                                     fnRegInfo(false, // not inlined
                                               fnRegInfo::regsAvailAtCallTo,
                                               avail_regs_at_switchin,
                                               fnRegInfo::blankParamInfo,
                                               // no args
                                               sparc_reg::g0
                                               // no return value
                                               ),
                                     theTraceBuffer, // may be NULL
                                     switchin_begin_traceop,
                                     switchin_finish_traceop,
                                     stacklist_kernelAddr,
                                     stacklist_free_kernelAddr);

   assert(this->howToRestartRoutineFnRegInfo == NULL);
   this->howToRestartRoutineFnRegInfo = new fnRegInfo(helperRoutineFnRegInfos.second);
   
   const fnRegInfo &hashTableRemoveFnRegInfo = helperRoutineFnRegInfos.first;

#ifdef sparc_sun_solaris2_7
   sparc_tempBufferEmitter remove_em(kernelABI);
#elif defined(i386_unknown_linux2_4)
   x86_tempBufferEmitter remove_em(kernelABI);
#endif
   emit_hashtable_remove(remove_em,
                         hashtable_kernelAddr,
                         hashTableRemoveFnRegInfo);
   remove_em.complete();
   hashtable_remove_reqid = theGlobalInstrumenter->downloadToKernel(remove_em, true);
   std::pair<pdvector<kptr_t>,unsigned> theAddrs = theGlobalInstrumenter->queryDownloadedToKernelAddresses1Fn(hashtable_remove_reqid);
   hashtable_remove_kernelAddr = theAddrs.first[theAddrs.second];

   dout << "hashtable_remove() installed at kernel "
        << addr2hex(hashtable_remove_kernelAddr) << endl;

   // Now that remove has been downloaded, we can complete cswitchin:
   in_em.makeSymAddrKnown("hashTableRemoveAddr", hashtable_remove_kernelAddr);
   in_em.complete();
   //cswitchin_instrumentation_reqid = theGlobalInstrumenter->downloadToKernelAndParse(in_em, "cswitchin", "!", true);
   cswitchin_instrumentation_reqid = theGlobalInstrumenter->downloadToKernel(in_em, true);
   theAddrs = theGlobalInstrumenter->
      queryDownloadedToKernelAddresses1Fn(cswitchin_instrumentation_reqid);
   cswitchin_instrumentation_kernelAddr = theAddrs.first[theAddrs.second];

   dout << "cswitchin routine has been installed at kernel addr "
        << addr2hex(cswitchin_instrumentation_kernelAddr) << endl;

   assert(hashtable_remove_kernelAddr != 0);
   assert(installedCSwitchInHandlerDownloadedStuff());
}

void vtimerMgr::removeContextSwitchHandlers() {
   assert(installedCSwitchOutHandlerDownloadedStuff());
   assert(installedCSwitchInHandlerDownloadedStuff());

   theGlobalInstrumenter->removeDownloadedToKernel(cswitchout_instrumentation_reqid);
   theGlobalInstrumenter->removeDownloadedToKernel(cswitchin_instrumentation_reqid);

   cswitchout_instrumentation_reqid = cswitchin_instrumentation_reqid = UINT_MAX;
   cswitchout_instrumentation_kernelAddr = cswitchin_instrumentation_kernelAddr = 0;

   delete howToStopRoutineFnRegInfo; // harmless if already NULL
   howToStopRoutineFnRegInfo = NULL;
   
   delete howToRestartRoutineFnRegInfo; // harmless if already NULL
   howToRestartRoutineFnRegInfo = NULL;

   assert(!installedCSwitchOutHandlerDownloadedStuff());
   assert(!installedCSwitchInHandlerDownloadedStuff());
}

// Returns registers available for a vrestart routine to use
const regset_t& vtimerMgr::getVRestartAvailRegs() const
{
    return howToRestartRoutineFnRegInfo->getAvailRegsForFn();
}

// Returns registers available for a vstop routine to use
const regset_t& vtimerMgr::getVStopAvailRegs() const
{
    return howToStopRoutineFnRegInfo->getAvailRegsForFn();
}

// vrestart routines expect their input arguments in
// the following registers
unsigned vtimerMgr::getVRestartAddrReg() const
{
    return howToRestartRoutineFnRegInfo->getParamRegByName("vtimer_addr_reg").getIntNum();
}

unsigned vtimerMgr::getAuxdataReg() const
{
    return howToRestartRoutineFnRegInfo->getParamRegByName("vtimer_auxdata_reg").getIntNum();
}

// vstop routines expect their input arguments in
// the following registers
unsigned vtimerMgr::getVStopAddrReg() const
{
    return howToStopRoutineFnRegInfo->getParamRegByName("vtimer_addr_reg").getIntNum();
}

unsigned vtimerMgr::getStartReg() const
{
    return howToStopRoutineFnRegInfo->getParamRegByName("vtimer_start_reg").getIntNum();
}

unsigned vtimerMgr::getIterReg() const
{
    return howToStopRoutineFnRegInfo->getParamRegByName("vtimer_iter_reg").getIntNum();
}
