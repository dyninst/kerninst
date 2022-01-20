#include "sampleHandler.h"

extern metricFocusManager *theGlobalMetricFocusManager;

// Given our theMetric and provided theFocus, find the corresponding
// CMF or create one, if nothing found
complexMetricFocus& 
sampleHandler::findOrCreateMetricFocus(const focus& theFocus) 
{
    pdvector<complexMetricFocus *>::iterator iter = theMetricFocuses.begin();
    pdvector<complexMetricFocus *>::iterator end = theMetricFocuses.end();

    for (; iter != end; iter++) {
	complexMetricFocus *existingCMF = (*iter);
	const focus& aFocus = existingCMF->getFocus();
	if (aFocus.getCPUId() == theFocus.getCPUId()) {
	    assert(aFocus == theFocus);
	    return (*existingCMF);
	}
    }
    complexMetricFocus *newCMF = new complexMetricFocus(false, theMetric, 
							theFocus, *this);
    assert(newCMF);
    theMetricFocuses += newCMF;

    return (*newCMF);
}

// Check if CMF specified by (metricid, focusid) is subscribed to us
complexMetricFocus* 
sampleHandler::findMetricFocus(unsigned metricid, unsigned focusid) 
{
    if (metricid != theMetric.getId()) {
	return 0;
    }
    complexMetricFocus* cmf = 0;
    pdvector<complexMetricFocus *>::iterator iter = theMetricFocuses.begin();
    pdvector<complexMetricFocus *>::iterator end = theMetricFocuses.end();

    for (; iter != end; iter++) {
	cmf = (*iter);
	if (cmf->getFocus().getId() == focusid) {
	    return cmf;
	}
    }
    return 0;
}

// Unsplice and stop sampling
void sampleHandler::uninstrument(bool goingDown)
{
    assert(theMetricFocuses.size() == 0);
    theGlobalMetricFocusManager->
	    unSpliceAndStopSamplingComplexMetricFocus(*this, goingDown);
}

// Unsubscribe cmf from this handler. Do not uninstrument the kernel
// even if it was the last metric
void sampleHandler::removeMetric(complexMetricFocus *cmf) 
{
    pdvector<complexMetricFocus *>::iterator wr = theMetricFocuses.begin();
    pdvector<complexMetricFocus *>::iterator end = theMetricFocuses.end();

    for (; wr != end && *wr != cmf; wr++);

    assert(wr != end);
    pdvector<complexMetricFocus *>::iterator rd = wr; 
    rd++;

    for (; rd != end; rd++, wr++) {
	*wr = *rd;
    }
    theMetricFocuses.pop_back();

    delete cmf;
}

// Unsubscribe all cmf's that feed only theSubscriber
void sampleHandler::frySubscribedMetrics(cmfSubscriber *theSubscriber)
{
    pdvector<complexMetricFocus *> allCMFs = theMetricFocuses;
    pdvector<complexMetricFocus *>::iterator imf = allCMFs.begin();
    pdvector<complexMetricFocus *>::iterator iend = allCMFs.end();

    for (; imf != iend; imf++) {
	complexMetricFocus *cmf = (*imf);
	if (cmf->unsubscribe_if(*theSubscriber)) {
	    removeMetric(cmf);
	}
    }
}

// When two handlers conflict on the use of %pic, we disable one of
// them (the old one)
void sampleHandler::disableMe() 
{
    assert(!disabled);
    disabled = true;

    pdvector<complexMetricFocus*>::iterator icmf = theMetricFocuses.begin();
    pdvector<complexMetricFocus*>::iterator endcmf = theMetricFocuses.end();
    for (; icmf != endcmf; icmf++) {
	complexMetricFocus *cmf = *icmf;
	cmf->disableMe();
    }
    theGlobalMetricFocusManager->
	unSpliceAndStopSamplingComplexMetricFocus(*this, false);
    // Gut the component smfs.  We assume that this cmf has already been
    // uninstrumented, and reference counts for smfs have been decremented
    // already.  Indeed, we shouldn't try to play with any component smf
    // structure, since those with ref counts of 0 will have been fried!
    componentSMFs.clear();

    // We'll need re-genesis on re-Enable
    genesisTime = 0;
    lastTimeOfSample = 0;
    presentInterval = 0;
    for (unsigned i=0; i<lastSampleValues.size(); i++) {
	lastSampleValues[i] = 0;
    }
}

// Is is ever called?
void sampleHandler::unDisableMe()
{
    assert(disabled);
    assert(genesisTime == 0);
    assert(lastTimeOfSample == 0); // 0 --> undefined (until re-genesis)

    disabled = false;

    pdvector<complexMetricFocus*>::iterator icmf = theMetricFocuses.begin();
    pdvector<complexMetricFocus*>::iterator endcmf = theMetricFocuses.end();
    for (; icmf != endcmf; icmf++) {
	complexMetricFocus *cmf = *icmf;
	cmf->unDisableMe();
    }
}

// Receive the first sample, initialize our data structures and
// start periodic sampling
void sampleHandler::doGenesisNow(uint64_t cmfGenesisTime,
				 const sample_t *initialRunningTotals,
				 unsigned numvalues) 
{
    assert(!hasGenesisOccurredYet());
    assert(cmfGenesisTime != 0 && "genesis time of 0 is not allowed");

    assert(genesisTime == 0);
    genesisTime = cmfGenesisTime;
    assert(hasGenesisOccurredYet());

    assert(lastTimeOfSample == 0); // 0 --> undefined
    lastTimeOfSample = cmfGenesisTime-1;
    // yes, -1; we don't want to give the impression that the cycle at
    // "cmfGenesisTime" has yet been assigned any sampled values, cuz it hasn't

    assert(lastSampleValues.size() == numvalues);
    for (unsigned i=0; i<numvalues; i++) {
	lastSampleValues[i] = initialRunningTotals[i];
    }
    assert(deltaSamples.size() == numvalues);

    assert(hasGenesisOccurredYet());

    // Hopefully, will start periodic sampling
    recalcPresentSampleIntervalFromSubscribers();
}


// All samples arrive here. We convert them to deltas and pass to
// the complexMetrics subscribed to us
void sampleHandler::handleRawSample(uint64_t timeOfSample, 
				    const sample_t *runningTotals,
				    unsigned numvalues)
{
    if (isDisabled()) {
	return;
    }

    if (!hasGenesisOccurredYet()) {
	// Time to send periodic sampling requests to kerninstd!  (Up
	// till now, we've only gotten a one-time "genesis" injection sample, 
	// and if we don't ask for more samples, no more will arrive.)
	doGenesisNow(timeOfSample, runningTotals, numvalues);

	assert(hasGenesisOccurredYet());
	return;
    }

    // compute deltaSamples[i] = runningTotals[i] - lastSampleTotals[i]
    // bucketer::add() wants a delta-total (since the last call to add())
    for (unsigned i = 0; i<numvalues; i++) {
	assert(runningTotals[i] >= lastSampleValues[i] && "value rollback?");

	deltaSamples[i] = runningTotals[i] - lastSampleValues[i];
	lastSampleValues[i] = runningTotals[i];
    }

    const uint64_t startTime = lastTimeOfSample + 1;
    const uint64_t endTimePlus1 = timeOfSample + 1;
    assert(endTimePlus1 > startTime);
    lastTimeOfSample = timeOfSample;

    // Pass the computed delta to subscribed CMFs
    pdvector<complexMetricFocus*>::iterator iter = theMetricFocuses.begin();
    pdvector<complexMetricFocus*>::iterator end = theMetricFocuses.end();
    for (; iter!= end; iter++) {
	(*iter)->handleDeltaSample(startTime, endTimePlus1, deltaSamples);
    }
}

// Ask all subscribers about their sampling interval, pick the 
// minimum value and adjust the sampling rate in kerninstd
void sampleHandler::recalcPresentSampleIntervalFromSubscribers() {
    if (!hasGenesisOccurredYet()) {
	return;
    }

    pdvector<complexMetricFocus *>::const_iterator iter = 
	theMetricFocuses.begin();
    pdvector<complexMetricFocus *>::const_iterator end = 
	theMetricFocuses.end();
	
    // Find the minimum non-zero interval (imin)
    unsigned imin = 0;
    for (; iter != end; iter++) {
	unsigned ival = (*iter)->getPresentSampleIntervalFromSubscribers();
	if (ival != 0 && (imin > ival || imin == 0)) {
	    imin = ival;
	}
    }
    if (imin != 0) {
	if (presentInterval == 0) {
	    // start sampling
	    theGlobalMetricFocusManager->reqPeriodicCMFSample(*this, imin);
	}
	else if (presentInterval != imin) {
	    // change rate
	    theGlobalMetricFocusManager->changePeriodicCMFSampleInterval(*this,
									 imin);
	}
    }
    else {
	if (presentInterval != 0) {
	    // stop sampling
	    theGlobalMetricFocusManager->removePeriodicCMFSampleInterval(
		*this);
	}
    }
    presentInterval = imin;
}
