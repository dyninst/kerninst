// entriesTo.C

#include "entriesTo.h"
#include "complexMetricFocus.h"
#include "dataHeap.h"
#include "allSimpleMetricFocuses.h"

entriesTo::entriesTo(unsigned iid, unsigned iclusterid,
                     const add1AtEntry &iAdd1AtEntry) :
   ctr64ComplexMetric(iid, iclusterid,
                      "entries to",
                      "This metric measures the number of entry executions of specified code (a function or an individual basic block).\n\n\tIt works by inserting code to increment a counter by 1 at the entry of that function/basic block.\n\n\tContrast with the \"exitsFrom\" metric, which instruments the exit (instead of the entry) of that function/basic block.  Of course, both metrics should, over time, yield the same total, differing only when function/basic block in question is being executed.",
                      "#/sec", // units when showing current values
                      "#", // units when showing total values
                      complexMetric::unnormalized, // "events/sec" when shown in rthist, not "events"
                      complexMetric::sum, // aggregate (fold) by summing
                      iAdd1AtEntry
                      )
{
}
