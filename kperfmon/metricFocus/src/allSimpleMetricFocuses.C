// allSimpleMetricFocuses.C

#include "allSimpleMetricFocuses.h"

static unsigned smf_id_hash(const std::pair<unsigned,unsigned> &id) {
   // id.first: simpleMetric id
   // id.second: focus id
   return id.first + id.second;
}

allSimpleMetricFocuses::allSimpleMetricFocuses() :
   allSMFs(smf_id_hash) {
}

allSimpleMetricFocuses::~allSimpleMetricFocuses() {
   // a very strong assert; too strong?
   assert(allSMFs.size() == 0);
}

simpleMetricFocus *
allSimpleMetricFocuses::subscribeTo(const simpleMetric &smet,
                                    const focus &theFocus) {
   const std::pair<unsigned, unsigned> id(smet.getId(), theFocus.getId());
   
   simpleMetricFocus *result = NULL;
   if (!allSMFs.find(id, result)) {
      result = new simpleMetricFocus(smet, theFocus, 0, 0);
         // 0 for initial subscribe and splice reference counts
      allSMFs.set(id, result);
   }

   result->referenceSubscribed();

   return result;
}

bool allSimpleMetricFocuses::unsubscribeTo(const simpleMetricFocus &ismf) {
   const std::pair<unsigned, unsigned> id(ismf.getSimpleMetric().getId(),
                                     ismf.getFocus().getId());
   
   simpleMetricFocus *smf = allSMFs.get(id);
      // assert fails if not found

   return smf->dereferenceSubscribed();
      // doesn't fry the smf, even if ref count is now zero
}

void allSimpleMetricFocuses::
fryZeroReferencedSimpleMetricFocus(const simpleMetricFocus &ismf) {
   const std::pair<unsigned, unsigned> id(ismf.getSimpleMetric().getId(),
                                          ismf.getFocus().getId());
   
   simpleMetricFocus *smf = allSMFs.get_and_remove(id);
      // assert fails if not found

   assert(smf->getSubscribedRefCount() == 0);
   assert(smf->getSplicedRefCount() == 0);
   
   const simpleMetric &sm = smf->getSimpleMetric();

   delete smf;
   smf = NULL;

   // Free any hardware counters used by the simple metric
   if(sm.getRefCount() == 0) {
      kapi_hwcounter_kind hwkind = sm.getHwCtrKind();
      kapi_hwcounter_set current_set;
      current_set.readConfig();
      current_set.free(hwkind);
      current_set.writeConfig();
   }
      

   assert(!allSMFs.defines(id));
}
