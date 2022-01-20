#ifndef _ALL_SAMPLE_HANDLERS_H_
#define _ALL_SAMPLE_HANDLERS_H_

#include "sampleHandler.h"
#include "util/h/hashFns.h"

class allSampleHandlers {
 private:
    static unsigned sample_handler_id_hash(const sample_handler_id &id) {
	return addrHash(id.metricid) + addrHash(id.focusid);
    }
    dictionary_hash<sample_handler_id, sampleHandler*> 
	allSampleHandlersHash;
    // value: metricFocus, allocated with "new".  A pointer is best, so
    // when the dictionary resizes itself internally, references to a
    // metricFocus don't become dangling.
    
 public:
    allSampleHandlers() : allSampleHandlersHash(sample_handler_id_hash){}
    ~allSampleHandlers();

    typedef dictionary_hash<sample_handler_id, sampleHandler*>::const_iterator 
	const_iterator;
    const_iterator begin() {
	return allSampleHandlersHash.begin();
    }
    const_iterator end() {
	return allSampleHandlersHash.end();
    }

    sampleHandler& findOrCreateHandler(const complexMetric &aMetric, 
				       const focus &aFocus,
				       bool *found);

    void fry1(sampleHandler &theHandler) {
	assert(!theHandler.isReferenced());

	const sample_handler_id id = theHandler.getSampleHandlerId();
	delete allSampleHandlersHash.get_and_remove(id);
    }

    complexMetricFocus* findMetricFocus(unsigned metricid,
					unsigned focusid) const;
};

#endif
