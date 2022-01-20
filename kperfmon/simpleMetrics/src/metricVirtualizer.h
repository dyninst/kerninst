#ifndef _METRIC_VIRTUALIZER_H_
#define _METRIC_VIRTUALIZER_H_

#include "kapi.h"
#include "util/h/Dictionary.h"
#include "util/h/hashFns.h"
#include "countedVEvents.h"

class metricVirtualizer {
    unsigned numReferences; // If >0 then certain code is already in the kernel
    kptr_t allVTimersAddr;

    kapi_vector<int> callInReqids, callOutReqids;

    // Virtualization: dictionary of metric-specific "vstop-if-needed" and
    // "vrestart" routines:
    static unsigned simpleMetricPtrHash(const simpleMetric * const &ptr) {
	const simpleMetric &theMetric = *ptr;
	return theMetric.getId();
    }

    // VRoutines are howToStopIfNeeded and howToRestart.  They are specific
    // to the (simple) metric.
    struct simpleMetricSpecificVRoutines { // v stands for virtualization
	unsigned refCount;
	unsigned stopIfNeededReqId;
	kptr_t stopIfNeededKernelAddr;
	unsigned restartReqId;
	kptr_t restartKernelAddr;
    };
    dictionary_hash<const simpleMetric*,
	simpleMetricSpecificVRoutines> theSimpleMetricSpecificVRoutines;
  
    // Routines for managing the list of all vtimers
    void add_timer_to_all_vtimers(kptr_t vtimer_addr);
    void remove_timer_from_all_vtimers(kptr_t vtimer_addr);

    // Routines for instrumenting/deinstrumenting the context switch code
    void instrument_switch_points(kapi_point_location type,
				  kapi_vector<int> &callReqids);

    void deinstrument_switch_points(const kapi_vector<int> &callReqids);

 public:
    metricVirtualizer();
    ~metricVirtualizer();

    // If refcount == 0, allocates all_vtimers, uploads and splices the
    // virtualization code. Increments refcount.
    void referenceVirtualizationCode();

    // Decrements refcount. If refcount == 0, unsplices virtualization,
    // deallocates all_vtimers
    void dereferenceVirtualizationCode();

    // Asks the metric to generate stop and restart expressions, emits
    // code for them and uploads it into the kernel. Returns entry
    // addresses of the vroutines
    std::pair<kptr_t, kptr_t>
	referenceVRoutinesForMetric(const countedVEvents &theMetric);

    // Removes the stop and restart routines from the kernel
    void dereferenceVRoutinesForMetric(const countedVEvents &theMetric);

    // Appends the timer to all_vtimers. This effectively starts its
    // virtualization -- the context switch-out code is now able to see it
    void virtualizeInitializedTimer(kptr_t vtimerAddrInKernel);

    // Remove the timer from all_vtimers
    void unVirtualizeVTimer(kptr_t vtimerAddrInKernel);
};

#endif
