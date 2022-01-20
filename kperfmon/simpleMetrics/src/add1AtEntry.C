// add1AtEntry.C

#include "add1AtEntry.h"
#include "pidInSetPredicate.h"
#include "add2IntCounter64.h"
#include "focus.h"
#include "kapi.h"

extern kapi_manager kmgr;

pdvector<kptr_t>
add1AtEntry::getInstrumentationPoints(const focus &theFocus) const 
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
	kapi_vector<kapi_point> points;
	switch (theFocus.getLocation()) {
	case 0: // function entry
	    if (kFunc.findEntryPoint(&points) < 0) {
		assert(false);
	    }
	    break;
	case 1: // just before function exit
	    if (kFunc.findExitPoints(&points) < 0) {
		assert(false);
	    }
	    break;
	case 2:
            // a specific instruction within the function: no longer supported
	    assert(false && "Point type no longer supported");
	    break;
	default:
            assert(false);
	}

	kapi_vector<kapi_point>::const_iterator ptiter = points.begin();
	for (; ptiter != points.end(); ptiter++) {
	    instrumentationPoints.push_back(ptiter->getAddress());
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

snippet::placement add1AtEntry::getPlacement(const focus &theFocus) const {
   if (theFocus.getLocation() == 1)
      return snippet::prereturn;
   else
      return snippet::preinsn;
}

