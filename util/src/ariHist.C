#include "common/h/headers.h"
#include "util/h/ariHist.h"

int AriHistogram::numBins = 1000;
//int AriHistogram::lastGlobalBin = 0;
timeStamp AriHistogram::baseBucketSize = BASEBUCKETWIDTH;
//timeStamp AriHistogram::globalBucketSize = BASEBUCKETWIDTH;
//vector<AriHistogram *> AriHistogram::allHist;

AriHistogram::AriHistogram(float ibucketWidth,
                           metricStyle /* type */,
                           dataCallBack d, 
                           foldCallBack f, void *c, bool globalFlag)
:globalData(globalFlag)
{
    lastBin = 0;
    //metricType = type;

    buckets = new Bin[numBins];
    for(int i = 0; i < numBins; i++){
        buckets[i] = PARADYN_NaN; 
    }
    
    dataFunc = d;
    foldFunc = f;
    cData = c;
    active = true;
    fold_on_inactive = false;
    startTime = 0.0; // used to be a param --ari

    bucketWidth = ibucketWidth; // used to be calculated --ari
    
//    allHist += this;
    total_time = startTime + bucketWidth*numBins;
}

AriHistogram::~AriHistogram(){

//      // remove from allHist
//      unsigned i=0;
//      for(; i < allHist.size(); i++){
//          if(allHist[i] == this){
//  	    break;
//          }
//      }
//      for(unsigned j = i; j < (allHist.size() - 1); j++){
//          allHist[j] = allHist[j+1];
//      }
//      allHist.resize(allHist.size() - 1);

    delete [] buckets;
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
void AriHistogram::addInterval(timeStamp start, 
			    timeStamp end, 
			    sampleValue value) {
    while ((end >= total_time) || (start >= total_time)) {
	// colapse histogram.
	fold();
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

//      // TODO: this should be replaced with the above code when
//      // the performance consultant is changed to correctly
//      // delete histograms that it is no longer collection 
//      // data values for
//      // change this so that lastGlobalbin is max of all lastBins
//      if (startTime == 0.0) {
//  	if(lastBin > lastGlobalBin)
//              lastGlobalBin = lastBin;

//  	const unsigned all_hist_size = allHist.size();
//          for (unsigned i=0; i < all_hist_size; i++) {
//  	    AriHistogram *theHist = allHist[i];

//  	    if (theHist->startTime == 0.0 && theHist->isActive())
//  	        if (theHist->lastBin > lastGlobalBin)
//  	            lastGlobalBin = theHist->lastBin;
//  	}
//      }

    bucketValue(start, end, value);
}

void AriHistogram::fold() {
   // update global info.
//     if(startTime == 0.0){
//        //globalBucketSize *= 2.0;
//        //lastGlobalBin = numBins/2 - 1;
//     }
   timeStamp newBucketWidth = bucketWidth*2.0;

   // don't fold this histogram if it has already folded
   // This can happen to histograms that are created right
   // after a fold, so that the initial bucket width may 
   // not be correct and then the first data values will cause
   // another fold...in this case we only want to fold the
   // histograms that were not folded in the first round.  
   if(bucketWidth < newBucketWidth) {
      bucketWidth *= 2.0;

      Bin *bins = buckets;
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
      lastBin = last_bin; 
      for(int k=numBins/2; k<numBins; k++){
         bins[k] = PARADYN_NaN;
      }

      total_time = startTime + 
         numBins*bucketWidth;

      if(foldFunc) 
         (foldFunc)(bucketWidth, 
                    cData,
                    globalData);
   }
         
}

void AriHistogram::bucketValue(timeStamp start_clock, 
                               timeStamp end_clock, 
                               sampleValue value) {
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

   // normalize by bucket size.
   value /= bucketWidth;
   if (last_bin == first_bin) {
      if(isnan(buckets[first_bin])){ 
         buckets[first_bin] = 0.0;
      }
      buckets[first_bin] += value;

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
   if(isnan(buckets[first_bin])) 
      buckets[first_bin] = 0.0;
   if(isnan(buckets[last_bin])) 
      buckets[last_bin] = 0.0;

   /* add the appropriate amount of time to each bin */
   buckets[first_bin] += amt_first_bin;
   buckets[last_bin]  += amt_last_bin;
   for (i=first_bin+1; i < last_bin; i++){
      if(isnan(buckets[i])) 
         buckets[i] = 0.0;
      buckets[i]  += amt_other_bins;
   }

   // inform users about the data.
   // make sure they want to hear about it (dataFunc)
   //  && that we have a full bin (last_bin>first_bin)
   if (dataFunc && (last_bin-first_bin)) {
      (dataFunc)(&buckets[first_bin], 
                 startTime,
                 last_bin-first_bin, 
                 first_bin, 
                 cData,
                 globalData);
   }
}
