// add1AtExitNoUnwindTailCalls.C

#include "add1AtExitNoUnwindTailCalls.h"
#include "pidInSetPredicate.h"
#include "add2IntCounter64.h"
#include "focus.h"
#include "kapi.h"

extern kapi_manager kmgr;

pdvector<kptr_t> add1AtExitNoUnwindTailCalls::getInstrumentationPoints(
    const focus &theFocus) const 
{
    kapi_module kMod;
    kapi_function kFunc;
    
    if (kmgr.findModuleAndFunctionByAddress(theFocus.getFnAddr(),
					    &kMod, &kFunc) < 0) {
	assert(false);
    }

    pdvector<kptr_t> instrumentationPoints; // to return
    const bool basicBlock = (theFocus.getBBId() != (bbid_t)-1);

    // just before fn exit, BUT make sure to unwind any tail call
    // create instrumentation for all exit point(s) to this fn.  Ask fn where
    // its exit points are; it knows best.

    if (!basicBlock) {
	// no basic block; entire function was specified.
	kapi_vector<kapi_point> exitPoints;
	if (kFunc.findExitPoints(&exitPoints) < 0) {
	    assert(false);
	}
	kapi_vector<kapi_point>::const_iterator iter = exitPoints.begin();
	for (; iter != exitPoints.end(); iter++) {
	    instrumentationPoints.push_back(iter->getAddress());
	}
    }
    else {
	// A basic block was specified.
	kapi_basic_block kBlock;
	if (kFunc.findBasicBlockById(theFocus.getBBId(), &kBlock) < 0) {
	    assert(false);
	}
	instrumentationPoints.push_back(kBlock.getStartAddr());
    }
    return instrumentationPoints;
}

snippet::placement add1AtExitNoUnwindTailCalls::getPlacement(const focus &) const {
   return snippet::preinsn; // not prereturn (which would unwind tail calls)
}
