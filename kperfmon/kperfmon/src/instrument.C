// instrument.C

#include <inttypes.h> // uint32_t
#include "util/h/out_streams.h"
#include "util/h/Dictionary.h"
#include "abstractions.h"
#include "instrument.h"
#include "kpmChildrenOracle.h"
#include "focus.h"
#include "complexMetricFocus.h"
#include "metricFocusManager.h"
#include "allComplexMetrics.h"
#include "sampleHandler.h"
#include "allSampleHandlers.h"
#include "kapi.h"

static unsigned resHandleHashf(const unsigned &val) {
   unsigned result = 5381;
   unsigned working = val;

   while (working) { // siphon off 3 bits at a time (arbitrary choice)
      result = (result << 5) + result + (working & 0x7);
      working >>= 3;
   }

   return result;
}

unsigned codeWhereAxisId=1; // must stay in sync with the value init'zd in tclCommands.C
dictionary_hash<unsigned, pdstring> xmoduleWhereAxisId2ModuleName(resHandleHashf);
dictionary_hash<pdstring, unsigned> moduleName2ModuleWhereAxisId(stringHash);
dictionary_hash<unsigned, kptr_t> xfnWhereAxisId2FnAddr(resHandleHashf);
dictionary_hash<kptr_t, unsigned> fnAddr2FnWhereAxisId(addrHash4);
dictionary_hash<unsigned, unsigned> bbWhereAxisId2BBNum(resHandleHashf);

extern abstractions<kpmChildrenOracle> *global_abstractions; // main.C
extern metricFocusManager *theGlobalMetricFocusManager; // main.C
extern allSampleHandlers *globalAllSampleHandlers; // main.C
extern allComplexMetrics global_allMetrics; // main.C
extern kapi_manager kmgr; // main.C
// ------------------------------------------------------------

bool sampleCodeBufferer::
handleSamplingCode(complexMetricFocus *cmf) {
   oneSampleInfo *info = allTheSamplingCode.append_with_inplace_construction();
   new((void*)info)oneSampleInfo(cmf);

   assert(numMetricFocusesDoneInstrumented < numMetricFocusesBeingInstrumented);

   if (++numMetricFocusesDoneInstrumented == numMetricFocusesBeingInstrumented) {
      allTheSamplingCodeIsReady(allTheSamplingCode); // user-specifified routine
      return true;
   }
   else
      return false;
}

bool sampleCodeBufferer::noSamplingNeeded_AlreadyBeingDone() {
   if (++numMetricFocusesDoneInstrumented == numMetricFocusesBeingInstrumented) {
      allTheSamplingCodeIsReady(allTheSamplingCode); // user-specifified routine
      return true;
   }
   else
      return false;
}

// ----------------------------------------------------------------------

void sampleCodeBufferer_genesisCMFSamplesWhenDone::
allTheSamplingCodeIsReady(const pdvector<oneSampleInfo> &theSamplingCodes) const {
   pdvector<oneSampleInfo>::const_iterator iter = theSamplingCodes.begin();
   pdvector<oneSampleInfo>::const_iterator finish = theSamplingCodes.end();
   for (; iter != finish; ++iter) {
      complexMetricFocus *cmf = iter->cmf;
      if (!cmf->isDisabled()) {
	  theMFManager.reqGenesisEstablishingCMFSample(cmf);
      }
   }
}

// ------------------------------------------------------------

static void
parse1CodeFocus(const whereAxis<kpmChildrenOracle>::focus &theFocus,
                pdstring &modname, // set to empty string if no module chosen (whole prog)
                kptr_t &fnaddr, // set to UINT_MAX if no fn chosen (whole module)
                unsigned &bbnum // set to -1 if no bb chosen (whole fn)
                ) {
   // we don't return a focusForNow since the where axis focus doesn't 
   // include e.g. location & innsnum.
   assert(theFocus.size() == 1);
   const unsigned resid = theFocus[0];
   
   // ok, now we've got an id for the resource.  expand to its full 
   // path name of resids.
   pdvector<unsigned> res_components =
      global_abstractions->expandResourceIdInCurrAbs(resid);

   // It can have a varying number of components

   modname = ""; // empty string --> no module (for now)
   fnaddr = 0;
   bbnum = UINT_MAX;

   unsigned res_comp_size = res_components.size();

   if (res_comp_size == 0)
      assert(false);

   if ((res_comp_size == 1)     // '/root'
       || (res_comp_size == 2)  // '/root/code'
       || (res_comp_size == 3)) // '/root/code/module'
      return;

   if (res_comp_size > 3) {
      // at least '/root/code/module/func'...
      const unsigned module_resid = res_components[2];

      // convert resource ids to numbers that our moduleMgr likes
      if (!xmoduleWhereAxisId2ModuleName.find(module_resid, modname))
         assert(false && "unknown module resid in dictionary lookup?");
   }
   
   if (res_comp_size >= 4) {
      // at least '/root/code/module/func'...
      const unsigned fn_resid = res_components[3];

      if (!xfnWhereAxisId2FnAddr.find(fn_resid, fnaddr))
         assert(false && "unknown fn resid in dictionary_lookup?");
   }

   if (res_comp_size == 5) {
      // at least '/root/code/module/func/bb'...

      const unsigned bb_resid = res_components[4];
      if (!bbWhereAxisId2BBNum.find(bb_resid, bbnum))
         assert(false && "unknown bb resid in dictionary_lookup?");
   }

   assert(res_comp_size <= 5); // beyond basic blocks not (yet?) supported
}

pdvector<unsigned> getCurrSelectedMetrics(Tcl_Interp *interp) {
   myTclEval(interp, "getSelectedMetrics");
   
   int argc;
   char **argv;
   if (TCL_OK != Tcl_SplitList(interp, interp->result, &argc, 
                               (TCLCONST char ***)&argv))
      assert(false);
   
   pdvector<unsigned> metrics;

   if (argc == 0) {
      Tcl_Free((char*)argv);
      return metrics;
   }

   char **metric_string_ptr = argv;
   while (*metric_string_ptr)
      metrics += atoi(*metric_string_ptr++);

   Tcl_Free((char*)argv);
   return metrics;
}

pdvector<focus> getCurrSelectedFoci(Tcl_Interp *interp) {
   // get location:
   const char *location_str = Tcl_GetVar(interp, "location", TCL_GLOBAL_ONLY);
   assert(location_str);
   int location = atoi(location_str);

   // get instruction #:
   myTclEval(interp, "getFocusInstructionNum");
   const char *insnnum_str = interp->result;
   const unsigned insnnum = atoi(insnnum_str);


   // get selected pids, if any
   pdvector<long> pids;
   myTclEval(interp, "getFocusPids");
   int argc;
   char **argv;
   pdstring origPidList(Tcl_GetStringResult(interp));
   if (TCL_OK != Tcl_SplitList(interp, interp->result, &argc, 
                               (TCLCONST char ***)&argv)) {
      cout << "Parse error splitting the list.  Ignoring pid selections\n"
           << "the string was \"" << origPidList << "\"\n";
   }
   else {
      if (argc > 0) {
         char **pid_string_ptr = argv;
         while (*pid_string_ptr)
            pids += atol(*pid_string_ptr++);

      }

      // we want to call this, even when argc==0, else mem leak
      Tcl_Free((char*)argv);
   }

   assert(global_abstractions);
   pdvector<whereAxis<kpmChildrenOracle>::focus> foci = 
       global_abstractions->getCurrAbsSelectionsAsFoci();

   const pdvector<int>& cpuids = 
       global_abstractions->getCurrentCPUSelections();

   pdvector<focus> result;
   for (pdvector<whereAxis<kpmChildrenOracle>::focus>::const_iterator iter = 
	    foci.begin(); iter != foci.end(); iter++) {
      const whereAxis<kpmChildrenOracle>::focus &theFocus = *iter;

      pdstring modname;
      kptr_t fnaddr;
      unsigned bbnum;
      parse1CodeFocus(theFocus, modname, fnaddr, bbnum);
      // as appropriate, will set modname to emptystring and set fnnum & bbnum
      // to UINT_MAX when undefined.

      unsigned calledfrom_mod = 0;
      unsigned calledfrom_fn = 0;

      pdvector<int>::const_iterator icpu = cpuids.begin();
      for (; icpu != cpuids.end(); icpu++) {
         if((modname != "") && (fnaddr != (kptr_t)0))
            result += focus::makeFocus(modname, fnaddr, bbnum, 
                                       location, insnnum, calledfrom_mod, 
                                       calledfrom_fn, pids, *icpu);
         // focus::makeFocus() takes in all the fields of a focus,
         // except its id, and returns a focus (with id set).  The id of the
         // returned focus will equal one which we have seen already, if all 
         // of the fields (except id) match.  Otherwise, a new unique focus 
         // id will be chosen for the returned focus.
      }
   }

   return result;
}

class selections {
 private:
   pdvector<unsigned> selected_metrics;
   pdvector<focus> selected_foci;

   // explicitly private to disallow:
   selections& operator=(selections &);

 public:
   selections(const pdvector<unsigned> &imets, const pdvector<focus> &ifns) :
         selected_metrics(imets), selected_foci(ifns) {
   }
   selections(const selections &src) :
      selected_metrics(src.selected_metrics), selected_foci(src.selected_foci) {
   }
   
   const pdvector<unsigned> getSelectedMetrics() const {
      return selected_metrics;
   }
   const pdvector<focus> getSelectedFoci() const {
      return selected_foci;
   }
};

selections getCurrentSelections(Tcl_Interp *interp) {
   pdvector<unsigned> metrics = getCurrSelectedMetrics(interp);
   pdvector<focus> foci = getCurrSelectedFoci(interp);

   return selections(metrics, foci);
}

class completeInstrumentFrom1Selection : public postCMFInstantiationCallback {
 private:
   typedef postCMFInstantiationCallback inherited;
   cmfSubscriber &theSubscriber; // e.g. a visi
   bool needsInstrumentation;
   sampleCodeBufferer &howToHandleSamplingCode;
      // NOT dup()'d; make sure it won't point to dangling memory!
   
 public:
   completeInstrumentFrom1Selection(cmfSubscriber &icmfSubscriber,
                                    bool iNeedsInstrumentation,
                                    sampleCodeBufferer &ihowToHandleSamplingCode) :
         theSubscriber(icmfSubscriber),
         howToHandleSamplingCode(ihowToHandleSamplingCode) {
      needsInstrumentation = iNeedsInstrumentation;
   }
   completeInstrumentFrom1Selection(const completeInstrumentFrom1Selection &src) :
         inherited(src), theSubscriber(src.theSubscriber),
         howToHandleSamplingCode(src.howToHandleSamplingCode) {
      needsInstrumentation = src.needsInstrumentation;
   }
  ~completeInstrumentFrom1Selection() {}

   inherited *dup() const {
      return new completeInstrumentFrom1Selection(*this);
   }

    void operator()(complexMetricFocus &mf);
};

void completeInstrumentFrom1Selection::
operator()(complexMetricFocus &theMetricFocus)
{
    // The metric/focus pair has been instantiated: any counters /
    // timers have been allocated (if it hadn't already been done).  What's
    // left: asynchronously instrument the kernel and start sampling, if
    // those things haven't already been done.

    if (needsInstrumentation) {
	// Instrument the kernel and begin sampling
	assert(theGlobalMetricFocusManager);
	
	sampleHandler &theHandler = theMetricFocus.getSampleHandler();
	theGlobalMetricFocusManager->
	    spliceButDoNotSampleInstantiatedComplexMetricFocus(theHandler);
	
	howToHandleSamplingCode.handleSamplingCode(&theMetricFocus);
    }
    else {
	// This code is executed when instrumentation isn't needed.
	howToHandleSamplingCode.noSamplingNeeded_AlreadyBeingDone();
    }
}

static void disableSomeCMFs(const pdvector<sampleHandler*> &handlersToFry)
{
    for (pdvector<sampleHandler*>::const_iterator iter = handlersToFry.begin(); 
	 iter != handlersToFry.end(); iter++) {
	sampleHandler *theHandler = *iter;

	// Inform subscribers(s) to this handler that it has 
	// been disabled:
	theHandler->disableMe();
    }
}

void destroySomeCMFsDueToRemovalOfDownloadedKernelCode(kptr_t fnEntryAddr) {
    pdvector<sampleHandler *> sampleHandlersToFry; // initially empty

    allSampleHandlers::const_iterator iter = globalAllSampleHandlers->begin();
    allSampleHandlers::const_iterator finish = globalAllSampleHandlers->end();
    for (; iter != finish; iter++) {
	sampleHandler *existingHandler = *iter;

	// If this SH is presently disabled, then ignore it.  (If we
	// forget this check, then we'll probably end up adding it to
	// metricFocusesToFry, resulting in us trying to uninstrument this
	// complexMetricFocus that has already been uninstrumented -- 
	// leading to an assert failure.)
	if (existingHandler->isDisabled())
	    continue;
      
	const focus &theExistingFocus = existingHandler->getFocus();

	if (theExistingFocus.isRoot()) continue;
	if (theExistingFocus.isModule()) continue;
	if (theExistingFocus.getFnAddr() == fnEntryAddr) {
	    cout << "NOTE: disabling metric/resource pairing \""
		 << existingHandler->getMetric().getName() << "\", \""
		 << existingHandler->getFocus().getName() << "\"" << endl;
	    cout << "Due to immiment removal of downloaded code" << endl;

	    sampleHandlersToFry += existingHandler;
	}
    }

    disableSomeCMFs(sampleHandlersToFry);
}

static void disableSomeCMFsDueToProposedPcrChange(
    const kapi_hwcounter_set &/*oldPcrValue*/,
    const kapi_hwcounter_set &proposedNewPcrValue,
    sampleHandler &theNewHandler) 
{
    pdvector<sampleHandler *> sampleHandlersToFry; // initially empty

    allSampleHandlers::const_iterator iter = globalAllSampleHandlers->begin();
    allSampleHandlers::const_iterator finish = globalAllSampleHandlers->end();
    for (; iter != finish; iter++) {
	sampleHandler *existingHandler = *iter;
	
	// ignore undefined complexMetricFocus being added now.
	if (existingHandler == &theNewHandler) continue;

	// If this CMF is presently disabled, then ignore it.  (If we
	// forget this check, then we'll probably end up adding it to
	// metricFocusesToFry, resulting in us trying to uninstrument 
	// this complexMetricFocus that has already been uninstrumented 
	// -- leading to an assert failure.)
	if (existingHandler->isDisabled())
	    continue;
      
	const complexMetric &theExistingMetric = existingHandler->getMetric();
	const focus &theExistingFocus = existingHandler->getFocus();
	
	if (theExistingMetric.changeInPcrWouldInvalidateMF(proposedNewPcrValue,
							   theExistingFocus)) {
	    cout << "NOTE: disabling metric/resource pairing \""
		 << existingHandler->getMetric().getName() << "\", \""
		 << existingHandler->getFocus().getName() << "\""
		 << "since its performance counter settings are incompatible with those of the newly requested metric"
		 << endl;
      
	    sampleHandlersToFry += existingHandler;
	}
    }

    disableSomeCMFs(sampleHandlersToFry);
}

static void
launchInstantiate1ComplexMetricFocus(complexMetricFocus &theNewMetricFocus,
                                     completeInstrumentFrom1Selection &postCMFInstantiateCallback) {
   // A CMF needs to be instantiated (i.e., pretty much only its metric and
   // focus identifier fields have been initialized).
   // This routine does it.

   const complexMetric &theCMetric = theNewMetricFocus.getMetric();
   const focus &theFocus = theNewMetricFocus.getFocus();
   sampleHandler &theHandler = theNewMetricFocus.getSampleHandler();

   // If this new metric/focus is going to use certain %pcr settings
   // that conflict with an existing metric/focus, then disable the previous
   // metric/focus(es) in favor of the new guy.
   kapi_hwcounter_set oldPcrValue;
   if (oldPcrValue.readConfig() < 0) {
       assert(false);
   }
   const kapi_hwcounter_set newPcrValue = 
       theCMetric.queryPostInstrumentationPcr(oldPcrValue, theFocus);

   if (!newPcrValue.equals(oldPcrValue)) {
       disableSomeCMFsDueToProposedPcrChange(oldPcrValue, newPcrValue, 
					     theHandler);
   }
   
   theCMetric.asyncInstantiate(theNewMetricFocus, postCMFInstantiateCallback);
      // virtual fn call; depends on the metric
}

void asyncInstrument1Selection(cmfSubscriber &theSubscriber,
                               const complexMetric &theCMetric,
                               const focus &theFocus,
                               sampleCodeBufferer &howToHandleSamplingCode) 
{
    // 1) Verify that this metric/focus combination is OK
    // 2) SampleHandler creation: Ask allSHs for a SH structure (it'll either 
    //    create a new one, or return an existing one if appropriate.
    // 3) CMF creation. Bind it to the subscriber
    // 4) SH Instantiation: Ask the complexMetric to instantiate the SH

    if (!theCMetric.canInstantiateFocus(theFocus)) {
	cout << "Metric \"" << theCMetric.getName()
	     << "\" cannot be applied to the resource \""
	     << theFocus.getName() << "\"" << endl;
	return;
    }

    // 2) SampleHandler creation:
    bool found;
    sampleHandler &theHandler = 
	globalAllSampleHandlers->findOrCreateHandler(theCMetric, theFocus, 
						     &found);
    bool needsInstantiation = (!found);

    if (theHandler.isDisabled()) {
	assert(!needsInstantiation);
	needsInstantiation = true;

	theHandler.unDisableMe();
    }

    // 3) CMF creation
    complexMetricFocus &cmf = theHandler.findOrCreateMetricFocus(theFocus);
    const unsigned mhz = kmgr.getSystemClockMHZ();
    bool added = cmf.subscribe(theSubscriber, false);
                               // false --> don't bomb if already subscribed.
    if (added) {
	theSubscriber.newMetricFocusPair(cmf, mhz);
    }

    // 4) sampleHander instantiation, if needed:
    completeInstrumentFrom1Selection 
	postCMFInstantiateCallback(theSubscriber, needsInstantiation,
				   howToHandleSamplingCode);
    if (needsInstantiation) {
	// Using logic from 'theCMetric', instantiate this metric-focus pair:
	// asynchronous; postCMFInstantiateCallback() will be called when ready
	launchInstantiate1ComplexMetricFocus(cmf, postCMFInstantiateCallback);
    }
    else {
	//  manually invoke the "already-instrumented" version of the callback
	postCMFInstantiateCallback(cmf);
    }
}

void asyncInstrumentCurrentSelections_thenSample(cmfSubscriber &theSubscriber,
                                                 Tcl_Interp *interp) {
   // Doesn't return anything, since it can't be made synchronous in the presence
   // of asynchronous-only igen calls.  The problem is complexMetricFocus instantiation
   // which'll need to make calls to kerninstd to, e.g. to mmap a variable, which
   // isn't synchronous.

   // (We used to temporarily turn off sampling now, but that would cause the
   // genesis sample to not get sent)

   const selections theSelections = getCurrentSelections(interp);

   const pdvector<unsigned> &metrics = theSelections.getSelectedMetrics();
   const pdvector<focus> &foci = theSelections.getSelectedFoci();

   sampleCodeBufferer_genesisCMFSamplesWhenDone
      mySampleCodeBufferer(metrics.size() * foci.size(),
                           *theGlobalMetricFocusManager);

   for (pdvector<unsigned>::const_iterator iter = metrics.begin();
        iter != metrics.end(); ++iter) {
      const unsigned metricnum = *iter;
      const complexMetric &theMetric = global_allMetrics.getById(metricnum);

      for (unsigned focuslcv=0; focuslcv < foci.size(); ++focuslcv) {
         const focus &theFocus = foci[focuslcv];

         asyncInstrument1Selection(theSubscriber,
                                   theMetric,
                                   theFocus,
                                   mySampleCodeBufferer);
      }
   }

#ifdef sparc_sun_solaris2_7
   // %pcr/%pic settings may have changed; update the GUI
   myTclEval(interp, "updatePcrPicGUIFromKernelSettings");
#endif
}

// --------------------

void unsubscribe(cmfSubscriber &theSubscriber, // this subscriber...
                 complexMetricFocus &cmf) // no longer wants this metric/focus
{
    // uninstruments the kernel if applicable (if no subscribers are left 
    // subscribed to cmf)

    if (cmf.unsubscribe(theSubscriber)) {
	// cmf has no more subscribers, so uninstrument the kernel and
	// stop sampling.  As usual, this takes place asynchronously, so be
	// prepared for some straggler igen sampling messages.  Just drop such
	// msgs on the floor.

	sampleHandler& theHandler = cmf.getSampleHandler();

	theHandler.removeMetric(&cmf);

	if (!theHandler.isReferenced()) {
	    theHandler.uninstrument(false);
	    globalAllSampleHandlers->fry1(theHandler);   
	}
    }
    else {
	dout << "unsubscribe: didn't uninstrument/unsample "
	     << "(still subscriber(s))" << endl;
    }
}

void fry1CmfSubscriberAndUninstrument(cmfSubscriber *theSubscriber,
                                      bool kperfmonGoingDownNow) 
{
    allSampleHandlers::const_iterator iall = globalAllSampleHandlers->begin();
    allSampleHandlers::const_iterator iallend = globalAllSampleHandlers->end();
    pdvector<sampleHandler*> handlersToFry;

    for (; iall != iallend; iall++) {
	sampleHandler *theHandler = (*iall);
	theHandler->frySubscribedMetrics(theSubscriber);
	if (!theHandler->isReferenced()) {
	    handlersToFry += theHandler;
	}
    }
    
    pdvector<sampleHandler*>::iterator ifry = handlersToFry.begin();
    pdvector<sampleHandler*>::iterator ifryend = handlersToFry.end();
    
    for (; ifry != ifryend; ifry++) {
	sampleHandler *theHandler = (*ifry);
	theHandler->uninstrument(kperfmonGoingDownNow);
	globalAllSampleHandlers->fry1(*theHandler);
    }

    theSubscriber->closeDownShop();
    // Here is the only time we should actually call delete
    // on a cmfSubscriber (beyond just closeDownShop()).
    // also removes from the dictionary of all cmfSubscribers
    delete theSubscriber;
    theSubscriber = NULL; // help purify find mem leaks
}

int kperfmonSamplingCallback(unsigned reqid, uint64_t time, 
			     const sample_t *values, unsigned numvalues)
{
    assert(theGlobalMetricFocusManager != 0);
    theGlobalMetricFocusManager->handleOneSample(reqid, time,
						 values, numvalues);
    return 0;
}
