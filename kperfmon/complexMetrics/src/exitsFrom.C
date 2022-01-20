// exitsFrom.C

#include "exitsFrom.h"
#include "complexMetricFocus.h"
#include "dataHeap.h"
#include "allSimpleMetricFocuses.h"

exitsFrom::exitsFrom(unsigned iid, unsigned iclusterid, const add1AtExit &iadd1AtExit) :
   ctr64ComplexMetric(iid, iclusterid,
                      "exits from",
          "This metric measures the number of exits from a piece of code that you specify (a function or an individual basic block).\n\n\tIt works by inserting code to increment a counter by 1 at the exit of that function/basic block.\n\n\tContrast with the \"entries to\" metric, which instruments the entry (instead of the exit) of that function/basic block.  Of course, both metrics should, over time, yield the same total, differing only transitionally as the function/basic block being instrumented is in the middle of execution",
                      "#/sec", // units when showing current values
                      "#", // units when showing total values
                      complexMetric::unnormalized, // "events/sec" when shown in rthist, not "events"
                      complexMetric::sum, // aggregate (fold) by summing
                      iadd1AtExit)
{
}
