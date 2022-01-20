// countedVEvents.h
// simple metric for counted, virtualized events, where an "event" is something
// like "machine cycles" or "# of cache misses".
// countedVEvents is just a base class; derived classes are needed to create
// primitives.  This allows much code to be shared.
//
// Compare to class threadEvents, which is similar except (1) it isn't counted, and
// thus isn't recursion safe, and (2) has different units (thread x events instead
// of just events)

#ifndef _COUNTED_VEVENTS_H_
#define _COUNTED_VEVENTS_H_

#include "util/h/kdrvtypes.h"
#include "simpleMetric.h"
#include "kapi.h"

class countedVEvents : public simpleMetric {
 private:
    kapi_hwcounter_kind hwkind;
    mutable kapi_hwcounter_expr rawCtrExpr;

    // Initialize the timer data in the kernel memory
    void initialize_timer(kptr_t kernelAddr,
			  unsigned timer_type,
			  unsigned elemsPerVector,
			  unsigned bytesPerStride,
			  kptr_t stopRoutineKernelAddr,
			  kptr_t restartRoutineKernelAddr) const;

   // prevent copying
   countedVEvents(const countedVEvents &);
   countedVEvents& operator=(const countedVEvents &);
   
 public:
   countedVEvents(unsigned iSimpleMetId, kapi_hwcounter_kind iKind) :
      simpleMetric(iSimpleMetId), hwkind(iKind), rawCtrExpr(iKind) {
   }
   virtual ~countedVEvents() {
   }

   bool canInstantiateFocus(const focus &theFocus) const;
   unsigned getNumValues() const { return 1; }

   kapi_hwcounter_kind getHwCtrKind() const {
       return hwkind;
   }

   const kapi_hwcounter_expr &getRawCtrExpr() const {
       return rawCtrExpr;
   }

   // This routine determines whether to include events that occured 
   // while we were switched out. Wall timers want to include them
   virtual bool includeEventsSwitchedOut() const {
       return false;
   }

#if 0 // See comments in .C for why disabled

   // What is does: emits code to stop a vtimer of this metric (if needed), and
   // also appends the 8 byte {vtimeraddr, auxdata [rcount]} entity to
   // vtimerIter iff stopping. vtimerStart is passed in to aid detection of
   // overflow.
   kapi_snippet generateMetricSpecificVStop(
       const kapi_snippet &vtimer_addr_base,
       const kapi_snippet &vtimer_start,
       const kapi_snippet &vtimer_iter) const;
   // The opposite of the above routine; they work hand-in-hand.
   kapi_snippet generateMetricSpecificVRestart(
       const kapi_snippet &vtimer_addr_base,
       const kapi_snippet &vtimer_auxdata) const;

#endif

   void asyncInstantiate(simpleMetricFocus &,
                         cmfInstantiatePostSmfInstantiation &) const;

   void cleanUpAfterUnsplice(const simpleMetricFocus &smf) const;
      // Typically when a simpleMetricFocus is unspliced, all that's needed is to free
      // the associated data & code snippets.  But we have to do much more:
      // (1) remove vtimer from all_vtimers[] (so cswitch handlers won't try to stop/
      // restart the vtimer any more).
      // (2) decrement a refcount for how-to-stop and how-to-restart routines for
      // this simpleMetric; if now 0, then un-download them from kernel.  (Don't worry,
      // we can be sure they're not in-progress, since they're not interruptable except
      // by trap handlers or other stuff above LOCK_LEVEL.)

   // Get the state of the counters as if the metric has been enabled
   kapi_hwcounter_set queryPostInstrumentationPcr(
       const kapi_hwcounter_set &oldPcrValue, const focus &) const {
       
       kapi_hwcounter_set result = oldPcrValue;
       result.insert(hwkind);

       return result;
   }
   
   // Check if the metric can be enabled without invalidating another one
   bool changeInPcrWouldInvalidateSMF(
       const kapi_hwcounter_set &pendingNewPcrValue, const focus &) const {

       return pendingNewPcrValue.conflictsWith(hwkind);
   }
};

// ********************************************************************************

class countedVCycles : public countedVEvents {
 public:
   countedVCycles(unsigned iSimpleMetricId) :
       countedVEvents(iSimpleMetricId, HWC_TICKS) {
   }
};

class countedWallCycles : public countedVEvents {
 public:
    countedWallCycles(unsigned iSimpleMetricId) : 
	countedVEvents(iSimpleMetricId, HWC_TICKS) {
    }
    bool includeEventsSwitchedOut() const {
	return true;
    }
};



// ***************************************************************************
#ifdef sparc_sun_solaris2_7

class countedDCacheVReads : public countedVEvents {
 public:
   countedDCacheVReads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_DCACHE_VREADS) {
   }
};

// ***************************************************************************

class countedDCacheVWrites : public countedVEvents {
 public:
    countedDCacheVWrites(unsigned iSimpleMetricId) :
	countedVEvents(iSimpleMetricId, HWC_DCACHE_VWRITES) {
    }
};

// ***************************************************************************

class countedDCacheVReadHits : public countedVEvents {
 public:
   countedDCacheVReadHits(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_DCACHE_VREAD_HITS) {
   }
};

// ***************************************************************************

class countedDCacheVWriteHits : public countedVEvents {
 public:
   countedDCacheVWriteHits(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_DCACHE_VWRITE_HITS) {
   }
};

// ***************************************************************************

class countedICacheVRefs : public countedVEvents {
 public:
   countedICacheVRefs(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ICACHE_VREFS) {
   }
};

// ***************************************************************************

class countedICacheVHits : public countedVEvents {
 public:
   countedICacheVHits(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ICACHE_VHITS) {
   }
};

// ***************************************************************************

class countedICacheVStallCycles : public countedVEvents {
 public:
   countedICacheVStallCycles(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ICACHE_STALL_CYCLES) {
   }
};

class countedBranchMispredVStallCycles : public countedVEvents {
 public:
   countedBranchMispredVStallCycles(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_BRANCH_MISPRED_VSTALL_CYCLES) {
   }
};


// ***************************************************************************

class countedECacheVRefs : public countedVEvents {
 public:
   countedECacheVRefs(unsigned iSimpleMetricId) :
       countedVEvents(iSimpleMetricId, HWC_ECACHE_VREFS) {
   }
};

// ***************************************************************************

class countedECacheVHits : public countedVEvents {
 public:
   countedECacheVHits(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ECACHE_VHITS) {
   }
};

// ***************************************************************************

class countedECacheVReadHits : public countedVEvents {
 public:
   countedECacheVReadHits(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ECACHE_VREAD_HITS) {
   }
};

// ***************************************************************************

class countedVInsns : public countedVEvents {
   // #insns is a little unusual, in that it can use %pic0 event 0x1 **or**
   // %pic1 event 0x1.  This simpleMetric is for %pic0; another will be for %pic1
 public:
   countedVInsns(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_VINSNS) {
   }
};

// ***************************************************************************

class countedVInsnsPic1 : public countedVEvents {
   // #insns is a little unusual, in that it can use %pic1 event 0x1 **or**
   // %pic1 event 0x1.  This simpleMetric is for %pic1; above one is for %pic0

 public:
   countedVInsnsPic1(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_VINSNS) {
   }
};

// ***************************************************************************

#elif defined(i386_unknown_linux2_4)

class countedITLBHits : public countedVEvents {
 public:
   countedITLBHits(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ITLB_HIT) {
   }
};

// ***************************************************************************

class countedITLBHitsUC : public countedVEvents {
 public:
   countedITLBHitsUC(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ITLB_UNCACHEABLE_HIT) {
   }
};

// ***************************************************************************

class countedITLBMisses : public countedVEvents {
 public:
   countedITLBMisses(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ITLB_MISS) {
   }
};

// ***************************************************************************

class countedITLBMissPageWalks : public countedVEvents {
 public:
   countedITLBMissPageWalks(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_ITLB_MISS_PAGE_WALK) {
   }
};

// ***************************************************************************

class countedDTLBMissPageWalks : public countedVEvents {
 public:
   countedDTLBMissPageWalks(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_DTLB_MISS_PAGE_WALK) {
   }
};

// ***************************************************************************

class countedDTLBLoadMisses : public countedVEvents {
 public:
   countedDTLBLoadMisses(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_DTLB_LOAD_MISS) {
   }
};

// ***************************************************************************

class countedDTLBStoreMisses : public countedVEvents {
 public:
   countedDTLBStoreMisses(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_DTLB_STORE_MISS) {
   }
};

// ***************************************************************************

class countedL1ReadMisses : public countedVEvents {
 public:
   countedL1ReadMisses(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L1_LOAD_MISS) {
   }
};

// ***************************************************************************

class countedL2ReadHitsShr : public countedVEvents {
 public:
   countedL2ReadHitsShr(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L2_READ_HIT_SHR) {
   }
};

// ***************************************************************************

class countedL2ReadHitsExcl : public countedVEvents {
 public:
   countedL2ReadHitsExcl(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L2_READ_HIT_EXCL) {
   }
};

// ***************************************************************************

class countedL2ReadHitsMod : public countedVEvents {
 public:
   countedL2ReadHitsMod(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L2_READ_HIT_MOD) {
   }
};

// ***************************************************************************

class countedL2ReadMisses : public countedVEvents {
 public:
   countedL2ReadMisses(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L2_READ_MISS) {
   }
};

// ***************************************************************************

class countedL3ReadHitsShr : public countedVEvents {
 public:
   countedL3ReadHitsShr(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L3_READ_HIT_SHR) {
   }
};

// ***************************************************************************

class countedL3ReadHitsExcl : public countedVEvents {
 public:
   countedL3ReadHitsExcl(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L3_READ_HIT_EXCL) {
   }
};

// ***************************************************************************

class countedL3ReadHitsMod : public countedVEvents {
 public:
   countedL3ReadHitsMod(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L3_READ_HIT_MOD) {
   }
};

// ***************************************************************************

class countedL3ReadMisses : public countedVEvents {
 public:
   countedL3ReadMisses(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L3_READ_MISS) {
   }
};

// ***************************************************************************

class countedMemStores : public countedVEvents {
 public:
   countedMemStores(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_MEM_STORE) {
   }
};

// ***************************************************************************

class countedMemLoads : public countedVEvents {
 public:
   countedMemLoads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_MEM_LOAD) {
   }
};

// ***************************************************************************

class countedCondBranches : public countedVEvents {
 public:
   countedCondBranches(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_COND_BRANCH) {
   }
};

// ***************************************************************************

class countedCondBranchesMispredicted : public countedVEvents {
 public:
   countedCondBranchesMispredicted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_COND_BRANCH_MISPREDICT) {
   }
};

// ***************************************************************************

class countedCalls : public countedVEvents {
 public:
   countedCalls(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_CALL) {
   }
};

// ***************************************************************************

class countedCallsMispredicted : public countedVEvents {
 public:
   countedCallsMispredicted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_CALL_MISPREDICT) {
   }
};

// ***************************************************************************

class countedReturns : public countedVEvents {
 public:
   countedReturns(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_RET) {
   }
};

// ***************************************************************************

class countedReturnsMispredicted : public countedVEvents {
 public:
   countedReturnsMispredicted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_RET_MISPREDICT) {
   }
};

// ***************************************************************************

class countedIndirects : public countedVEvents {
 public:
   countedIndirects(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_INDIRECT) {
   }
};

// ***************************************************************************

class countedIndirectsMispredicted : public countedVEvents {
 public:
   countedIndirectsMispredicted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_INDIRECT_MISPREDICT) {
   }
};

// ***************************************************************************

class countedBranchesTakenPredicted : public countedVEvents {
 public:
   countedBranchesTakenPredicted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_BRANCH_TAKEN_PREDICT) {
   }
};

// ***************************************************************************

class countedBranchesTakenMispredicted : public countedVEvents {
 public:
   countedBranchesTakenMispredicted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_BRANCH_TAKEN_MISPREDICT) {
   }
};

// ***************************************************************************

class countedBranchesNotTakenPredicted : public countedVEvents {
 public:
   countedBranchesNotTakenPredicted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_BRANCH_NOTTAKEN_PREDICT) {
   }
};

// ***************************************************************************

class countedBranchesNotTakenMispredicted : public countedVEvents {
 public:
   countedBranchesNotTakenMispredicted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_BRANCH_NOTTAKEN_MISPREDICT) {
   }
};

// ***************************************************************************

class countedVInsns : public countedVEvents {
 public:
   countedVInsns(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_INSTR_ISSUED) {
   }
};

// ***************************************************************************

class countedVInsnsCompleted : public countedVEvents {
 public:
   countedVInsnsCompleted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_INSTR_RETIRED) {
   }
};

// ***************************************************************************

class countedVuops : public countedVEvents {
 public:
   countedVuops(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_UOPS_RETIRED) {
   }
};

// ***************************************************************************

class countedPipelineClears : public countedVEvents {
 public:
   countedPipelineClears(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_PIPELINE_CLEAR) {
   }
};
#elif defined(ppc64_unknown_linux2_4)
class countedInsnsCompleted : public countedVEvents {
 public:
   countedInsnsCompleted(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_INSTRUCTIONS_COMPLETED) { 
   }
};

// ***************************************************************************

class countedL1ReadMisses : public countedVEvents {
 public:
   countedL1ReadMisses(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L1_DATA_MISS) {
   }
};

// ***************************************************************************

class countedL1Reads : public countedVEvents {
 public:
   countedL1Reads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L1_DATA_LOAD) {
   }
};

// ***************************************************************************

class countedL2Reads : public countedVEvents {
 public:
   countedL2Reads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L2_DATA_LOAD) {
   }
};

// ***************************************************************************

class countedL3Reads : public countedVEvents {
 public:
   countedL3Reads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L3_DATA_LOAD) {
   }
};

// ***************************************************************************

class countedL1Writes : public countedVEvents {
 public:
   countedL1Writes(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L1_DATA_STORE) {
   }
};

// ***************************************************************************

class countedL2Castouts : public countedVEvents {
 public:
   countedL2Castouts(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L2_DATA_INVALIDATION) {
   }
};

// ***************************************************************************

class countedInsnsDispatched : public countedVEvents {
 public:
   countedInsnsDispatched(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_INSTRUCTIONS_DISPATCHED) { 
   }
};

// ***************************************************************************

class countedL1InstructionReads : public countedVEvents {
 public:
   countedL1InstructionReads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L1_INSTRUCTIONS_LOAD) {
   }
};

// ***************************************************************************

class countedL2InstructionReads : public countedVEvents {
 public:
   countedL2InstructionReads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L2_INSTRUCTIONS_LOAD) {
   }
};

// ***************************************************************************

class countedL3InstructionReads : public countedVEvents {
 public:
   countedL3InstructionReads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_L3_INSTRUCTIONS_LOAD) {
   }
};

// ***************************************************************************

class countedMemInstructionReads : public countedVEvents {
 public:
   countedMemInstructionReads(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_MEMORY_INSTRUCTIONS_LOAD) {
   }
};

// ***************************************************************************

class countedInsnsPrefetched : public countedVEvents {
 public:
   countedInsnsPrefetched(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_PREFETCHED_INSTRUCTIONS) { 
   }
};
// ***************************************************************************

class countedInsnsUnfetched : public countedVEvents {
 public:
   countedInsnsUnfetched(unsigned iSimpleMetricId) :
      countedVEvents(iSimpleMetricId, HWC_NO_INSTRUCTIONS) { 
   }
};
// ***************************************************************************


#endif // arch-os

#endif
