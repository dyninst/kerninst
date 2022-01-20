// presentSampleData.C

#include "presentSampleData.h"
#include "instrumentationMgr.h"

extern instrumentationMgr *global_instrumentationMgr; // main.C
extern pdvector<client_connection*> clientProcs; // loop.C

static uint64_t firstSampCycles = 0;

void presentSampleData1Value(kerninstdClient *igenHandle, // for picking out the client
                             unsigned sampReqId,
                             uint64_t sampCycles,
                             sample_type sampValue) {
   // It has been decreed that there shall be a uniform concept of sample time that
   // all metricFocus pairs must agree upon.  This time is simply: rd %tick.
   // The point is: there's no excuse for rollback.  Assert that now
   if (firstSampCycles == 0)
      firstSampCycles = sampCycles;

   assert(sampCycles >= firstSampCycles && "presentSampleData1Value(): rollback");

   extern dictionary_hash<kerninstdClient*, client_connection*> igenHandle2ClientConn;
   client_connection *theClient = NULL;
   if (!igenHandle2ClientConn.find(igenHandle, theClient)) {
      cout << "presentSampleData: unknown client uniquifier; ignoring" << endl;
   }
   else {
//        cout << "About to ship sampleData: reqid="
//             << sampReqId << " value=" << sampValue
//             << " time=" << sampCycles << endl;

      global_instrumentationMgr->presentSampleData1Value(*theClient, sampReqId,
                                                         sampCycles, sampValue);
   }
}

void presentSampleDataSeveralValues(kerninstdClient *igenHandle,
                                       // for picking out the client
                                    unsigned sampReqId,
                                    unsigned numSampleValues,
                                    sample_type *theSampleValues)
{
   uint64_t sampCycles = igenHandle->getCurrentKerninstdTime();
   if (firstSampCycles == 0)
      firstSampCycles = sampCycles;

   assert(sampCycles >= firstSampCycles && "presentSampleData1Value(): rollback");

   extern dictionary_hash<kerninstdClient*, client_connection*> igenHandle2ClientConn;
   client_connection *theClient = NULL;
   if (!igenHandle2ClientConn.find(igenHandle, theClient)) {
      cout << "presentSampleData: unknown client uniquifier; ignoring" << endl;
   }
   else {
//        cout << "About to ship sampleData: reqid="
//             << sampReqId << " value=" << sampValue
//             << " time=" << sampCycles << endl;

      global_instrumentationMgr->
         presentSampleDataSeveralValues(*theClient, sampReqId, sampCycles,
                                        numSampleValues, theSampleValues);
   }
}

