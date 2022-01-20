#include "allSampleHandlers.h"

sampleHandler& 
allSampleHandlers::findOrCreateHandler(const complexMetric &aMetric, 
				       const focus &aFocus, bool *found) 
{
    focus fuzzy = focus::makeFocus(aFocus, FUZZY);

    const sample_handler_id id(aMetric.getId(), fuzzy.getId());
	
    sampleHandler *theHandler;

    if ((*found = allSampleHandlersHash.find(id, theHandler))) {
	// cout << "Reusing an existing sampleHandler\n";
    }
    else {
	// cout << "Creating a new sampleHandler\n";
	// Create new metric/focus pair
	theHandler = new sampleHandler(aMetric, fuzzy);
	allSampleHandlersHash.set(id, theHandler);
    } 
   
    return *theHandler;
}

complexMetricFocus*
allSampleHandlers::findMetricFocus(unsigned metricid,
				   unsigned focusid) const
{
    const_iterator ihand = allSampleHandlersHash.begin();
    const_iterator iend = allSampleHandlersHash.end();
    for (; ihand != iend; ihand++) {
	sampleHandler *shand = (*ihand);
	complexMetricFocus *cmf;

	if ((cmf = shand->findMetricFocus(metricid, focusid)) != 0) {
	    return cmf;
	}
    }
    return 0;
}

allSampleHandlers::~allSampleHandlers()
{
    const_iterator ihand = allSampleHandlersHash.begin();
    const_iterator iend = allSampleHandlersHash.end();
    for (; ihand != iend; ihand++) {
	sampleHandler *theHandler = (*ihand);
	delete theHandler;
    }

    allSampleHandlersHash.clear();
}
