// exitsFromNoUnwindTailCalls.C

#include "exitsFromNoUnwindTailCalls.h"
#include "complexMetricFocus.h"
#include "dataHeap.h"
#include "allSimpleMetricFocuses.h"

exitsFromNoUnwindTailCalls::exitsFromNoUnwindTailCalls(unsigned iid,
                                                       unsigned iclusterid,
                                                       const add1AtExitNoUnwindTailCalls &iadd1AtExitNoUnwindTailCalls) :
   ctr64ComplexMetric(iid, iclusterid,
                      "exits from (no unwinding tail calls)",
          "This metric measures the number of exits from a piece of code that you specify (a function or an individual basic block).\n\n\tIt works by inserting code to increment a counter by 1 at the exit of that function/basic block, with one exception (and here's where it differs from \"exitsFrom\"): if the exit spot being instrumented is an optimized tail call (tears down its stack frame before making the call, so that the callee will return two levels.  On a Sparc, this is usually achived through the asm sequence call; restore.), the counter will be incremented before the tail call is made.\n\n\tThis metric is mostly used internally to test out KernInst's code for the tricky case of instrumenting at such an optimized tail call.",
                      "#/sec", // current values have these units
                      "#", // total values have these units
                      complexMetric::unnormalized, // "events/sec" when shown in rthist, not "events"
                      complexMetric::sum, // aggregate (fold) by summing
                      iadd1AtExitNoUnwindTailCalls)
{
}
