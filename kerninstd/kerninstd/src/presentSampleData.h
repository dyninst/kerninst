// presentSampleData.h

#ifndef _PRESENT_SAMPLE_DATA_H_
#define _PRESENT_SAMPLE_DATA_H_

#include "clientConn.h"

extern "C" void presentSampleData1Value(kerninstdClient *,
                                        unsigned sampReqId,
                                        uint64_t sampCycles,
                                        sample_type sampValue);
extern "C" void presentSampleDataSeveralValues(kerninstdClient *,
                                               unsigned sampReqId,
                                               unsigned numSampleValues,
                                               sample_type *theSampleValues);

#endif
