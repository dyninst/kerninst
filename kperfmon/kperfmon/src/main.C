#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include "common/h/Ident.h"

#include "allSampleHandlers.h"
#include "allSimpleMetricFocuses.h"
#include "visiSubscriber.h"
#include "dataHeap.h"
#include "instrument.h" // fry1CmfSubscriberAndUninstrument()

#include "allComplexMetrics.h"
#include "entriesTo.h"
#include "exitsFrom.h"
#include "exitsFromNoUnwindTailCalls.h"

#include "metricVirtualizer.h"
#include "vtime.h" // virtualized & counted [i.e., recursion-safe] timers
#include "walltime.h"
#include "vinsnsPerVCycle.h"

#ifdef sparc_sun_solaris2_7
#include "sparc_dcache_vreads.h"
#include "sparc_dcache_vreadhits.h"
#include "sparc_dcache_vwrites.h"
#include "sparc_dcache_vwritehits.h"
#include "sparc_ecache_vrefs.h"
#include "sparc_ecache_vhits.h"
#include "sparc_ecache_vreadhits.h"
#include "sparc_icache_vrefs.h"
#include "sparc_icache_vhits.h"
#include "sparc_icache_vstallcycles.h"
#include "sparc_branchMispredict.h"
#elif defined(i386_unknown_linux2_4)
#include "x86_branch.h"
#include "x86_Lcache.h"
#include "x86_misc.h"
#include "x86_tlb.h"
#elif defined(ppc64_unknown_linux2_4)
#include "power_misc.h"
#include "power_data.h"
#include "power_icache.h"
#endif

#include "abstractions.h" // where axis & stuff
#include "kpmChildrenOracle.h"
#include "funcHooks.h"
#include "metricFocusManager.h"
#include "outliningMgr.h"
#include "tclCommands.h"
#include "prompt.h"
#include "common/h/Ident.h"

#include "kapi.h"

extern const char V_kperfmon[]; // not const char* (would crash!)
extern const Ident kperfmon_ident(V_kperfmon, "KernInst");
   // extern needed lest the variable have a static definition

bool haveGUI = true;
Tcl_Interp *global_interp = 0;
allSampleHandlers *globalAllSampleHandlers = 0;
allSimpleMetricFocuses *globalAllSimpleMetricFocuses = 0;
allComplexMetrics global_allMetrics;
abstractions<kpmChildrenOracle> *global_abstractions = 0;
dataHeap *theGlobalDataHeap = 0;
metricFocusManager *theGlobalMetricFocusManager = 0;
metricVirtualizer *theGlobalMetricVirtualizer = 0;
outliningMgr *theOutliningMgr = 0;

kapi_manager kmgr; // Have to make it global

int usage(char *basename)
{
    cerr << "Usage: " << basename << " hostname portnum\n";
    return (-1);
}

bool nonGUI_stillRunning = true;
static int nonGUI_exit(ClientData, Tcl_Interp *, int, TCLCONST char **) {
   nonGUI_stillRunning = false;
   return TCL_OK;
}

static void sendBufferedUpSampleDataToVisisNow()
{
    const dictionary_hash<unsigned,cmfSubscriber*> &allCmfSubscribers =
	cmfSubscriber::getAllCmfSubscribers();
   
    dictionary_hash<unsigned, cmfSubscriber*>::const_iterator iter =
	allCmfSubscribers.begin();
    dictionary_hash<unsigned, cmfSubscriber*>::const_iterator finish =
	allCmfSubscribers.end();

    for (; iter != finish; ++iter) {
	visiSubscriber *theVisi =
	    dynamic_cast<visiSubscriber*>(iter.currval());
	if (theVisi == NULL)
	    // wrong type of cmfSubscriber, so do nothing.  Ugly code; sorry.
	    continue;
	
	theVisi->shipBufferedSampleDataToVisiIfAny();
    }
}

static int processPendingEvents(kapi_manager *pmgr)
{
    // Some events come from the API. Mostly these are callbacks
    int rv = pmgr->handleEvents();

    // Since one or more events may have come in, it's conceivable that
    // some of those messages contained sample data. Furthermore, it's
    // possible that one or more perfect buckets were filled up. So, check
    // each "visi" class for buffered-up data & send it now.
    sendBufferedUpSampleDataToVisisNow();

    return rv;
}

static void event_handler_proc(ClientData cd, int mask) 
{
    if (mask & TCL_EXCEPTION) {
	cerr << "igen connection exception" << endl;
	assert(false);
    }
	
    kapi_manager *pmgr = (kapi_manager *)cd;
    processPendingEvents(pmgr);
}

static bool tclvrble2bool(Tcl_Interp *interp, const char *vname) {
   const char *str = Tcl_GetVar(interp, const_cast<char*>(vname), TCL_GLOBAL_ONLY);
   assert(str);
   
   if (0 == strcmp(str, "0"))
      return false;
   else {
      assert(0 == strcmp(str, "1"));
      return true;
   }
}

static int tclvrble2int(Tcl_Interp *interp, const char *vname) {
   const char *str = Tcl_GetVar(interp, const_cast<char*>(vname), TCL_GLOBAL_ONLY);
   assert(str);

   int result = atoi(str);
   return result;
}

#ifdef sparc_sun_solaris2_7
static kapi_manager::blockPlacementPrefs
tclvrble2OutlinedBlockPlacementEnum(Tcl_Interp *interp,
                                    const char *vname) {
   const char *str = Tcl_GetVar(interp, const_cast<char*>(vname), TCL_GLOBAL_ONLY);
   assert(str);
   
   if (0 == strcmp(str, "origSeq"))
      return kapi_manager::origSeq;
   else if (0 == strcmp(str, "chains"))
      return kapi_manager::chains;
   else {
      assert(false);
      return kapi_manager::origSeq;
   }
}

static kapi_manager::ThresholdChoices
tclvrble2OutlinedHotBlockThresholdEnum(Tcl_Interp *interp,
                                       const char *vname) {
   const char *str = Tcl_GetVar(interp, const_cast<char*>(vname), TCL_GLOBAL_ONLY);
   assert(str);
   
   if (0 == strcmp(str, "AnyNonZeroBlock"))
      return kapi_manager::AnyNonZeroBlock;
   else if (0 == strcmp(str, "FivePercent"))
      return kapi_manager::FivePercent;
   else {
      assert(false);
      return kapi_manager::AnyNonZeroBlock;
   }
}
#endif // sparc_sun_solaris2_7

int process_tcl_files()
{
    // Install commands implemented in C++ files (mostly tclCommands.C)
    installTclCommands(global_interp);

    pdstring tclFilesDirectory = ".";
    // a cmd line option should be installed to let us change this...

    const pdstring kperfmonTclFullFilePath = tclFilesDirectory + "/kperfmon.tcl";
    if (TCL_OK != Tcl_EvalFile(global_interp,
	      const_cast<char*>(kperfmonTclFullFilePath.c_str()))) {
	cerr << "Tcl_EvalFile of kperfmon.tcl failed:\""
	     << Tcl_GetStringResult(global_interp) << "\"" << endl;
	cerr << "Perhaps the file kperfmon.tcl could not be found?" << endl;
	cerr << "I searched for it in the directory \""
	     << tclFilesDirectory << "\"" << endl;
      
	return (-1);
    }

    const pdstring genericTclFullFilePath = tclFilesDirectory + "/generic.tcl";
    if (TCL_OK != Tcl_EvalFile(global_interp,
	      const_cast<char*>(genericTclFullFilePath.c_str()))) {
	cerr << "Tcl_EvalFile of generic.tcl failed:\""
	     << Tcl_GetStringResult(global_interp) << "\"" << endl;
	cerr << "Perhaps the file generic.tcl could not be found?" << endl;
	cerr << "I searched for it in the directory \""
	     << tclFilesDirectory << "\"" << endl;
      
	return (-1);
    }
    return 0;
}

void init_kperfmon_globals()
{
   // Simple Metrics:
   unsigned nextSimpleMetricId = 0;
   static add1AtEntry theAdd1AtEntrySimpleMetric(nextSimpleMetricId++);
   static add1AtExit theAdd1AtExitSimpleMetric(nextSimpleMetricId++);
   static add1AtExitNoUnwindTailCalls theAdd1AtExitSimpleMetricNoUnwindTailCalls(nextSimpleMetricId++);
      // 1 --> deltay

   static countedVCycles theCountedVCyclesSimpleMetric(nextSimpleMetricId++);
   static countedWallCycles theCountedWallCyclesSimpleMetric(nextSimpleMetricId++);

#ifdef sparc_sun_solaris2_7
   static countedDCacheVReads theCountedDCacheVReadsSimpleMetric(nextSimpleMetricId++);
   static countedDCacheVWrites theCountedDCacheVWritesSimpleMetric(nextSimpleMetricId++);
   static countedDCacheVReadHits theCountedDCacheVReadHitsSimpleMetric(nextSimpleMetricId++);
   static countedDCacheVWriteHits theCountedDCacheVWriteHitsSimpleMetric(nextSimpleMetricId++);
   static countedICacheVRefs theCountedICacheVRefsSimpleMetric(nextSimpleMetricId++);
   static countedICacheVHits theCountedICacheVHitsSimpleMetric(nextSimpleMetricId++);
   static countedICacheVStallCycles theCountedICacheVStallCyclesSimpleMetric(nextSimpleMetricId++);
   static countedBranchMispredVStallCycles theCountedBranchMispredVStallCyclesSimpleMetric(nextSimpleMetricId++);
   static countedECacheVRefs theCountedECacheVRefsSimpleMetric(nextSimpleMetricId++);
   static countedECacheVHits theCountedECacheVHitsSimpleMetric(nextSimpleMetricId++);
   static countedECacheVReadHits theCountedECacheVReadHitsSimpleMetric(nextSimpleMetricId++);
#elif defined(i386_unknown_linux2_4)
   static countedITLBHits theCountedITLBHitsSimpleMetric(nextSimpleMetricId++);
   static countedITLBHitsUC theCountedITLBHitsUCSimpleMetric(nextSimpleMetricId++);
   static countedITLBMisses theCountedITLBMissesSimpleMetric(nextSimpleMetricId++);
   static countedITLBMissPageWalks theCountedITLBMissPageWalksSimpleMetric(nextSimpleMetricId++);
   static countedDTLBMissPageWalks theCountedDTLBMissPageWalksSimpleMetric(nextSimpleMetricId++);
   static countedDTLBLoadMisses theCountedDTLBLoadMissesSimpleMetric(nextSimpleMetricId++);
   static countedDTLBStoreMisses theCountedDTLBStoreMissesSimpleMetric(nextSimpleMetricId++);
   static countedL1ReadMisses theCountedL1ReadMissesSimpleMetric(nextSimpleMetricId++);
   static countedL2ReadHitsShr theCountedL2ReadHitsShrSimpleMetric(nextSimpleMetricId++);
   static countedL2ReadHitsExcl theCountedL2ReadHitsExclSimpleMetric(nextSimpleMetricId++);
   static countedL2ReadHitsMod theCountedL2ReadHitsModSimpleMetric(nextSimpleMetricId++);
   static countedL2ReadMisses theCountedL2ReadMissesSimpleMetric(nextSimpleMetricId++);
   static countedL3ReadHitsShr theCountedL3ReadHitsShrSimpleMetric(nextSimpleMetricId++);
   static countedL3ReadHitsExcl theCountedL3ReadHitsExclSimpleMetric(nextSimpleMetricId++);
   static countedL3ReadHitsMod theCountedL3ReadHitsModSimpleMetric(nextSimpleMetricId++);
   static countedL3ReadMisses theCountedL3ReadMissesSimpleMetric(nextSimpleMetricId++);
   static countedMemStores theCountedMemStoresSimpleMetric(nextSimpleMetricId++);
   static countedMemLoads theCountedMemLoadsSimpleMetric(nextSimpleMetricId++);
   static countedCondBranches theCountedCondBranchesSimpleMetric(nextSimpleMetricId++);
   static countedCondBranchesMispredicted theCountedCondBranchesMispredictedSimpleMetric(nextSimpleMetricId++);
   static countedCalls theCountedCallsSimpleMetric(nextSimpleMetricId++);
   static countedCallsMispredicted theCountedCallsMispredictedSimpleMetric(nextSimpleMetricId++);
   static countedReturns theCountedReturnsSimpleMetric(nextSimpleMetricId++);
   static countedReturnsMispredicted theCountedReturnsMispredictedSimpleMetric(nextSimpleMetricId++);
   static countedIndirects theCountedIndirectsSimpleMetric(nextSimpleMetricId++);
   static countedIndirectsMispredicted theCountedIndirectsMispredictedSimpleMetric(nextSimpleMetricId++);
   static countedBranchesTakenPredicted theCountedBranchesTakenPredictedSimpleMetric(nextSimpleMetricId++);
   static countedBranchesTakenMispredicted theCountedBranchesTakenMispredictedSimpleMetric(nextSimpleMetricId++);
   static countedBranchesNotTakenPredicted theCountedBranchesNotTakenPredictedSimpleMetric(nextSimpleMetricId++);
   static countedBranchesNotTakenMispredicted theCountedBranchesNotTakenMispredictedSimpleMetric(nextSimpleMetricId++);
   static countedVInsnsCompleted theCountedVInsnsCompletedSimpleMetric(nextSimpleMetricId++);
   static countedVuops theCountedVuopsSimpleMetric(nextSimpleMetricId++);
   static countedPipelineClears theCountedPipelineClearsSimpleMetric(nextSimpleMetricId++);
#endif
#ifdef ppc64_unknown_linux2_4
   static countedInsnsCompleted theCountedInsnsCompletedSimpleMetric(nextSimpleMetricId++);
    static countedL1ReadMisses theCountedL1ReadMissesSimpleMetric(nextSimpleMetricId++);
    static countedL2Castouts theCountedL2CastoutsSimpleMetric(nextSimpleMetricId++);
    static countedInsnsDispatched theCountedInsnsDispatchedSimpleMetric(nextSimpleMetricId++);
    static countedL1Writes theCountedL1WritesSimpleMetric(nextSimpleMetricId++);
    static countedL1Reads theCountedL1ReadsSimpleMetric(nextSimpleMetricId++);
    static countedL2Reads theCountedL2ReadsSimpleMetric(nextSimpleMetricId++);
    static countedL3Reads theCountedL3ReadsSimpleMetric(nextSimpleMetricId++);
    static countedL1InstructionReads theCountedL1InstructionReadsSimpleMetric(nextSimpleMetricId++);
    static countedL2InstructionReads theCountedL2InstructionReadsSimpleMetric(nextSimpleMetricId++);
  static countedL3InstructionReads theCountedL3InstructionReadsSimpleMetric(nextSimpleMetricId++);
    static countedMemInstructionReads theCountedMemInstructionReadsSimpleMetric(nextSimpleMetricId++);
   static countedInsnsPrefetched theCountedInsnsPrefetchedSimpleMetric(nextSimpleMetricId++);
   static countedInsnsUnfetched theCountedInsnsUnfetchedSimpleMetric(nextSimpleMetricId++);
#else
   static countedVInsns theCountedVInsnsSimpleMetric(nextSimpleMetricId++);
#endif
      // Complex Metrics:

   unsigned nextComplexMetricId = 0;
   unsigned currComplexMetricClusterId = 0;
   
   global_allMetrics.add(new entriesTo(nextComplexMetricId++,
                                       currComplexMetricClusterId,
                                       theAdd1AtEntrySimpleMetric));

   global_allMetrics.add(new exitsFrom(nextComplexMetricId++,
                                       currComplexMetricClusterId,
                                       theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new exitsFromNoUnwindTailCalls(nextComplexMetricId++,
                                                        currComplexMetricClusterId,
                                                        theAdd1AtExitSimpleMetricNoUnwindTailCalls));

//     global_allMetrics.add(new codeSize(nextComplexMetricId++,
//                                        ++currComplexMetricClusterId));

   global_allMetrics.add(new vtime(nextComplexMetricId++,
                                   ++currComplexMetricClusterId,
                                   theCountedVCyclesSimpleMetric));
   global_allMetrics.add(new vtime_perinvoc(nextComplexMetricId++,
                                            currComplexMetricClusterId,
                                            theCountedVCyclesSimpleMetric,
                                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new walltime(nextComplexMetricId++,
				      currComplexMetricClusterId,
				      theCountedWallCyclesSimpleMetric));
   global_allMetrics.add(new 
			 walltime_perinvoc(nextComplexMetricId++,
					   currComplexMetricClusterId,
					   theCountedWallCyclesSimpleMetric,
					   theAdd1AtExitSimpleMetric));

#ifdef sparc_sun_solaris2_7
   // Here we have the sparc specific performance counter metrics

   global_allMetrics.add(new dcache_vreads(nextComplexMetricId++,
                                           ++currComplexMetricClusterId,
                                           theCountedDCacheVReadsSimpleMetric));

   global_allMetrics.add(new dcache_vreads_perinvoc(nextComplexMetricId++,
                                                    currComplexMetricClusterId,
                                                    theCountedDCacheVReadsSimpleMetric,
                                                    theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new dcache_vreadhits(nextComplexMetricId++,
                                              currComplexMetricClusterId,
                                              theCountedDCacheVReadHitsSimpleMetric));

   global_allMetrics.add(new dcache_vreadhits_perinvoc(nextComplexMetricId++,
                                                       currComplexMetricClusterId,
                                                       theCountedDCacheVReadHitsSimpleMetric,
                                                       theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new dcache_vwrites(nextComplexMetricId++,
                                            currComplexMetricClusterId,
                                            theCountedDCacheVWritesSimpleMetric));

   global_allMetrics.add(new dcache_vwrites_perinvoc(nextComplexMetricId++,
                                                     currComplexMetricClusterId,
                                                     theCountedDCacheVWritesSimpleMetric,
                                                     theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new dcache_vwritehits(nextComplexMetricId++,
                                               currComplexMetricClusterId,
                                               theCountedDCacheVWriteHitsSimpleMetric));

   global_allMetrics.add(new dcache_vwritehits_perinvoc(nextComplexMetricId++,
                                                        currComplexMetricClusterId,
                                                        theCountedDCacheVWriteHitsSimpleMetric,
                                                        theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new ecache_vrefs(nextComplexMetricId++,
                                          ++currComplexMetricClusterId,
                                          theCountedECacheVRefsSimpleMetric));

   global_allMetrics.add(new ecache_vrefs_perinvoc(nextComplexMetricId++,
                                                   currComplexMetricClusterId,
                                                   theCountedECacheVRefsSimpleMetric,
                                                   theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new ecache_vhits(nextComplexMetricId++,
                                          currComplexMetricClusterId,
                                          theCountedECacheVHitsSimpleMetric));

   global_allMetrics.add(new ecache_vhits_perinvoc(nextComplexMetricId++,
                                                   currComplexMetricClusterId,
                                                   theCountedECacheVHitsSimpleMetric,
                                                   theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new ecache_vmisses(nextComplexMetricId++,
                                            currComplexMetricClusterId,
                                            theCountedECacheVHitsSimpleMetric,
                                            theCountedECacheVRefsSimpleMetric));

   global_allMetrics.add(new ecache_vhitratio(nextComplexMetricId++,
                                              currComplexMetricClusterId,
                                              theCountedECacheVHitsSimpleMetric,
                                              theCountedECacheVRefsSimpleMetric));

   global_allMetrics.add(new ecache_vmissratio(nextComplexMetricId++,
                                               currComplexMetricClusterId,
                                               theCountedECacheVHitsSimpleMetric,
                                               theCountedECacheVRefsSimpleMetric));

   global_allMetrics.add(new ecache_vreadhits(nextComplexMetricId++,
                                              currComplexMetricClusterId,
                                              theCountedECacheVReadHitsSimpleMetric));

   global_allMetrics.add(new icache_vrefs(nextComplexMetricId++,
                                          ++currComplexMetricClusterId,
                                          theCountedICacheVRefsSimpleMetric));
   global_allMetrics.add(new icache_vrefs_perinvoc(nextComplexMetricId++,
                                                   currComplexMetricClusterId,
                                                   theCountedICacheVRefsSimpleMetric,
                                                   theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new icache_vhits(nextComplexMetricId++,
                                          currComplexMetricClusterId,
                                          theCountedICacheVHitsSimpleMetric));
   global_allMetrics.add(new icache_vhits_perinvoc(nextComplexMetricId++,
                                                   currComplexMetricClusterId,
                                                   theCountedICacheVHitsSimpleMetric,
                                                   theAdd1AtExitSimpleMetric));

//     global_allMetrics.add(new icache_vmisses(nextComplexMetricId++,
//                                              currComplexMetricClusterId,
//                                              theCountedICacheVHitsSimpleMetric,
//                                              theCountedICacheVRefsSimpleMetric));
//     global_allMetrics.add(new icache_vmisses_perinvoc(nextComplexMetricId++,
//                                                       currComplexMetricClusterId,
//                                                       theCountedICacheVMissesSimpleMetric,
//                                                       theAdd1AtExitSimpleMetric));

//     global_allMetrics.add(new icache_vhitratio(nextComplexMetricId++,
//                                                currComplexMetricClusterId,
//                                                theCountedICacheVHitsSimpleMetric,
//                                                theCountedICacheVRefsSimpleMetric));

//     global_allMetrics.add(new icache_vmissratio(nextComplexMetricId++,
//                                                 currComplexMetricClusterId,
//                                                 theCountedICacheVHitsSimpleMetric,
//                                                 theCountedICacheVRefsSimpleMetric));

   global_allMetrics.add(new icache_vstallcycles(nextComplexMetricId++,
                                                 currComplexMetricClusterId,
                                                 theCountedICacheVStallCyclesSimpleMetric));

   global_allMetrics.add(new icache_vstallcycles_perinvoc(nextComplexMetricId++,
                                                          currComplexMetricClusterId,
                                                          theCountedICacheVStallCyclesSimpleMetric,
                                                          theAdd1AtExitSimpleMetric));

   global_allMetrics.add(new icache_vstallcycles_per_vtime(nextComplexMetricId++,
                                                           currComplexMetricClusterId,
                                                           theCountedICacheVStallCyclesSimpleMetric,
                                                           theCountedVCyclesSimpleMetric));

   global_allMetrics.add(new branchMispred_vstallcycles(nextComplexMetricId++,
                                                        ++currComplexMetricClusterId,
                                                        theCountedBranchMispredVStallCyclesSimpleMetric));
   global_allMetrics.add
      (new branchMispred_vstallcycles_perinvoc(nextComplexMetricId++,
                                               currComplexMetricClusterId,
                                               theCountedBranchMispredVStallCyclesSimpleMetric,
                                               theAdd1AtExitSimpleMetric));
   global_allMetrics.add(
      new branchMispred_vstallcycles_per_vtime(nextComplexMetricId++,
                                               currComplexMetricClusterId,
                                               theCountedBranchMispredVStallCyclesSimpleMetric,
                                                  theCountedVCyclesSimpleMetric));

   global_allMetrics.add(new vInsnsPerVCycle(nextComplexMetricId++,
                                             ++currComplexMetricClusterId,
                                             theCountedVInsnsSimpleMetric,
                                             theCountedVCyclesSimpleMetric));
   global_allMetrics.add(new vCyclesPerVInsn(nextComplexMetricId++,
                                             currComplexMetricClusterId,
                                             theCountedVInsnsSimpleMetric,
                                             theCountedVCyclesSimpleMetric));
   
#elif defined(i386_unknown_linux2_4)

   // Here we have the x86 cpu-specific performance counter metrics

   // hackish method to get cpu family (.first) and model (.second) info
   kapi_vector<unsigned> cpu_model_info = kmgr.getABIs();
   assert(cpu_model_info.size() == 2);

   if(cpu_model_info[0] == 15) { // P4 or Xeon

      global_allMetrics.add(new ITLB_vhits(nextComplexMetricId++,
                                           ++currComplexMetricClusterId,
                                           theCountedITLBHitsSimpleMetric));
      global_allMetrics.add(new ITLB_vhits_perinvoc(nextComplexMetricId++,
                                                currComplexMetricClusterId,
                                                theCountedITLBHitsSimpleMetric,
                                                theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new ITLB_vhits_uncacheable(
                                           nextComplexMetricId++,
                                           currComplexMetricClusterId,
                                           theCountedITLBHitsUCSimpleMetric));
      global_allMetrics.add(new ITLB_vhits_uncacheable_perinvoc(
                                           nextComplexMetricId++,
                                           currComplexMetricClusterId,
                                           theCountedITLBHitsUCSimpleMetric,
                                           theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new ITLB_vmisses(nextComplexMetricId++,
                                          currComplexMetricClusterId,
                                          theCountedITLBMissesSimpleMetric));
      global_allMetrics.add(new ITLB_vmisses_perinvoc(
                                          nextComplexMetricId++,
                                          currComplexMetricClusterId,
                                          theCountedITLBMissesSimpleMetric,
                                          theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new ITLB_vmiss_page_walks(
                                    nextComplexMetricId++,
                                    currComplexMetricClusterId,
                                    theCountedITLBMissPageWalksSimpleMetric));
      global_allMetrics.add(new ITLB_vmiss_page_walks_perinvoc(
                                    nextComplexMetricId++,
                                    currComplexMetricClusterId,
                                    theCountedITLBMissPageWalksSimpleMetric,
                                    theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new DTLB_vmiss_page_walks(
                                    nextComplexMetricId++,
                                    ++currComplexMetricClusterId,
                                    theCountedDTLBMissPageWalksSimpleMetric));
      global_allMetrics.add(new DTLB_vmiss_page_walks_perinvoc(
                                    nextComplexMetricId++,
                                    currComplexMetricClusterId,
                                    theCountedDTLBMissPageWalksSimpleMetric,
                                    theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new DTLB_vload_misses(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedDTLBLoadMissesSimpleMetric));
      global_allMetrics.add(new DTLB_vload_misses_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedDTLBLoadMissesSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new DTLB_vstore_misses(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedDTLBStoreMissesSimpleMetric));
      global_allMetrics.add(new DTLB_vstore_misses_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedDTLBStoreMissesSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L1_vread_misses(
                                      nextComplexMetricId++,
                                      ++currComplexMetricClusterId,
                                      theCountedL1ReadMissesSimpleMetric));
      global_allMetrics.add(new L1_vread_misses_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL1ReadMissesSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L2_vread_hits_shared(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL2ReadHitsShrSimpleMetric));
      global_allMetrics.add(new L2_vread_hits_shared_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL2ReadHitsShrSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L2_vread_hits_exclusive(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL2ReadHitsExclSimpleMetric));
      global_allMetrics.add(new L2_vread_hits_exclusive_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL2ReadHitsExclSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L2_vread_hits_modified(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL2ReadHitsModSimpleMetric));
      global_allMetrics.add(new L2_vread_hits_modified_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL2ReadHitsModSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L2_vread_misses(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL2ReadMissesSimpleMetric));
      global_allMetrics.add(new L2_vread_misses_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL2ReadMissesSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L3_vread_hits_shared(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL3ReadHitsShrSimpleMetric));
      global_allMetrics.add(new L3_vread_hits_shared_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL3ReadHitsShrSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L3_vread_hits_exclusive(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL3ReadHitsExclSimpleMetric));
      global_allMetrics.add(new L3_vread_hits_exclusive_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL3ReadHitsExclSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L3_vread_hits_modified(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL3ReadHitsModSimpleMetric));
      global_allMetrics.add(new L3_vread_hits_modified_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL3ReadHitsModSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new L3_vread_misses(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL3ReadMissesSimpleMetric));
      global_allMetrics.add(new L3_vread_misses_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedL3ReadMissesSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new Memory_vloads(
                                      nextComplexMetricId++,
                                      ++currComplexMetricClusterId,
                                      theCountedMemLoadsSimpleMetric));
      global_allMetrics.add(new Memory_vloads_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedMemLoadsSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new Memory_vstores(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedMemStoresSimpleMetric));
      global_allMetrics.add(new Memory_vstores_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedMemStoresSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new CondBranches(
                                      nextComplexMetricId++,
                                      ++currComplexMetricClusterId,
                                      theCountedCondBranchesSimpleMetric));
      global_allMetrics.add(new CondBranches_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedCondBranchesSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new CondBranch_mispredicts(
                              nextComplexMetricId++,
                              currComplexMetricClusterId,
                              theCountedCondBranchesMispredictedSimpleMetric));
      global_allMetrics.add(new CondBranch_mispredicts_perinvoc(
                                nextComplexMetricId++,
                                currComplexMetricClusterId,
                                theCountedCondBranchesMispredictedSimpleMetric,
                                theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new Calls(nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedCallsSimpleMetric));
      global_allMetrics.add(new Calls_perinvoc(nextComplexMetricId++,
                                               currComplexMetricClusterId,
                                               theCountedCallsSimpleMetric,
                                               theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new Call_mispredicts(
                                   nextComplexMetricId++,
                                   currComplexMetricClusterId,
                                   theCountedCallsMispredictedSimpleMetric));
      global_allMetrics.add(new Call_mispredicts_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedCallsMispredictedSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new Returns(nextComplexMetricId++,
                                        currComplexMetricClusterId,
                                        theCountedReturnsSimpleMetric));
      global_allMetrics.add(new Returns_perinvoc(nextComplexMetricId++,
                                                 currComplexMetricClusterId,
                                                 theCountedReturnsSimpleMetric,
                                                 theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new Return_mispredicts(
                                   nextComplexMetricId++,
                                   currComplexMetricClusterId,
                                   theCountedReturnsMispredictedSimpleMetric));
      global_allMetrics.add(new Return_mispredicts_perinvoc(
                                   nextComplexMetricId++,
                                   currComplexMetricClusterId,
                                   theCountedReturnsMispredictedSimpleMetric,
                                   theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new Indirects(nextComplexMetricId++,
                                          currComplexMetricClusterId,
                                          theCountedIndirectsSimpleMetric));
      global_allMetrics.add(new Indirects_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedIndirectsSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new Indirect_mispredicts(
                                nextComplexMetricId++,
                                currComplexMetricClusterId,
                                theCountedIndirectsMispredictedSimpleMetric));
      global_allMetrics.add(new Indirect_mispredicts_perinvoc(
                                   nextComplexMetricId++,
                                   currComplexMetricClusterId,
                                   theCountedIndirectsMispredictedSimpleMetric,
                                   theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new BranchesTakenPredicted(
                               nextComplexMetricId++,
                               ++currComplexMetricClusterId,
                               theCountedBranchesTakenPredictedSimpleMetric));
      global_allMetrics.add(new BranchesTakenPredicted_perinvoc(
                               nextComplexMetricId++,
                               currComplexMetricClusterId,
                               theCountedBranchesTakenPredictedSimpleMetric,
                               theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new BranchesTakenMispredicted(
                             nextComplexMetricId++,
                             currComplexMetricClusterId,
                             theCountedBranchesTakenMispredictedSimpleMetric));
      global_allMetrics.add(new BranchesTakenMispredicted_perinvoc(
                              nextComplexMetricId++,
                              currComplexMetricClusterId,
                              theCountedBranchesTakenMispredictedSimpleMetric,
                              theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new BranchesNotTakenPredicted(
                             nextComplexMetricId++,
                             currComplexMetricClusterId,
                             theCountedBranchesNotTakenPredictedSimpleMetric));
      global_allMetrics.add(new BranchesNotTakenPredicted_perinvoc(
                              nextComplexMetricId++,
                              currComplexMetricClusterId,
                              theCountedBranchesNotTakenPredictedSimpleMetric,
                              theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new BranchesNotTakenMispredicted(
                          nextComplexMetricId++,
                          currComplexMetricClusterId,
                          theCountedBranchesNotTakenMispredictedSimpleMetric));
      global_allMetrics.add(new BranchesNotTakenMispredicted_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedBranchesNotTakenMispredictedSimpleMetric,
                            theAdd1AtExitSimpleMetric));
      global_allMetrics.add(new insn_vuops(nextComplexMetricId++,
                                           ++currComplexMetricClusterId,
                                           theCountedVuopsSimpleMetric));
      global_allMetrics.add(new insn_vuops_perinvoc(
                                        nextComplexMetricId++,
                                        currComplexMetricClusterId,
                                        theCountedVuopsSimpleMetric,
                                        theAdd1AtExitSimpleMetric));

      if(cpu_model_info[1] == 3) {
         // instructions completed event available on only model 3
         global_allMetrics.add(new insn_vcompleted(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedVInsnsCompletedSimpleMetric));
         global_allMetrics.add(new insn_vcompleted_perinvoc(
                                      nextComplexMetricId++,
                                      currComplexMetricClusterId,
                                      theCountedVInsnsCompletedSimpleMetric,
                                      theAdd1AtExitSimpleMetric));
      }
      else {
         // instructions retired event available on all P4/Xeon models
         global_allMetrics.add(new insn_vretired(
                                        nextComplexMetricId++,
                                        currComplexMetricClusterId,
                                        theCountedVInsnsSimpleMetric));
         global_allMetrics.add(new insn_vretired_perinvoc(
                                        nextComplexMetricId++,
                                        currComplexMetricClusterId,
                                        theCountedVInsnsSimpleMetric,
                                        theAdd1AtExitSimpleMetric));
      }

      global_allMetrics.add(new vInsnsPerVCycle(nextComplexMetricId++,
                                              currComplexMetricClusterId,
                                              theCountedVInsnsSimpleMetric,
                                              theCountedVCyclesSimpleMetric));
      global_allMetrics.add(new vCyclesPerVInsn(nextComplexMetricId++,
                                              currComplexMetricClusterId,
                                              theCountedVInsnsSimpleMetric,
                                              theCountedVCyclesSimpleMetric));

      global_allMetrics.add(new pipeline_vclears(
                                        nextComplexMetricId++,
                                        ++currComplexMetricClusterId,
                                        theCountedPipelineClearsSimpleMetric));
      global_allMetrics.add(new pipeline_vclears_perinvoc(
                                        nextComplexMetricId++,
                                        currComplexMetricClusterId,
                                        theCountedPipelineClearsSimpleMetric,
                                        theAdd1AtExitSimpleMetric));
   }
#elif defined(ppc64_unknown_linux2_4)
   global_allMetrics.add(new insn_vcompleted(
                            nextComplexMetricId++,
                            ++currComplexMetricClusterId,
                            theCountedInsnsCompletedSimpleMetric));
   global_allMetrics.add(new insn_vcompleted_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedInsnsCompletedSimpleMetric,
                            theAdd1AtExitSimpleMetric));
    global_allMetrics.add(new insn_vdispatched(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedInsnsDispatchedSimpleMetric));
   global_allMetrics.add(new insn_vdispatched_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedInsnsDispatchedSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new L1_vread_misses(
                            nextComplexMetricId++,
                            ++currComplexMetricClusterId,
                            theCountedL1ReadMissesSimpleMetric));
   global_allMetrics.add(new L1_vread_misses_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL1ReadMissesSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new L1_vreads(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL1ReadsSimpleMetric));
   global_allMetrics.add(new L1_vreads_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL1ReadsSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new L2_vreads(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL2ReadsSimpleMetric));
   global_allMetrics.add(new L2_vreads_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL2ReadsSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new L3_vreads(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL3ReadsSimpleMetric));
   global_allMetrics.add(new L3_vreads_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL3ReadsSimpleMetric,
                            theAdd1AtExitSimpleMetric));
    global_allMetrics.add(new L1_vwrites(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL1WritesSimpleMetric));
   global_allMetrics.add(new L1_vwrites_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL1WritesSimpleMetric,
                            theAdd1AtExitSimpleMetric));
    global_allMetrics.add(new L2_vcastouts(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL2CastoutsSimpleMetric));
   global_allMetrics.add(new L2_vcastouts_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL2CastoutsSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new L1_instruction_vreads(
                            nextComplexMetricId++,
                            ++currComplexMetricClusterId,
                            theCountedL1InstructionReadsSimpleMetric));
   global_allMetrics.add(new L1_instruction_vreads_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL1InstructionReadsSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new L2_instruction_vreads(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL2InstructionReadsSimpleMetric));
   global_allMetrics.add(new L2_instruction_vreads_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL2InstructionReadsSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new L3_instruction_vreads(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL3InstructionReadsSimpleMetric));
   global_allMetrics.add(new L3_instruction_vreads_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedL3InstructionReadsSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new Mem_instruction_vreads(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedMemInstructionReadsSimpleMetric));
   global_allMetrics.add(new Mem_instruction_vreads_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedMemInstructionReadsSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new insn_vprefetched(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedInsnsPrefetchedSimpleMetric));
   global_allMetrics.add(new insn_vprefetched_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedInsnsPrefetchedSimpleMetric,
                            theAdd1AtExitSimpleMetric));
   global_allMetrics.add(new insn_vunfetched(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedInsnsUnfetchedSimpleMetric));
   global_allMetrics.add(new insn_vunfetched_perinvoc(
                            nextComplexMetricId++,
                            currComplexMetricClusterId,
                            theCountedInsnsUnfetchedSimpleMetric,
                            theAdd1AtExitSimpleMetric));
#endif // arch_os   

   globalAllSampleHandlers = new allSampleHandlers();
   globalAllSimpleMetricFocuses = new allSimpleMetricFocuses();

   // 100 vectors of counters, 50 vectors of wall timers, and 30
   // vectors of vtimers Note that each is slightly under 1 page on
   // 1-8 processors...no use in making them any less since that's the 
   // granularity of mmap
   theGlobalDataHeap = new dataHeap(kmgr.getMaxCPUId() + 1,
				    100, 50, 30);

   theGlobalMetricFocusManager = new metricFocusManager();

   theGlobalMetricVirtualizer = new metricVirtualizer();

#ifdef sparc_sun_solaris2_7
   extern void traceOutliningParameters(); // tclCommands.C
   traceOutliningParameters();

   theOutliningMgr = new outliningMgr(
       tclvrble2bool(global_interp, "outliningDoReplaceFunction"),
       tclvrble2bool(
	   global_interp, "outliningReplaceFunctionPatchUpCallSitesToo"),
       tclvrble2int(global_interp, "outliningBlockCountDelay"),
       tclvrble2bool(global_interp, "outlineChildrenToo"),
       tclvrble2bool(global_interp, "forceOutliningOfAllChildren"),
       tclvrble2OutlinedBlockPlacementEnum(
	   global_interp, "outliningBlockPlacement"),
       tclvrble2bool(global_interp, "outliningUsingFixedDescendants"),
       tclvrble2OutlinedHotBlockThresholdEnum(
	   global_interp, "outliningHotBlockThresholdChoice"));
   assert(theOutliningMgr);
#endif
}

void delete_kperfmon_globals()
{
#ifdef sparc_sun_solaris2_7
    // This should go before we fry any subscribers, since the outliner
    // creates a few subscribers of its own
    theOutliningMgr->kperfmonIsGoingDownNow();
    delete theOutliningMgr;
#endif

    // Any cmfSubscribers left?  If so, fry them now.
    while (cmfSubscriber::getAllCmfSubscribers().size() > 0) {
	dictionary_hash<unsigned, cmfSubscriber*>::const_iterator iter = 
	    cmfSubscriber::getAllCmfSubscribers().begin();
	cmfSubscriber *theSubscriber = iter.currval();

	fry1CmfSubscriberAndUninstrument(theSubscriber, true);
	// true --> kperfmon is going down now. Note that
	// fry1CmfSubscriberAndUninstrument() modifies the dictionary,
	// which is why this loop looks a little odd (not the usual
	// iteration from begin() to end(), because they'll become danling
	// pointers as various items get fried)
    }
    //   assert(!globalAllSampleHandlers->anythingSubscribed());
    //   FIXME

    delete theGlobalMetricVirtualizer;
    theGlobalMetricVirtualizer = NULL;

    delete theGlobalMetricFocusManager;
    theGlobalMetricFocusManager = NULL;

    delete theGlobalDataHeap;
    theGlobalDataHeap = NULL;

    // global_abstractions should already have been fried in
    // whereAxisDestroyed_cmd
    assert(global_abstractions == NULL);

    delete globalAllSimpleMetricFocuses;
    globalAllSimpleMetricFocuses = NULL; // help purify find mem leaks
   
    delete globalAllSampleHandlers;
    globalAllSampleHandlers = NULL; // help purify find mem leaks

}

int main(int argc, char **argv)
{
    if (argc < 3) {
	return usage(argv[0]);
    }

    char *host = argv[1];
    int port = atoi(argv[2]);

    cout << "Welcome to Kperfmon, " << kperfmon_ident.release() << endl;
   
    // Attach to the kernel
    if (kmgr.attach(host, port) < 0) {
	abort();
    }

    extern int dynloadCallback(kapi_dynload_event_t event,
			       const kapi_module *kmod,
			       const kapi_function *kfunc);// dynloadCallback.C
    if (kmgr.registerDynloadCallback(dynloadCallback) < 0) {
	abort();
    }

    if (kmgr.setTestingFlag(false) < 0) {
	abort();
    }

    funcHooks hookMgr;
    if (argc == 4) {
	char hooksFileName[PATH_MAX];
	if (sscanf(argv[3], "--hooks=%[^ \t\n]", hooksFileName) != 1) {
	    cerr << "unknown argument " << argv[3] 
		 << ", expected: --hooks=hooks.txt\n";
	    return (-1);
	}
	haveGUI = false;
	hookMgr.processHooksFromFile(hooksFileName);
    }

    global_interp = Tcl_CreateInterp();
    
    if (Tcl_Init(global_interp) == TCL_ERROR) {
	cerr << "call to Tcl_Init() failed: \""
	     << Tcl_GetStringResult(global_interp) << "\"" << endl;
	return (-5);
    }
    if (haveGUI && Tk_Init(global_interp) == TCL_ERROR) {
	cerr << "call to Tk_Init() failed: \"" 
	     << Tcl_GetStringResult(global_interp) << "\""
	     << "(Attempting) to continue with no GUI..." << endl;
	haveGUI = false;
    }

    if (process_tcl_files() < 0) {
	return (-1);
    }
    
    // Set-up a tty handler. Currently, we do it right in main() so that the
    // stack-placed "idle" object does not get deleted until the program ends
    command_file_fd = fileno(stdin);
    tclInstallIdle idle(installTheTtyHandler);
    idle.install(NULL); // arg: clientData

    init_kperfmon_globals();

    // Get the number of CPUs on the remote (kerninstd) machine
    int numCPUs = kmgr.getNumCPUs();

    char buffer[100]; // Tcl_Eval writes to it!
    sprintf(buffer, "initializeKperfmon %d %d", (haveGUI ? 1 : 0), numCPUs);
    if (Tcl_Eval(global_interp, buffer) != TCL_OK) {
	cout << global_interp->result << endl;
	abort();
    }

    // set up a tcl file handler for processing incoming igen info from 
    // kerninstd
    Tcl_CreateFileHandler(kmgr.getEventQueueDescriptor(),
			  TCL_READABLE | TCL_EXCEPTION,
			  event_handler_proc,
			  &kmgr); // client data
    // Exit command
    Tcl_CreateCommand(global_interp, "exit", nonGUI_exit, NULL, NULL);

    if (haveGUI) {
	Tk_SetClass(Tk_MainWindow(global_interp), "Kperfmon");
	Tk_MainLoop();
    }
    else {
	assert(nonGUI_stillRunning);
	// non-GUI main loop.
	// processPendingEvents returns (-1) on quit
	while (processPendingEvents(&kmgr) != -1 && nonGUI_stillRunning) {
	    Tcl_DoOneEvent(0);
	}
    }

    hookMgr.uninstallHooks();

    delete_kperfmon_globals();

    if (kmgr.detach() < 0) {
	abort();
    }
}
