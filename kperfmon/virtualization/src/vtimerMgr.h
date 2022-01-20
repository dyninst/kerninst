// vtimerMgr.h

#ifndef _VTIMER_MGR_H_
#define _VTIMER_MGR_H_

#include "util/h/kdrvtypes.h"
#include "util/h/hashFns.h"
#include "traceBuffer.h"
#include "fnRegInfo.h"
#include <utility> // STL pair

class vtimerMgr {
 private:
   unsigned numReferences;

   // keeping track of cswitchout and cswitchin points:
   pdvector<kptr_t> cswitchout_cat1_points; // category 1: where enough free regs
   pdvector<kptr_t> cswitchout_cat2_points; // category 2: where not enough free regs
   regset_t *freeRegsAtSwitchOut;
      // intersection of category 1 & category 2 free regs, with a save
      // already applied for the latter.
   
   pdvector<kptr_t> cswitchin_cat1_points; // category 1: where enough free regs
   pdvector<kptr_t> cswitchin_cat2_points; // category 2: where not enough free regs
   regset_t *freeRegsAtSwitchIn;
      // intersection of category 1 & category 2 free regs, with a save
      // already applied for the latter.
   
#ifdef sparc_sun_solaris2_7
   // "stacklist" heap containing a bunch of vector-of-vtimerptrs that we can
   // allocate, at the granularity of whole vectors.  Note that we keep info
   // for get and free, but not initialize and destroy, since the latter group, when
   // needed, are created & downloaded into kerninstd & fried in one fell swoop.
   kptr_t stacklist_kernelAddr;
   unsigned downloaded_stacklist_get_reqid, downloaded_stacklist_free_reqid;
   kptr_t stacklist_get_kernelAddr, stacklist_free_kernelAddr;
   bool installedStacklistHeapStuff() const;
   void installStackListHeapAndRoutines();
   void uninstallStackListHeapAndRoutines();
   
   // "hash table"
   kptr_t hashtable_kernelAddr;
   unsigned hashtable_pack_reqid, hashtable_add_reqid, hashtable_remove_reqid;
   kptr_t hashtable_pack_kernelAddr, hashtable_add_kernelAddr, 
            hashtable_remove_kernelAddr;
   bool installedHashTableStructure() const;
   bool installedHashTableCode() const;
   void installHashTableStructure();
   void uninstallHashTableAndRoutines();

   // cswitchout and cswitchin instrumentation -- downloaded to kernel
   // FAQ: why are these routines downloaded instead of turned into a snippet that's
   // spliced in?  ANS: to avoid code duplications, since there are on the order of
   // a half dozen splice points, and we don't want this code to be duplicated
   // 6 times.  Not that it would be a tragedy, and it would improve performance a
   // a touch.  So perhaps in the future these'll be directly spliced in, not
   // downloaded and then called via trivual spliced in code.
   unsigned cswitchout_instrumentation_reqid, cswitchin_instrumentation_reqid;
   kptr_t cswitchout_instrumentation_kernelAddr,
            cswitchin_instrumentation_kernelAddr;
   bool installedCSwitchOutHandlerDownloadedStuff() const;
   bool installedCSwitchInHandlerDownloadedStuff() const;

   // Information for the various how-to-stop and how-to-restart routines
   // (needed so that these various routines can emit code that safely use registers)
   fnRegInfo *howToStopRoutineFnRegInfo; // a ptr since undefined in the ctor
   fnRegInfo *howToRestartRoutineFnRegInfo; // a ptr since undefined in the ctor
   
//     void installStopAndRestartForPic0VTime();
//     void uninstallStopAndRestartForPic0VTime();
//     void installStopAndRestartForPic0CountedVTime();
//     void uninstallStopAndRestartForPic0CountedVTime();

   void installContextSwitchOutRoutineAndHelpers(const regset_t &avail_regs_at_switchout, kptr_t all_vtimers_kernelAddr);
   void installContextSwitchInRoutineAndHelpers(const regset_t &avail_regs_at_switchin);
   
   void removeContextSwitchHandlers();

#endif // sparc_solaris

   // Trace buffer for interesting events
   traceBuffer *theTraceBuffer; // if NULL then we are not tracing anything
   unsigned switchout_begin_traceop, switchout_finish_traceop;
   unsigned switchin_begin_traceop, switchin_finish_traceop;
      // undefined if not tracing


   // private to ensure not called:
   vtimerMgr(const vtimerMgr &);
   vtimerMgr &operator=(const vtimerMgr &);
   
 public:

   vtimerMgr(traceBuffer *iTraceBuffer,
             unsigned iswitchout_begin_traceop,
             unsigned iswitchout_finish_traceop,
             unsigned iswitchin_begin_traceop,
             unsigned iswitchin_finish_traceop);
      // we don't store either the kernel's or kerninstd's ABI; we can
      // always grab those from theGlobalInstrumenter.
  ~vtimerMgr();

#ifdef sparc_sun_solaris2_7
   // Outside code can call these static routines for torture testing:
   static void test_all_vtimers_stuff();
   static void test_stacklist_stuff();
   static void test_hashtable_stuff(const regset_t &freeRegs,
                                    const abi &kerninstdABi);

   static void test_virtualization_full_ensemble
         (const regset_t &freeRegsAtSwitchOut,
          const regset_t &freeRegsAtSwitchIn);

   // cswitch handler splice points (instrumentation code which basically just
   // forces a call to the above downloaded routines...)
   pdvector<unsigned> cswitchout_splicepoint_reqids;
   pdvector<unsigned> cswitchin_splicepoint_reqids;
#endif

   static std::pair< std::pair< pdvector<kptr_t>, pdvector<kptr_t> >,
                std::pair< pdvector<kptr_t>, pdvector<kptr_t> > >
   obtainCSwitchOutAndInSplicePoints();
      // first: category 1 swtchout & swtchin points
      // second: category 2 swtchout & swtchin points

#ifdef sparc_sun_solaris2_7
   // Returns registers available for a vrestart routine to use
   const regset_t &getVRestartAvailRegs() const;

   // Returns registers available for a vstop routine to use
   const regset_t &getVStopAvailRegs() const;

   // vrestart routines expect their input arguments in
   // the following registers
   unsigned getVRestartAddrReg() const;
   unsigned getAuxdataReg() const;

   // vstop routines expect their input arguments in
   // the following registers
   unsigned getVStopAddrReg() const;
   unsigned getStartReg() const;
   unsigned getIterReg() const;
#endif

   // Here is the order in which virtualization should take place. Note
   // that some of these steps are now implemented in metricVirtualizer
   // 1) Call referenceVirtualizationCode().
   // 2) Call referenceVRoutinesForMetric()
   // 3) Allocate your vtimer and initialize its fields completely, including the
   //    pointers to how-to-stop and how-to-restart.
   // 4) Call virtualizeInitializedTimer() for this vtimer.  Virtualization of the
   //    vtimer will now take place.  Of course, because its present state is "off",
   //    virtualization won't try to do much of anything at this point.
   // 5) Splice the "normal" startVTimer and stopVTimer routines at the desired
   //    kernel function(s).  Everything's running full-tilt now.
   //
   // [Now let's assume that you want to remove/uninstrument this vtimer:]
   // [Note that, as expected, the following ordering is simply the above five
   // steps replayed in reverse order]
   // 1) unsplice the "normal" startVTimer/stopVTimer instrumentation.
   //    It's possible that the vtimer was in a started state when this happened,
   //    so virtualization code may still stop & restart it.
   // 2) call unVirtualizeVTimer(), which will remove it from all_vtimers[].  This
   //    vtimer will no longer be switched out, though it might still be switched in by
   //    virtue of being in the hash table when called.
   //    I BELIEVE THAT THIS CAN BE A SOURCE OF A RACE-CONDITION BUG.  WE NEED TO
   //    TRACK DOWN THIS VTIMER IN ANY HASH TABLE ENTRIES AND REMOVE, IF APPROPRIATE.
   //    NOT YET IMPLEMENTED!  FORTUNATELY, THE COST WOULDN'T BE TOO MUCH OF A BIG
   //    DEAL, SINCE IT ONLY NEEDS TO HAPPEN WHEN UNINSTRUMENTING, A RELATIVELY
   //    RARE OCCURRANCE.
   // 3) Now that neither conventional start/stop routines nor virtualization
   //    will happen for this vtimer, we can really start to tear down its
   //    components.  Deallocate your vtimer at will.
   // 4) call dereferenceVRoutinesForMetric(), which may uninstall
   //    the how-to-stop and how-to-restart routines from the kernel.
   // 5) call dereferenceVirtualizationCode(), which may un-splice and remove
   //    the cswitchout and cswitchin routines from the kernel.
   
   // --------------------------------------------------------------------------
   // --------------- Virtualization: generic structures setup -----------------
   // --------------------------------------------------------------------------
   std::pair<unsigned,unsigned> referenceVirtualizationCode(kptr_t allVTimersAddr);
      // virt'n step 1/5
      // If reference count was zero, installs a lot of cswitch-related stuff
      // into the kernel (cswitchout, cswitchin, blank all_vtimers[], blank
      // hash table, stacklist heap), but does not splice anything or otherwise
      // cause this code to be invoked.  This is the routine to call first, in part
      // because it initializes necessary private vrbles that keep track of which
      // regs are free for the metric-specific how-to-stop and how-to-restart
      // routines. Returns request ids for the switch-in and switch-out
      // routines
   void dereferenceVirtualizationCode(); // un-virt'n step 5/5
      // The opposite of the above routine; de-installs stuff if appropriate.
      // Call this last.
};

extern vtimerMgr *theGlobalVTimerMgr;

#endif
