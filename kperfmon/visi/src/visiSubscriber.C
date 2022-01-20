// visiSubscriber.C

#include "pdutil/h/pdsocket.h" // PDSOCKET
#include "visiSubscriber.h"
#include "complexMetricFocus.h"
#include "sampleHandler.h"
#include "util/h/out_streams.h"

void visi1CmfBucketer::
process1NumberedBucket(unsigned bucketNum, const pdvector<sample_t> &vals) {
   theVisi.processPerfectBucket(cmf, bucketNum, vals);
}

void visi1CmfBucketer::process1NumberedUndefinedBucket(unsigned bucketNum) {
   theVisi.processPerfectUndefinedBucket(cmf, bucketNum);
}

void visi1CmfBucketer::processFold(unsigned newMultipleOfBaseBucketWidth) {
   // foldingBucketer has determined that this curve has filled its last bucket to
   // the brim.  Anything more would overflow, so we must fold the bucketer.
   // Thus, we'd better fold the whole visi (every curve's bucketer), lest one
   // curve report values beyond the visi's end time.
   // Then, since the visi's going to fold, we must fold all m/f
   // curve bucketers being subscribed to by this visi.
   // Fortunately, bucketers are smart enough to accept a fold
   // at any time, right?  Right?
   theVisi.processFold(newMultipleOfBaseBucketWidth);
}

// ----------------------------------------------------------------------

class visualizationUserGentleError : public visualizationUser {
 public:
   visualizationUserGentleError(PDSOCKET use_sock,
                                int(*r)(void*,caddr_t,int),
                                int(*w)(void*,caddr_t,int),
                                const int nblock) :
      visualizationUser(use_sock, r, w, nblock) {
   }
   void handle_error() {}
};

// --------------------

static unsigned cmfPtrHash(complexMetricFocus * const &cmfPtr) {
   const complexMetricFocus &cmf = *cmfPtr;

   std::pair<unsigned,unsigned> ids = cmf.getMetricAndFocusIds();
   return ids.first + ids.second;
}

visiSubscriber::visiSubscriber(int igenfd, uint64_t kerninstdMachineTime,
                               unsigned ikerninstdMachineMhZ,
                               uint64_t ibucketSizeCycles) :
      cmfSubscriber(kerninstdMachineTime),
      theBucketers(cmfPtrHash),
      baseBucketSizeCycles(ibucketSizeCycles),
      bucketSizeMultiple(1), // no folds yet
      kerninstdMachineMhZ(ikerninstdMachineMhZ),
      kerninstdMachineMhZTimes1000(kerninstdMachineMhZ * 1000)
{
   assert(baseBucketSizeCycles > 0);
   
   igenHandle = new visualizationUserGentleError(igenfd, NULL, NULL, 0);
   assert(igenHandle);
   if (igenHandle->errorConditionFound) {
      cerr << "kperfmon: igen startup of visi failed." << endl;
      delete igenHandle;
      igenHandle = NULL;
   }
}

visiSubscriber::~visiSubscriber() {
   if (igenHandle) {
      (void)close(igenHandle->get_sock());
      
      delete igenHandle;
      igenHandle = NULL; // help purify find mem leaks
   }
}

void visiSubscriber::shipBufferedSampleDataToVisiIfAny() {
   if (reusableDataBuffer.size() == 0)
      return;
   if (igenHandle == NULL)
      return;
   igenHandle->Data(reusableDataBuffer);
   reusableDataBuffer.clear(); // fast; doesn't really deallocate memory
}

void visiSubscriber::newMetricFocusPair(complexMetricFocus &theMetricFocus,
                                        unsigned /*mhz*/) {
   const std::pair<unsigned, unsigned> metricAndFocusIds =
      theMetricFocus.getMetricAndFocusIds();
   const complexMetric &theMetric = theMetricFocus.getMetric();

   visi1CmfBucketer *theNewBucketer =
      new visi1CmfBucketer(*this, theMetricFocus,
                          theMetricFocus.getMetric().getNumberValuesToBucket(),
                           baseBucketSizeCycles * bucketSizeMultiple,
                           getGenesisTime(),
                           theMetric.getAggregateCode() == complexMetric::sum ?
                           visi1CmfBucketer::proportionalToTime:
                           visi1CmfBucketer::independentOfTime);
   theBucketers.set(&theMetricFocus, theNewBucketer);
   
   // Make igen call to tell the cmfSubscriber of this new metric/focus pair.
   visi_matrix elem0;

   const focus &theFocus = theMetricFocus.getFocus();
   
   elem0.res.name = theFocus.getName().c_str();
   elem0.res.Id = metricAndFocusIds.second;
   // visilib focus-id is the same as kerninst's focus-id

   // Metric information:
   elem0.met.curr_units = theMetric.getCurrentValUnitsName();
   elem0.met.tot_units = theMetric.getTotalValUnitsName();
   elem0.met.name = theMetric.getName();
   elem0.met.Id = metricAndFocusIds.first;
   // visilib metric-id same as kerninst's metric-id
   elem0.met.aggregate = theMetric.getAggregateCode();
   elem0.met.unitstype = theMetric.getNormalizedFlag();

   pdvector<visi_matrix> newElements;
   newElements += elem0;

   igenHandle->AddMetricsResources(newElements,
                                   0.2, // bucket width
                                   1000, // num buckets
                                   0, // seems strange to use this instead of metricFocus::get_current_time(), but using the latter seems to mess things up
                                   0 // phase handle (?)
                                   );
}

void visiSubscriber::subscriberHasRemovedCurve(complexMetricFocus &theMetricFocus) {
   delete theBucketers.get_and_remove(&theMetricFocus);
}

void visiSubscriber::disableMetricFocusPair(complexMetricFocus &theMetricFocus) {
   const std::pair<unsigned, unsigned> metricAndFocusIds =
      theMetricFocus.getMetricAndFocusIds();

   delete theBucketers.get_and_remove(&theMetricFocus);
   
   igenHandle->disableMR(metricAndFocusIds.first, metricAndFocusIds.second);
}

void visiSubscriber::
reEnableDisabledMetricFocusPair(complexMetricFocus &theMetricFocus,
                                unsigned mhz) {
   // strangely enough, the following happens to do what we need:
   // make igen call to the visi process: AddMetricsResources() [I'd much prefer
   //    something called UnInvalidMR() to complement InvalidMR() but there isn't,
   //    and besides, AddMetricsResources() seems to work (!)
   newMetricFocusPair(theMetricFocus, mhz);
      // yes, proper to call derived class one
}

void visiSubscriber::ssCloseDownShopHook() {
   // BEWARE: we're closing the visi connection yet samples can still come in.
   // Be prepared for this, assuming that there's no way around this (the visi
   // probably has already died)

   // Close down igen connection between kperfmon and the visi; of course the
   // igen connection between kperfmon and kerninstd remains open (as it must;
   // since we have only just begun the asynchronous handshaking process of
   // uninstrumenting this visi's m/f pairs and ceasing to sample it)

   assert(igenHandle);
   int igenfd = igenHandle->get_sock();
   //Tcl_DeleteFileHandler(igenfd);
   (void)close(igenfd);
   delete igenHandle;
   igenHandle = NULL; // helps purify find mem leaks
}

void visiSubscriber::add_(complexMetricFocus &cmf,
                             // can't be const since we want non-const dictionary key
                          uint64_t startTime, uint64_t endTimePlus1,
                          pdvector<sample_t> &valuesForThisInterval) {
   // metric-specific normalization hasn't yet taken place.
   // startTime,endTime don't cleanly correspond to any "perfect bucket", etc.,
   // so we'll need to bucket things up.
   // note: trashes the contents of (but not the size() of) "valuesForThisTimeInterval",
   // courtesy of util/h/bucketer's add()
   
   visi1CmfBucketer *theBucketer = theBucketers.get(&cmf);
   assert(theBucketer);

   // NOTE: the value(s) in "valuesForThisInterval[]" still correspond to rawly
   // sampled values; they have not been folded up into a single double ready
   // for visi consumption yet.  Only the visi1CmfBucketer does that.

   theBucketer->add(startTime, endTimePlus1, valuesForThisInterval);
      // if a perfect bucket is realized, callbacks will (eventually)
      // invoke our processPerfectBucket() method, below
}

void visiSubscriber::
processPerfectBucket(complexMetricFocus &cmf,
                        // can't be const since we want non-const dictionary key
                     unsigned bucketNum,
                     const pdvector<sample_t> &vals) {
   // Because the present version of visilib does not properly normalize
   // appropriate metric values (and it sure should, imho), we must do it here.

   const complexMetric &theMetric = cmf.getMetric();
   assert(vals.size() == theMetric.getNumberValuesToBucket());

   dataValue dValItem;
   dValItem.metricId = theMetric.getId();
   dValItem.resourceId = cmf.getFocus().getId();
   dValItem.bucketNum = bucketNum;
   dValItem.data = cmf.interpretAndAggregate(vals);

   // Now the normalization talked about above:
   if (theMetric.getNormalizedFlag() == complexMetric::unnormalized) {
      const visi1CmfBucketer *theBucketer = theBucketers.get(&cmf);
      const uint64_t bucketSizeCycles = theBucketer->getCurrBucketSize();

      extern unsigned cycles2millisecs(uint64_t, unsigned mhzTimes1000);
      const unsigned bucketSizeMillisecs =
         cycles2millisecs(bucketSizeCycles, kerninstdMachineMhZTimes1000);

      const double bucketSizeSeconds = (double)bucketSizeMillisecs / 1000.0;

      dValItem.data /= bucketSizeSeconds;
   }

   reusableDataBuffer += dValItem;
}
                                     
void visiSubscriber::
processPerfectUndefinedBucket(const complexMetricFocus &,
                              unsigned /* bucketNum */) {
   // Undefined buckets are "sent" by sending nothing at all.
   // Soon, when the visi sees a gap in buckets, it'll recognize the appropriate
   // undefined gap and handle it accordingly.  This seems to work (tested on rthist).

   // If this doesn't work, take 2 would be try send a NaN for this bucket and see
   // what happens.
}

void visiSubscriber::processFold(unsigned /*newMultiple*/) {
    unsigned maxMultiple = 0;
    dictionary_hash<complexMetricFocus*, visi1CmfBucketer*>::const_iterator
	ib = theBucketers.begin();
    dictionary_hash<complexMetricFocus*, visi1CmfBucketer*>::const_iterator
	ie = theBucketers.end();
    // Make sure that all bucketers are ready to fold
    // Determine the new multiple (max across all visis)
    for (; ib != ie; ib++) {
	visi1CmfBucketer *theBucketer = *ib;
	if (!theBucketer->isFoldPending()) {
	    // dout << "Bucketer " << addr2hex((dptr_t)theBucketer) 
	    //      << " is not ready to fold, continuing\n";
	    return;
	}
	if (theBucketer->getBucketSizeMultiple() > maxMultiple) {
	    maxMultiple = theBucketer->getBucketSizeMultiple();
	}
    }
    assert(maxMultiple > 0);

    // Should not happen in practice anymore
    if (bucketSizeMultiple >= maxMultiple) {
	dout << "visi ignoring processFold(" << maxMultiple 
	     << ") (already done that)\n";
	return;
    }
    else {
	dout << "Completing fold with multiple = " << maxMultiple << endl;
    }

    // After a fold, bucket numbers are adjusted.  Since each existing
    // buffered-up entries will have the *old* bucket numbers, we had better
    // let the type-specific bucketer use up those values now (as it sees
    // fit), before the igen call to Fold() causes the bucket nums stored
    // within reusableDataBuffer[] to become obsolete, causing big trouble!

    shipBufferedSampleDataToVisiIfAny();

    assert(maxMultiple > bucketSizeMultiple);
    bucketSizeMultiple = maxMultiple;
    
    igenHandle->Fold(bucketSizeMultiple * 0.2);
      // TO DO -- don't hardcode the 0.2

   // Fold all bucketers of this visi, even though one of them has probably
   // already folded (that's how this routine got invoked).  It'll be harmless to
   // (try to) fold that bucketer again.
   {
      dictionary_hash<complexMetricFocus*, visi1CmfBucketer*>::const_iterator
         iter = theBucketers.begin();
      dictionary_hash<complexMetricFocus*, visi1CmfBucketer*>::const_iterator
         finish = theBucketers.end();
      for (; iter != finish; ++iter) {
         visi1CmfBucketer *theBucketer = *iter;

	 // Notify the bucketers that the visi has folded
	 theBucketer->foldCompleted();
      }
   }

   // Now recalculate the sampling interval for each complexMetricFocus; they might
   // enlarge their sampling interval due to the fold (depending on the interaction
   // with other cmfSubscribers to that cmf, if any), which is always a nice thing
   // to do (reduces sampling overhead).
   {
      dictionary_hash<complexMetricFocus*, visi1CmfBucketer*>::const_iterator
         iter = theBucketers.begin();
      dictionary_hash<complexMetricFocus*, visi1CmfBucketer*>::const_iterator
         finish = theBucketers.end();
      for (; iter != finish; ++iter) {
         complexMetricFocus *cmf = iter.currkey();
	 sampleHandler& sh = cmf->getSampleHandler();

         sh.recalcPresentSampleIntervalFromSubscribers();
      }
   }
}
