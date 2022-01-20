#include <string.h>
#include "metricVirtualizer.h"
#include "all_vtimers.h"
#include "kapi.h"

extern kapi_manager kmgr;

void metricVirtualizer::add_timer_to_all_vtimers(kptr_t vtimer_addr)
{
    kmgr.addTimerToAllVTimers(vtimer_addr);
}

void metricVirtualizer::remove_timer_from_all_vtimers(kptr_t vtimer_addr)
{
    kmgr.removeTimerFromAllVTimers(vtimer_addr);
}

void metricVirtualizer::instrument_switch_points(kapi_point_location type,
                                                 kapi_vector<int> &callReqids)
{
    kapi_vector<kapi_point> cswitchPoints;

    if (kmgr.findSpecialPoints(type, &cswitchPoints) < 0) {
	assert(false);
    }

    kapi_module kdriver_mod;
    if(kmgr.findModule("kerninst", &kdriver_mod) < 0) {
	assert(!"could not locate kerninst module\n");
    }
    kapi_function kfunc;
    if(type == kapi_cswitch_in) {
       if (kdriver_mod.findFunction("on_switch_in", &kfunc) < 0) {
          assert(!"could not locate on_switch_in() function\n");
       }
    }
    else {
       if (kdriver_mod.findFunction("on_switch_out", &kfunc) < 0) {
          assert(!"could not locate on_switch_out() function\n");
       }
    }
    kptr_t cswitch_addr = kfunc.getEntryAddr();

    kapi_vector<kapi_snippet> empty;
    kapi_call_expr my_call(cswitch_addr, empty);

    kapi_vector<kapi_point>::const_iterator ipoint = cswitchPoints.begin();
    for (; ipoint != cswitchPoints.end(); ipoint++) {
	int id = kmgr.insertSnippet(my_call, *ipoint);
	if (id < 0) {
	    assert(false);
	}
	callReqids.push_back(id);
    }
}

void metricVirtualizer::deinstrument_switch_points(const kapi_vector<int> &callReqids)
{
    kapi_vector<int>::const_iterator icall = callReqids.begin();
    for (; icall != callReqids.end(); icall++) {
	if (kmgr.removeSnippet(*icall) < 0) {
	    assert(false);
	}
    }
}

metricVirtualizer::~metricVirtualizer() 
{
    assert(numReferences == 0);
    assert(allVTimersAddr == 0);
}

metricVirtualizer::metricVirtualizer() : numReferences(0), allVTimersAddr(0), 
    theSimpleMetricSpecificVRoutines(simpleMetricPtrHash)
{
}

void metricVirtualizer::referenceVirtualizationCode() 
{
    // If reference count was zero, instruments the switch-in/out points
    // in the kernel to call the context-switch handling code in the 
    // kerninst driver

    if (numReferences++ == 0) {
	// Install all_vtimers: a vector to list all available vtimers
	assert(allVTimersAddr == 0);
	allVTimersAddr = kmgr.getAllVTimersAddr();
	assert(allVTimersAddr != 0);
	
	// Instrument the kernel at the context-switch points
	instrument_switch_points(kapi_cswitch_in, callInReqids);
	instrument_switch_points(kapi_cswitch_out, callOutReqids);
    }
}

void metricVirtualizer::dereferenceVirtualizationCode()
{
    assert(numReferences > 0);
    numReferences--;
    if (numReferences == 0) {
	deinstrument_switch_points(callOutReqids);
	deinstrument_switch_points(callInReqids);
	allVTimersAddr = 0;
	callInReqids.clear();
	callOutReqids.clear();
    }
}

std::pair<kptr_t, kptr_t>
metricVirtualizer::referenceVRoutinesForMetric(
    const countedVEvents &theSimpleMetric) 
{
    // installs if needed; otherwise just bumps up refcount
    if (!theSimpleMetricSpecificVRoutines.defines(&theSimpleMetric)) {
	// We need to install the routines
	assert(theSimpleMetric.getNumValues() == 1);

        int vrestart_id = 0;
        int vstop_id = 0;

        kapi_module kmod;
        if(kmgr.findModule("kerninst", &kmod) < 0) {
           assert(!"could not locate kerninst module\n");
        }
        kapi_function vrestart_func, vstop_func;
        if (kmod.findFunction("generic_vrestart_routine", &vrestart_func) < 0) {
           assert(!"could not locate generic_vrestart_routine() function\n");
        }
        if (kmod.findFunction("generic_vstop_routine", &vstop_func) < 0) {
           assert(!"could not locate generic_vstop_routine() function\n");
        }
        kptr_t vrestart_addr = vrestart_func.getEntryAddr();
        kptr_t vstop_addr = vstop_func.getEntryAddr();

	simpleMetricSpecificVRoutines theVRoutines;
	theVRoutines.refCount = 0;
	theVRoutines.stopIfNeededReqId = vstop_id;
	theVRoutines.stopIfNeededKernelAddr = vstop_addr;
	theVRoutines.restartReqId = vrestart_id;
	theVRoutines.restartKernelAddr = vrestart_addr;
	theSimpleMetricSpecificVRoutines.set(&theSimpleMetric, theVRoutines);
    }

    simpleMetricSpecificVRoutines &theVRoutines =
	theSimpleMetricSpecificVRoutines.get(&theSimpleMetric);
    ++theVRoutines.refCount;

    return std::make_pair(theVRoutines.stopIfNeededKernelAddr,
                          theVRoutines.restartKernelAddr);
}

void metricVirtualizer::dereferenceVRoutinesForMetric(
    const countedVEvents &theSimpleMetric) 
{
    simpleMetricSpecificVRoutines &theRoutines = 
	theSimpleMetricSpecificVRoutines.get(&theSimpleMetric);

    assert(theRoutines.refCount);
    if (--theRoutines.refCount == 0) {
	theSimpleMetricSpecificVRoutines.undef(&theSimpleMetric);
    }
}

void metricVirtualizer::virtualizeInitializedTimer(kptr_t vtimerAddrInKernel) 
{
    // invariants: vtimer exists and has had its fields fully
    // initialized, including the how-to-stop and how-to-restart fields (for
    // the benefit of cswitch handler routines).  cswitch stuff has been 
    // spliced into the kernel. We can now add vtimerAddrInKernel to 
    // all_vtimers[]

    assert(allVTimersAddr != 0);
    add_timer_to_all_vtimers(vtimerAddrInKernel);
}

void metricVirtualizer::unVirtualizeVTimer(kptr_t vtimerAddrInKernel) 
{
    // Remove vtimerAddrInKernel from all_vtimers[]
    
    assert(allVTimersAddr != 0);
    remove_timer_from_all_vtimers(vtimerAddrInKernel);
}
