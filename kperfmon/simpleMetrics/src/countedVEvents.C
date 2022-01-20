// countedVEvents.C
#include <string.h>
#include "util/h/rope-utils.h"
#include "util/h/out_streams.h"
#include "countedVEvents.h"
#include "metricVirtualizer.h"
#include "simpleMetricFocus.h"
#include "vtimerCountedPrimitive.h"
#include "dataHeap.h"
#include "pidInSetPredicate.h"
#include "stacklist.h"
#include "vtimer.h"
#include "kapi.h"

extern kapi_manager kmgr;
extern metricVirtualizer *theGlobalMetricVirtualizer;

#ifdef sparc_sun_solaris2_7
extern bool usesPic(kapi_hwcounter_kind kind, int num);
#endif

// Initialize the timer data in the kernel memory
void countedVEvents::initialize_timer(kptr_t kernelAddr,
				      unsigned timer_type,
				      unsigned elemsPerVector,
				      unsigned bytesPerStride,
				      kptr_t stopRoutineKernelAddr,
				      kptr_t restartRoutineKernelAddr) const
{
    char vtimer[sizeof_vtimer];
    
    // Prepare the vtimer entry for writing
    memset(vtimer, 0, sizeof_vtimer);

    // Fill in the vtimer type
    char *vtimer_type = &vtimer[counter_type_offset];
    unsigned ctr_type = timer_type;
    if(includeEventsSwitchedOut()) {
       ctr_type |= WALL_TIMER;
    }
    *(unsigned *)vtimer_type = ctr_type;

    // Addresses can not be copied as-is: the client byte order may not
    // match that of the kernel
    kmgr.to_kernel_byte_order(&stopRoutineKernelAddr, sizeof(kptr_t));
    kmgr.to_kernel_byte_order(&restartRoutineKernelAddr, sizeof(kptr_t));
    memcpy(vtimer + stop_routine_offset,
	   &stopRoutineKernelAddr, sizeof(kptr_t));
    memcpy(vtimer + restart_routine_offset,
	   &restartRoutineKernelAddr, sizeof(kptr_t));

    kptr_t addr = kernelAddr;
    for (unsigned i=0; i<elemsPerVector; i++) {
	// Do the writing itself
	kmgr.memcopy(vtimer, addr, sizeof_vtimer);
	addr += bytesPerStride;
    }
}

bool countedVEvents::canInstantiateFocus(const focus &theFocus) const {
   if (theFocus.isRoot() || theFocus.isModule())
      return false;
   else {
      // Function, basic block, or even instruction.
      // We'll return 'success' as long as the fn has been successfully parsed
      // into basic blocks.
       kapi_function kFunc;
       if (kmgr.findFunctionByAddress(theFocus.getFnAddr(), &kFunc) < 0) {
	   assert(false);
       }
       return !kFunc.isUnparsed();
   }
}

void countedVEvents::
asyncInstantiate(simpleMetricFocus &smf,
                 cmfInstantiatePostSmfInstantiation &postInstantiateCB) const {
   const focus &theFocus = smf.getFocus();
   
   // Step 0: on a metric-specific basic, disallow certain foci.
   // In fact, we can assert it, since outside code should have checked with
   // a call to canInstantiateFocus() before calling us.
   assert(canInstantiateFocus(theFocus));

   // Step 1: Allocate the (one) vtimer
   extern dataHeap *theGlobalDataHeap;

   kptr_t vtimerKernelAddr = theGlobalDataHeap->allocVT();

   smf.addVtimer(vtimerKernelAddr);

#ifndef i386_unknown_linux2_4
   assert(vtimerKernelAddr % 8 == 0);
#endif

   // virtualization step 0: change %pcr to a value that we like
   kapi_hwcounter_set oldPcrVal;
   if (oldPcrVal.readConfig() < 0) {
       assert(false);
   }
   kapi_hwcounter_set newPcrVal = 
       queryPostInstrumentationPcr(oldPcrVal, theFocus);
   if (!newPcrVal.equals(oldPcrVal)) {
       if (newPcrVal.writeConfig() < 0) {
	   assert(false);
       }
   }
   
   unsigned vtimerType = 0;

   vtimerType = (unsigned)hwkind;
   if(hwkind != HWC_TICKS) {
#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
      unsigned perfctr_num = newPcrVal.findCounter(hwkind, 
                                                   true /* must find */);
      vtimerType |= (perfctr_num << 16);

      // AST for raw hwcounter expression was already generated, but we
      // didn't know the counter that was actually going to be used, so
      // we need to regenerate the AST using the new counter info
      rawCtrExpr.resetType((kapi_hwcounter_kind)vtimerType);
#elif defined(sparc_sun_solaris2_7)
      // Set counter portion of vtimerType to pic0/1
      if(usesPic(hwkind, 1))
         vtimerType |= (1 << 16);
#endif
   }
   
   // virtualization step 1/4: call referenceVirtualizationCode() to put
   // calls to the context-switch handling routines in the kernel driver
   // at the kernel context-switch points

   theGlobalMetricVirtualizer->referenceVirtualizationCode();

   // virtualization step 2/4: reference metric-specific how-to-stop and
   // how-to-start routines.
   std::pair<kptr_t, kptr_t> stopAndRestartRoutines =
      theGlobalMetricVirtualizer->referenceVRoutinesForMetric(*this);

   // Since only a single stop/restart pair is returned above, it'll only
   // work when the number of vtimer's is exactly one.  Assert that.
   assert(getNumValues() == 1);

   dout << "countedVEvents: stop @ [kernel] "
        << addr2hex(stopAndRestartRoutines.first)
        << " and restart @ [kernel] "
        << addr2hex(stopAndRestartRoutines.second) << endl;

   // virtualization step 3/4: initialize the vtimer(s)
   initialize_timer(vtimerKernelAddr,
		    vtimerType,
		    theGlobalDataHeap->getElemsPerVectorVT(),
		    theGlobalDataHeap->getBytesPerStrideVT(),
		    stopAndRestartRoutines.first, // how-to-stop-if-nec
		    stopAndRestartRoutines.second); // how-to-restart
		    
   // virtualization step 4/4: call virtualizeInitializedTimer(), which will
   // start virtualizing it forthwith.  Of course, since the timer is initially
   // off, and no instrumentation code exists to turn it on, virtualization
   // code won't do much of anything yet, though the code is getting touched.
   theGlobalMetricVirtualizer->virtualizeInitializedTimer(vtimerKernelAddr);
      // pass the kernel address of this vtimer
   
   kapi_function kFunc;
   kmgr.findFunctionByAddress(theFocus.getFnAddr(), &kFunc);

   const bool basicBlock = (theFocus.getBBId() != (uint16_t)-1);
   kapi_basic_block kBlock;
   if (basicBlock) {
       if (kFunc.findBasicBlockById(theFocus.getBBId(), &kBlock) < 0) {
	   assert(false);
       }
   }

   pdvector<kptr_t> instrumentationPoints;

   // TO DO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   // For the moment, putting exit point snippets into the metricFocus' vec of
   // snippets first is ESSENTIAL.  This will force (at least in practice) the exit
   // points to be instrumented first.  THIS IS ESSENTIAL, because if the entry
   // points go in first, the timer will get set to wacky values as start-timer
   // instrumentation is executed but stop-timer instrumentation isn't executed because
   // it hasn't been put in yet (it will be there in a few millisecs but that's not
   // good enough).  This probably only appears in practice after kerninstd and
   // kperfmon were split into 2 processes...the real time between instrumentation
   // is suddenly quite high, and this once-rare bug became ubiquitous.
   // A REAL SOLUTION IS NEEDED TO SYNCH-UP "COOPERATIVE" CODE PATCHES!

   // First, do the exit points to the [function/basic block].

   if (basicBlock) {
       kptr_t exitAddr = kBlock.getExitAddr();
       instrumentationPoints += exitAddr;
   }
   else {
       // Exit points to function
       kapi_vector<kapi_point> fnExitPoints;
       if (kFunc.findExitPoints(&fnExitPoints) < 0) {
	   assert(false);
       }
       assert(fnExitPoints.size() > 0);
       kapi_vector<kapi_point>::const_iterator iter = fnExitPoints.begin();
       for (; iter != fnExitPoints.end(); iter++) {
	   instrumentationPoints += iter->getAddress();
       }
   }

   unsigned loop_count = 0;

   for (pdvector<kptr_t>::const_iterator iter = instrumentationPoints.begin();
        iter != instrumentationPoints.end(); ++iter) {
      const kptr_t launchAddr = *iter;
      assert(launchAddr != 0);

      // derived class must supply makeStopPredicate()
//      primitive *primptr = makeStopPrimitive(vtimerAddrs.first); // kernel address

      primitive *primptr = new vtimerCountedStopPrimitive(vtimerKernelAddr,
                                                          rawCtrExpr);
      
      if (theFocus.getPids().size() == 0) {
         snippet s(snippet::prereturn, *primptr);
         smf.addSnippet(launchAddr, s);
      }
      else {
         snippet s(snippet::prereturn,
                   pidInSetPredicate(theFocus.getPids()),
                   *primptr);
         smf.addSnippet(launchAddr, s);
      }

      delete primptr; // was allocated above

      ++loop_count;
   }

   // Second, do entry points to the [function/basic block]
   instrumentationPoints.clear(); // important, lest operator+= below do wacky things.
   
   if (basicBlock) {
       kptr_t startAddr = kBlock.getStartAddr();
       instrumentationPoints += startAddr;
   }
   else {
      instrumentationPoints += kFunc.getEntryAddr();
   }

   loop_count = 0;
   
   for (pdvector<kptr_t>::const_iterator iter = instrumentationPoints.begin();
        iter != instrumentationPoints.end(); ++iter) {
      const kptr_t launchAddr = *iter;
      assert(launchAddr != 0);

      primitive *primptr = new vtimerCountedStartPrimitive(vtimerKernelAddr,
                                                           rawCtrExpr);

      if (theFocus.getPids().size() == 0) {
         snippet s(snippet::preinsn, *primptr);
         smf.addSnippet(launchAddr, s);
      }
      else {
         snippet s(snippet::preinsn,
                   pidInSetPredicate(theFocus.getPids()),
                   *primptr);
         smf.addSnippet(launchAddr, s);
      }

      delete primptr; // was allocated with new
      
      ++loop_count;
   }
   
   // As requested:
   postInstantiateCB.smfInstantiationHasCompleted();
}

static pdvector<kptr_t>
simpleMetricFocus2VTimerKernelAddrs(const simpleMetricFocus &smf) {
   const pdvector<kptr_t> &result = smf.getvtimerKernelAddrs();

   const countedVEvents *theCountedVEvents = dynamic_cast<const countedVEvents*>(&smf.getSimpleMetric());
   assert(theCountedVEvents);
   
   assert(result.size() == theCountedVEvents->getNumValues());

   return result;
}

void countedVEvents::
cleanUpAfterUnsplice(const simpleMetricFocus &smf) const {
   // We have much to do when a metricFocus becomes uninstantiated.
   // unvirtualization steps:
   // 1/5: unsplice the "conventional" start & stop code.  We assume that this
   //      has already been done here.
   // 2/5: call unVirtualizeVTimer() to remove from all_vtimers[] and, if appropriate,
   //      unsplice calls to the cswitchout & in handlers.
   // 3/5: deallocate the vtimer at will
   // 4/5: call dereferenceVRoutinesForMetric(), which may
   //      un-download these how-to-stop and how-to-restart routines from the kernel.
   // 5/5: call dereferenceVirtualizationCode(), which may un-download the
   //      cswitchout and in handlers from the kernel.

   const pdvector<kptr_t> vtimerKernelAddrs = simpleMetricFocus2VTimerKernelAddrs(smf);
   assert(vtimerKernelAddrs.size() == getNumValues());
   assert(getNumValues() == 1);
      // I believe that this is all that'll work
   
   for (pdvector<kptr_t>::const_iterator iter = vtimerKernelAddrs.begin();
        iter != vtimerKernelAddrs.end(); ++iter)
      theGlobalMetricVirtualizer->unVirtualizeVTimer(*iter);

   theGlobalMetricVirtualizer->dereferenceVRoutinesForMetric(*this);

   theGlobalMetricVirtualizer->dereferenceVirtualizationCode();
}
