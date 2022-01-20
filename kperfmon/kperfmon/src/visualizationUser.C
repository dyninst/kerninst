// visualizationUser.C
// Our side (as opposed to in-visi) of the routines in visi.I.  We don't implement
// all routines in visi.I since some are implemented by the other side (the visis).

#include "visi.xdr.CLNT.h" // created by igen automatically
#include "tkTools.h"
#include "abstractions.h" // where axis & stuff
#include "util/h/Dictionary.h"
#include "visiSubscriber.h"
#include "allSampleHandlers.h"
#include "instrument.h" // launchInstrumentFromCurrentSelections()

extern Tcl_Interp *global_interp;

extern allSampleHandlers *globalAllSampleHandlers;

extern Tcl_TimerToken theTimerToken;

void visualizationUser::GetPhaseInfo() {
   // visi asking us to list the currently defined phases
   pdvector<phase_info> phases;
   this->PhaseData(phases);
}

static visiSubscriber* findVisiFromIgenHandle(const visualizationUser *igenHandle) {
   const dictionary_hash<unsigned, cmfSubscriber*> &allCmfSubscribers = cmfSubscriber::getAllCmfSubscribers();
   
   for (dictionary_hash<unsigned,cmfSubscriber*>::const_iterator iter = allCmfSubscribers.begin(); iter != allCmfSubscribers.end(); ++iter) {
      visiSubscriber *visi = dynamic_cast<visiSubscriber*>(iter.currval());
      if (visi == NULL)
         // wrong type, not a visi
         continue;
      
      if (visi->compareIgenHandle(igenHandle))
         return visi;
   }
   
   assert(false && "findVisiFromIgenHandle failed");
}

void visualizationUser::GetMetricResource(pdstring, int, int) {
   // visi asking us to instrument selected metric/focus pairs, and to
   // start sending that data to it.

   visiSubscriber *theVisi = findVisiFromIgenHandle(this);
   asyncInstrumentCurrentSelections_thenSample(*theVisi, global_interp);
}

void visualizationUser::StopMetricResource(u_int metricid, u_int focusid) {
   // visi asking us to disable data collection for m/f pair

   // Find the visi where this message came from:
   visiSubscriber *theVisi = findVisiFromIgenHandle(this);

   // Find the metricFocus:
   complexMetricFocus *cmf = globalAllSampleHandlers->findMetricFocus(metricid,
								      focusid);
   assert(cmf != 0);

   unsubscribe(*theVisi, *cmf); // instrument.h
      // uninstruments the kernel and fries theMetricFocus if no subscribers are left
      // subscribed to the cmf).
}

void visualizationUser::StartPhase(double, pdstring, bool, bool) {
   // visi asking us to start a new phase
   assert(0);
}


void visualizationUser::showError(int code, pdstring msg) {
   // visi asking us to show an error
   cerr << "visualizationUser::showError: code=" << code
        << "msg=" << msg << endl;
}


