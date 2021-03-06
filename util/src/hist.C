/*
 * Copyright (c) 1996 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * This license is for research uses.  For such uses, there is no
 * charge. We define "research use" to mean you may freely use it
 * inside your organization for whatever purposes you see fit. But you
 * may not re-distribute Paradyn or parts of Paradyn, in any form
 * source or binary (including derivatives), electronic or otherwise,
 * to any other organization or entity without our permission.
 * 
 * (for other uses, please contact us at paradyn@cs.wisc.edu)
 * 
 * All warranties, including without limitation, any warranty of
 * merchantability or fitness for a particular purpose, are hereby
 * excluded.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * Even if advised of the possibility of such damages, under no
 * circumstances shall we (or any other person or entity with
 * proprietary rights in the software licensed hereunder) be liable
 * to you or any third party for direct, indirect, or consequential
 * damages of any character regardless of type of action, including,
 * without limitation, loss of profits, loss of use, loss of good
 * will, or computer failure or malfunction.  You agree to indemnify
 * us (and any other person or entity with proprietary rights in the
 * software licensed hereunder) for any and all liability it may
 * incur to third parties resulting from your use of Paradyn.
 */

/*
 * hist.C - routines to manage hisograms.
 *
 * $Log: hist.C,v $
 * Revision 1.3  2003/01/07 18:49:41  mjbrim
 * replace use of vector with pdvector  - - - - - - - - - - - - - - - - - -
 * changed references to vector to pdvector
 *
 * Revision 1.2  2002/04/23 23:50:15  mjbrim
 * updates to share common/util classes between Kerninst & Paradyn,
 * which allows them to share igen, visilib, & binary visis  - - - - - - - -
 * use headers/classes from paradyn common/pdutil
 *
 * Revision 1.1.1.1  1999/07/02 22:55:11  tamches
 * initial commit
 *
 * Revision 1.31  1998/05/22 23:48:55  tamches
 * fixed a purify FMM (free mismatched memory) hit.
 *
 * Revision 1.30  1997/03/05 21:33:23  naim
 * Minor change to fix warning message about MAX macro - naim
 *
 * Revision 1.29  1997/02/26 23:49:53  mjrg
 * First part of WindowsNT commit: changes for compiling with VisualC++;
 * moved includes to platform header files
 *
 * Revision 1.28  1996/08/16 21:31:56  tamches
 * updated copyright for release 1.1
 *
 * Revision 1.27  1996/05/16 05:27:30  newhall
 * bug fix to foldAllHists, so that a check is done before folding to see if
 * the histogram has already been folded.  Changed how bucketWidth is computed
 * in constructor for histograms with a start time != 0.0, so that it will be
 * unlikely to create a histogram with a bucketWidth that is different from
 * other histograms with the same start time
 *
 * Revision 1.26  1996/05/15  18:20:49  newhall
 * bug fix to foldAllHist to correctly handle folding buckets where one or
 * both have NaN values
 *
 * Revision 1.25  1996/05/07  17:25:39  newhall
 * fix to bug caused by my previous commit
 *
 * Revision 1.24  1996/05/06  17:14:54  newhall
 * initialize top half of buckets to PARADYN_NaN on a fold
 *
 * Revision 1.23  1996/05/03  20:34:45  tamches
 * sped up addInterval()
 *
 * Revision 1.22  1996/02/12 19:54:19  karavan
 * bug fix: changed arguments to histFoldCallBack
 *
 */

#include "common/h/headers.h"
#include "util/h/hist.h"

/* number of intervals at which we switch to regular histograms */
#define MAX_INTERVALS	15

static void smoothBins(Bin *bins, int i, timeStamp bucketSize);

int Histogram::numBins = 1000;
int Histogram::lastGlobalBin = 0;
timeStamp Histogram::baseBucketSize = BASEBUCKETWIDTH;
timeStamp Histogram::globalBucketSize = BASEBUCKETWIDTH;
pdvector<Histogram *> Histogram::allHist;

Histogram::Histogram(metricStyle type, dataCallBack d, foldCallBack f, void *c,
		     bool globalFlag)
:globalData(globalFlag)
{
    smooth = false;
    lastBin = 0;
    metricType = type;
    intervalCount = 0;
    intervalLimit = MAX_INTERVALS;
    storageType = HistInterval;
    dataPtr.intervals = (Interval *) calloc(sizeof(Interval)*intervalLimit, 1);
    allHist += this;
    dataFunc = d;
    foldFunc = f;
    cData = c;
    active = true;
    fold_on_inactive = false;
    bucketWidth = globalBucketSize; 
    total_time = bucketWidth*numBins;
    startTime = 0.0;
}

/*
 * Given an array of buckets - turn them into a histogram.
 *
 */
Histogram::Histogram(Bin *buckets, 
		     metricStyle type, 
		     dataCallBack d, 
		     foldCallBack f,
		     void *c,
		     bool globalFlag)
{
    // First call default constructor.
    (void) Histogram(type, d, f, c, globalFlag);

    storageType = HistBucket;
    dataPtr.buckets = (Bin *) calloc(sizeof(Bin), numBins);
    memcpy(dataPtr.buckets, buckets, sizeof(Bin)*numBins);
}


// constructor for histogram that doesn't start at time 0
Histogram::Histogram(timeStamp start, metricStyle type, dataCallBack d, 
		     foldCallBack f, void *c, bool globalFlag)
:globalData(globalFlag)
{
    smooth = false;
    lastBin = 0;
    metricType = type;
    intervalCount = 0;
    intervalLimit = MAX_INTERVALS;
    storageType = HistInterval;
    dataPtr.intervals = (Interval *) calloc(sizeof(Interval)*intervalLimit, 1);
    dataFunc = d;
    foldFunc = f;
    cData = c;
    active = true;
    fold_on_inactive = false;
    startTime = start;

    // try to find an active histogram with the same start time 
    // and use its bucket width  for "bucketWidth", otherwise, compute
    // a value for "bucketWidth" based on startTime and global time
    bool found = false;
    for(unsigned i = 0; i < allHist.size(); i++){
	if(((allHist[i])->startTime == startTime)&&(allHist[i])->active){
            found = true;
	    bucketWidth = (allHist[i])->bucketWidth;
	    break;
    } }
    if(!found){
        // compute bucketwidth based on start time
        timeStamp minBucketWidth = 
	        ((lastGlobalBin*globalBucketSize)  - startTime) / numBins;    
        timeStamp i2 = baseBucketSize;
        for(; i2 < minBucketWidth; i2 *= 2.0) ; 
        bucketWidth = i2; 
    }

    allHist += this;
    total_time = startTime + bucketWidth*numBins;
}


Histogram::Histogram(Bin *buckets, 
                     timeStamp start,
		     metricStyle type, 
		     dataCallBack d, 
		     foldCallBack f,
		     void *c,
		     bool globalFlag)
{
    // First call default constructor.
    (void) Histogram(start, type, d, f, c, globalFlag);
    storageType = HistBucket;
    dataPtr.buckets = (Bin *) calloc(sizeof(Bin), numBins);
    memcpy(dataPtr.buckets, buckets, sizeof(Bin)*numBins);
}

Histogram::~Histogram(){

    // remove from allHist
    unsigned i=0;
    for(; i < allHist.size(); i++){
        if(allHist[i] == this){
	    break;
        }
    }
    for(unsigned j = i; j < (allHist.size() - 1); j++){
        allHist[j] = allHist[j+1];
    }
    allHist.resize(allHist.size() - 1);

    // free bin space
    if (storageType == HistInterval) {
        free(dataPtr.intervals);
    }
    else {
	delete [] dataPtr.buckets;
    }
}

#if !defined(MAX)
#define MAX(a, b)	((a) > (b) ? (a):(b))
#endif

/*
 * addInterval - add a value to a histogram from start to end.
 *
 * start, and end are relative to the global start time (i.e. 0.0 is the
 *   left side of the first hist bin).
 */
void Histogram::addInterval(timeStamp start, 
			    timeStamp end, 
			    sampleValue value, 
			    bool smooth)
{
    while ((end >= total_time) || (start >= total_time)) {
	// colapse histogram.
	foldAllHist();
    }

    lastBin = (int) ((end - startTime) / bucketWidth);

#ifdef n_def
    // update global info. if this histogram started at time 0
    if(startTime == 0.0){
        lastGlobalBin = lastBin;
        for (unsigned i=0; i < allHist.size(); i++) {
	    if((allHist[i])->startTime == 0.0){
	        if (((allHist[i])->lastBin < lastGlobalBin)) {
	            lastGlobalBin = (allHist[i])->lastBin;
	    }}
	}
    }
#endif

    // TODO: this should be replaced with the above code when
    // the performance consultant is changed to correctly
    // delete histograms that it is no longer collection 
    // data values for
    // change this so that lastGlobalbin is max of all lastBins
    if (startTime == 0.0) {
	if(lastBin > lastGlobalBin)
            lastGlobalBin = lastBin;

	const unsigned all_hist_size = allHist.size();
        for (unsigned i=0; i < all_hist_size; i++) {
	    Histogram *theHist = allHist[i]; // a time saver; call operator[] just once

	    if (theHist->startTime == 0.0 && theHist->isActive())
	        if (theHist->lastBin > lastGlobalBin)
	            lastGlobalBin = theHist->lastBin;
	}
    }

    if (storageType == HistInterval) {
	/* convert to buckets */
	convertToBins();
    }
    bucketValue(start, end, value, smooth);
}

void Histogram::convertToBins()
{
    int i;
    Interval *intervals;
    Interval *currentInterval;

    if (storageType == HistBucket) return;

    intervals = dataPtr.intervals;
    dataPtr.intervals = 0;
    dataPtr.buckets = new Bin[numBins];
    for(i = 0; i < numBins; i++){
        dataPtr.buckets[i] = PARADYN_NaN; 
    }

    for (i=0; i < intervalCount; i++) {
	currentInterval = &(intervals[i]);
	bucketValue(currentInterval->start, currentInterval->end, 
	    currentInterval->value, smooth);
    }
    free(intervals);
    intervalCount = -1;
    intervalLimit = 0;
    storageType = HistBucket;
}

void Histogram::foldAllHist()
{
    // update global info.
    if(startTime == 0.0){
	globalBucketSize *= 2.0;
        lastGlobalBin = numBins/2 - 1;
    }
    timeStamp newBucketWidth = bucketWidth*2.0;

    // fold all histograms with the same time base
    for(unsigned i = 0; i < allHist.size(); i++){
	if(((allHist[i])->startTime == startTime)  // has same base time and 
	   && ((allHist[i])->active                // either, histogram active
	   || (allHist[i])->fold_on_inactive)){    // or fold on inactive set 

          // don't fold this histogram if it has already folded
	  // This can happen to histograms that are created right
	  // after a fold, so that the initial bucket width may 
	  // not be correct and then the first data values will cause
	  // another fold...in this case we only want to fold the
	  // histograms that were not folded in the first round.  
	  if((allHist[i])->bucketWidth < newBucketWidth) {
	      (allHist[i])->bucketWidth *= 2.0;
	      if((allHist[i])->storageType == HistBucket){
                  Bin *bins = (allHist[i])->dataPtr.buckets;
		  int last_bin = -1;
		  int j=0;
		  for(; j < numBins/2; j++){
		      if(!isnan(bins[j*2+1])){   // both are not NaN
                          bins[j] = (bins[j*2] + bins[j*2+1]) / 2.0;
                      }
		      else if(!isnan(bins[j*2])){  // one is NaN
		          bins[j] = (bins[j*2])/2.0;	
			  if(last_bin == -1) last_bin = j;
		      }
		      else {  // both are NaN
			  bins[j] = PARADYN_NaN;
			  if(last_bin == -1) last_bin = j;
		      }
		  }
		  if(last_bin == -1) last_bin = j-1;
	          (allHist[i])->lastBin = last_bin; 
		  for(int k=numBins/2; k<numBins; k++){
		      bins[k] = PARADYN_NaN;
		  }
	      }
	      (allHist[i])->total_time = startTime + 
				       numBins*(allHist[i])->bucketWidth;
	      if((allHist[i])->foldFunc) 
	  	  ((allHist[i])->foldFunc)((allHist[i])->bucketWidth, 
					(allHist[i])->cData,
					(allHist[i])->globalData);
	  }
	}
    }
}

void Histogram::bucketValue(timeStamp start_clock, 
			   timeStamp end_clock, 
			   sampleValue value, 
			   bool smooth)
{
    register int i;

    // don't add values to an inactive histogram
    if(!active) return;

    timeStamp elapsed_clock = end_clock - start_clock;

    /* set starting and ending bins */
    int first_bin = (int) ((start_clock - startTime )/ bucketWidth);

    // ignore bad values
    if((first_bin < 0) || (first_bin > numBins)) return;
    if (first_bin == numBins)
	first_bin = numBins-1;
    int last_bin = (int) ((end_clock - startTime) / bucketWidth);

    // ignore bad values
    if((last_bin < 0) || (last_bin > numBins)) return;
    if (last_bin == numBins)
	last_bin = numBins-1;

    /* set starting times for first & last bins */
    timeStamp first_bin_start = bucketWidth * first_bin;
    timeStamp last_bin_start = bucketWidth  * last_bin;

    if (metricType == SampledFunction) {
	for (i=first_bin; i <= last_bin; i++) {
	    dataPtr.buckets[i] = value;
	}
    } else {
	// normalize by bucket size.
	value /= bucketWidth;
	if (last_bin == first_bin) {
	    if(isnan(dataPtr.buckets[first_bin])){ 
		dataPtr.buckets[first_bin] = 0.0;
            }
	    dataPtr.buckets[first_bin] += value;
	    if (smooth && (dataPtr.buckets[first_bin] > 1.0)) {
		/* value > 100% */
		smoothBins(dataPtr.buckets, first_bin, bucketWidth);
	    }
	    return;
	}

	/* determine how much of the first & last bins were in this interval */
	timeStamp time_in_first_bin = 
			bucketWidth - ((start_clock - startTime)- first_bin_start);
	timeStamp time_in_last_bin = (end_clock- startTime) - last_bin_start;
	timeStamp time_in_other_bins = 
	    MAX(elapsed_clock - (time_in_first_bin + time_in_last_bin), 0);
        // ignore bad values
        if((time_in_first_bin < 0) || 
	   (time_in_last_bin < 0) || 
	   (time_in_other_bins < 0)) 
	    return;

	/* determine how much of value should be in each bin in the interval */
	sampleValue amt_first_bin = (time_in_first_bin / elapsed_clock) * value;
	sampleValue amt_last_bin  = (time_in_last_bin  / elapsed_clock) * value;
	sampleValue amt_other_bins = 
				(time_in_other_bins / elapsed_clock) * value;
	if (last_bin > first_bin+1) 
	    amt_other_bins /= (last_bin - first_bin) - 1.0;

        // if bins contain NaN values set them to 0 before adding new value
	if(isnan(dataPtr.buckets[first_bin])) 
	    dataPtr.buckets[first_bin] = 0.0;
	if(isnan(dataPtr.buckets[last_bin])) 
	    dataPtr.buckets[last_bin] = 0.0;

	/* add the appropriate amount of time to each bin */
	dataPtr.buckets[first_bin] += amt_first_bin;
	dataPtr.buckets[last_bin]  += amt_last_bin;
	for (i=first_bin+1; i < last_bin; i++){
	    if(isnan(dataPtr.buckets[i])) 
	        dataPtr.buckets[i] = 0.0;
	    dataPtr.buckets[i]  += amt_other_bins;
        }
    }

    /* limit absurd time values - anything over 100% is pushed back into 
       previous buckets */
    if (smooth) {
	for (i = first_bin; i <= last_bin; i++) {
	    if (dataPtr.buckets[i] > bucketWidth) 
		smoothBins(dataPtr.buckets, i, bucketWidth);
	}
    }

    // inform users about the data.
    // make sure they want to hear about it (dataFunc)
    //  && that we have a full bin (last_bin>first_bin)
    if (dataFunc && (last_bin-first_bin)) {
	(dataFunc)(&dataPtr.buckets[first_bin], 
		   startTime,
		   last_bin-first_bin, 
		   first_bin, 
		   cData,
		   globalData);
    }
}


static void smoothBins(Bin *bins, int i, timeStamp bucketSize)
{
    int j;
    sampleValue extra_time;

    extra_time = (bins[i] - bucketSize);
    bins[i] = bucketSize;
    j = i - 1;
    while ((j >= 0) && (extra_time > 0.0)) {
	bins[j] += extra_time;
	extra_time = 0.0;
	if (bins[j] > bucketSize) {
	    extra_time = (bins[j] - bucketSize);
	    bins[j] = bucketSize;
	}
	j--;
    }
    if (extra_time > 0.0) {
	fprintf(stderr, "**** Unable to bucket %f seconds\n", extra_time);
	abort();
    }
}


static double splitTime(timeStamp startInterval, 
			timeStamp endInterval, 
			timeStamp startLimit, 
			timeStamp endLimit, 
			sampleValue value)
{
    sampleValue time;
    timeStamp inv_length;

    inv_length = endInterval-startInterval;
    if ((startInterval >= startLimit) && (endInterval <= endLimit)) { 	
	time = value;
    } else if (startInterval == endInterval) {	
	/* can't split delta functions */		
	time = 0.0;
    } else if ((startInterval >= startLimit) && (startInterval <= endLimit)) {
	/* overlaps the end */				
	time = value * (endLimit - startInterval) / inv_length;
    } else if ((endInterval <= endLimit) && (endInterval >= startLimit)) { 
	/* overlaps the end */				
	time = value * (endInterval - startLimit) / inv_length;    
    } else if ((startInterval < startLimit) && (endInterval > endLimit)) {
	/* detail contains interval */
	time = value * (endLimit - startLimit) / inv_length;
    } else {
	/* no overlap of time */
	time = 0.0;
    }
    return(time);
}

/*
 * Get the component of the histogram from start to end.
 *
 */
sampleValue Histogram::getValue(timeStamp start, timeStamp end)
{		
    int i;
    Interval *cp;
    sampleValue retVal = 0.0;
    int first_bin, last_bin;
    double pct_first_bin, pct_last_bin;

    if (storageType == HistInterval) {
	if (metricType == SampledFunction) {
	    // compute weight average of sample.
	    for (i=0; i < intervalCount; i++) {
		cp = &(dataPtr.intervals[i]);
		retVal += cp->value * (cp->end - cp->start);
	    }
	} else {
	    // total over the interval.
	    for (i=0; i < intervalCount; i++) {
		cp = &(dataPtr.intervals[i]);
		retVal += splitTime(cp->start, cp->end, start, end, cp->value);
	    }
	}
    } else {
	first_bin = (int) (start/bucketWidth);
	/* round up */
	last_bin = (int) (end/bucketWidth+0.5);
	if (last_bin >= numBins) last_bin = numBins-1;
	if (first_bin == last_bin) {
	    retVal = dataPtr.buckets[first_bin];
	} else {
	    /* (first_bin+1)*bucketWidth == time at end of first bucket */
	    pct_first_bin = (((first_bin+1)*bucketWidth)-start)/bucketWidth;
	    retVal += pct_first_bin * dataPtr.buckets[first_bin];
	    /* last_bin+*bucketWidth == time at start of last bucket */
	    pct_last_bin = (end - last_bin*bucketWidth)/bucketWidth;
	    retVal += pct_last_bin * dataPtr.buckets[last_bin];
	    for (i=first_bin+1; i <= last_bin-1; i++) {
		retVal += dataPtr.buckets[i];
	    }
	}
    }
    return(retVal * bucketWidth);
}

sampleValue Histogram::getValue()
{		
    return(getValue(0.0, numBins * bucketWidth));
}

int Histogram::getBuckets(sampleValue *buckets, int numberOfBuckets, int first)
{
    int i;
    int last;

    last = first + numberOfBuckets;
    if (lastBin < last) last = lastBin;

    // make sure its in bin form.
    convertToBins();

    assert(first >= 0);
    assert(last <= lastBin); 
    for (i=first; i < last; i++) {
	float temp = dataPtr.buckets[i];
	buckets[i-first] = temp;
    }
    return(last-first);
}
