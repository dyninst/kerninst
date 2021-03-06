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
 * 
 * $Log: aggregateSample.C,v $
 * Revision 1.2  2003/01/07 18:49:38  mjbrim
 * replace use of vector with pdvector  - - - - - - - - - - - - - - - - - -
 * changed references to vector to pdvector
 *
 * Revision 1.1.1.1  1999/07/02 22:55:11  tamches
 * initial commit
 *
 * Revision 1.16  1997/10/08 18:27:59  tamches
 * removed a print stmt
 *
 * Revision 1.15  1997/09/26 15:51:07  tamches
 * startTime() --> firstTimeAndValue().
 * newValue() now returns void
 * Provided an int version of newValue()
 *
 * Revision 1.14  1996/08/16 21:31:50  tamches
 * updated copyright for release 1.1
 *
 * Revision 1.13  1996/03/12 20:38:49  mjrg
 * New version of aggregateSample to support adding and removing components
 * dynamically.
 *
 * Revision 1.12  1996/02/09 22:15:28  mjrg
 * fixed aggregation to handle first samples and addition of new components
 *
 * Revision 1.11  1996/01/31 19:47:37  newhall
 * added a newValue method that takes a vector of weights for each part
 *
 * Revision 1.10  1995/09/08  19:44:56  krisna
 * stupid way to avoid the for-scope problem
 *
 * Revision 1.9  1995/06/02  21:00:07  newhall
 * added a NaN value generator
 * fixed memory leaks in Histogram class
 * added newValue member with a vector<sampleInfo *> to class sampleInfo
 *
 * Revision 1.8  1995/02/16  09:27:59  markc
 * Removed compiler warnings.
 * Changed Boolean to bool
 *
 */

#include <assert.h>
#include <math.h>

#include "util/h/aggregateSample.h"

// void sampleInfo::startTime(timeStamp time) {
//   assert(numAggregators > 0);
//   firstSampleReceived = true;
//   lastSampleStart = time;
//   // The remaining fields should be zero.
//   // It is important that the value of lastSampleEnd is zero, otherwise the
//   // aggregation code in newValue will not work.
// }

void sampleInfo::firstTimeAndValue(timeStamp time, int firstValue) {
  assert(numAggregators > 0);
  firstSampleReceived = true;
  lastSampleStart = time;

  assert(lastSample == 0);
  lastSample = (sampleValue)firstValue; // yuck; we lose something in int-->float conversion
  
  // The remaining fields should be zero.
  // It is important that the value of lastSampleEnd is zero, otherwise the
  // aggregation code in newValue will not work.
}

void sampleInfo::firstTimeAndValue(timeStamp time, float firstValue) {
  assert(numAggregators > 0);
  firstSampleReceived = true;
  lastSampleStart = time;

  assert(lastSample == 0);
  lastSample = firstValue;
  
  // The remaining fields should be zero.
  // It is important that the value of lastSampleEnd is zero, otherwise the
  // aggregation code in newValue will not work.
}

void sampleInfo::newValue(timeStamp sampleTime, 
                          int newVal,
                          unsigned weight_) {
    assert(firstSampleReceived);
    assert(sampleTime > lastSampleEnd);

    // used when it's a component of an aggregate.
    lastSample += newVal;
    lastSampleEnd = sampleTime;
    weight = weight_;
}

void sampleInfo::newValue(timeStamp sampleTime, 
                          sampleValue newVal, 
                          unsigned weight_) {
    // why does this routine return a value (which is essentially useless)?

    assert(firstSampleReceived);
    assert(sampleTime > lastSampleEnd);

    // used when it's a component of an aggregate.
    lastSample += newVal;
    lastSampleEnd = sampleTime;
    weight = weight_;
}


struct sampleInterval aggregateSample::aggregateValues() {
    struct sampleInterval ret;
    timeStamp earlyestTime = HUGE_VAL;
    ret.valid = false;

    if (newParts.size()) {
      // The new components do not need to have the same start time.
      // We must wait until we have samples from all components, and then
      // we need to find the first and second min start time for the new components
      // and if first min == lastSampleEnd, we can aggregate from first to second.
      // But we don't want to generate lots of very small samples in the case we have
      // a large number of components, so we round the start times that are close
      // enough to the min start time.

      timeStamp newStart = HUGE_VAL;
      timeStamp secondStart = HUGE_VAL;
      for (unsigned u = 0; u < newParts.size(); u++) {
	if ((!newParts[u]->firstValueReceived())) {
	  return ret;
	}
	if (newParts[u]->lastSampleStart < lastSampleEnd + 0.01) {
	  // round lastSampleStart to avoid generating very small aggregate
	  // samples. I'm using 0.01 as an arbitrary value. We should
	  // do some measurements to find a good value -- mjrg
	  newParts[u]->lastSampleStart = lastSampleEnd;
	}
	if (newParts[u]->lastSampleStart < newStart) {
	  newStart = newParts[u]->lastSampleStart;
	}
      }

      assert (newStart >= lastSampleEnd);

      if (parts.size() == 0)
	lastSampleEnd = newStart;

      if (newStart > lastSampleEnd) {
	// new parts can't be aggregated yet
	earlyestTime = newStart;
      }
      else { // newStart == lastSampleEnd
	// find the new parts that are ready to be aggregated and move them to parts.
	pdvector<sampleInfo *> temp;
	pdvector<bool> rtemp;
	for (unsigned u = 0; u < newParts.size(); u++) {
	  if (newParts[u]->lastSampleStart == newStart) {
	    parts += newParts[u];
	    removedParts += removedNewParts[u];
	  }
          else {
	    if (newParts[u]->lastSampleStart < secondStart)
	      secondStart = newParts[u]->lastSampleStart;
	    temp += newParts[u];
	    rtemp += removedNewParts[u];
	  }
	}
	if (temp.size()) {
	  newParts = temp;
	  removedNewParts = rtemp;
	  earlyestTime = secondStart;
	}
	else {
	  newParts.resize(0);
	  removedNewParts.resize(0);
	}
      }
    }

    bool partsToRemove = false;

    for (unsigned u = 0; u < parts.size(); u++) {
      if (removedParts[u]) {
	if (parts[u]->lastSampleEnd == parts[u]->lastSampleStart) {
	  partsToRemove = true;
	  continue;
	}
	if (parts[u]->lastSampleEnd < lastSampleEnd + 0.01) {
	   // round to avoid very small intervals;
	   parts[u]->lastSampleEnd = lastSampleEnd + 0.01;
	 }
      }
      if (parts[u]->lastSampleEnd < earlyestTime)
	earlyestTime = parts[u]->lastSampleEnd;
    }

    if (partsToRemove) {
      pdvector<sampleInfo *> temp;
      pdvector<bool> rtemp;
      for (unsigned u = 0; u < parts.size(); u++) {
	if (!(removedParts[u] && parts[u]->lastSampleEnd == parts[u]->lastSampleStart)) {
	  temp += parts[u];
	  rtemp += removedParts[u];
	}
	else {
	  --(parts[u]->numAggregators);
	  if (parts[u]->numAggregators == 0) {
	    delete parts[u];
	  }
	}
      }
      parts = temp;
      removedParts = rtemp;
      if (parts.size() == 0)
	return ret;
    }

    sampleValue aggregateVal;
    unsigned total_weight;

    if (earlyestTime > lastSampleEnd + 0.001) {
    // we can aggregate up to earlyest time.

            aggregateVal = 0.0;
            timeStamp fract;
            timeStamp component;

	    int first = 1;
	    total_weight = 0;

	    for (unsigned u = 0; u < parts.size(); u++) {
		// assert(earlyestTime >= parts[u]->lastSampleStart);

		fract = (earlyestTime - lastSampleEnd)/
		    (parts[u]->lastSampleEnd - parts[u]->lastSampleStart);
		component = (parts[u]->lastSample) * fract;

		assert(fract > 0.0);
		assert(fract <= 1.0);
		assert(component >= -0.01);

		parts[u]->lastSample -= component;

		// each list entry comes from a separate reporter
		switch (aggOp)
		  {
		  case aggSum:
		    aggregateVal += component;
		    break;
		  case aggAvg:
		    aggregateVal += parts[u]->weight*component;
		    total_weight += parts[u]->weight;
		    break;
		  case aggMin:
		    if (first) {
		      aggregateVal = component;
		      first = 0;
		    } else if (component < aggregateVal)
		      aggregateVal = component;
		    break;
		  case aggMax:
		    if (component > aggregateVal)
		      aggregateVal = component;
		    break;
		  }

		/* move forward our time of our earliest sample */
		parts[u]->lastSampleStart = earlyestTime;

	      }

	    if (aggOp == aggAvg)
	      aggregateVal /= total_weight;

	    ret.valid = true;
	    ret.start = lastSampleEnd;
	    ret.end = earlyestTime;
	    ret.value = aggregateVal;
	    assert(ret.value >= 0.0);

	    lastSampleStart = lastSampleEnd;
	    lastSampleEnd = earlyestTime;

	  }

    return ret;
}
