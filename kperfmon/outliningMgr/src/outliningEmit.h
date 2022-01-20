// outliningEmit.h

#ifndef _OUTLINING_EMIT_H_
#define _OUTLINING_EMIT_H_

#include "fnCodeObjects.h"
#include "outliningLocations.h"
#include "sparcTraits.h"
#include "hotBlockCalcOracle.h"
//class fnBlockAndEdgeCounts;

typedef function_t::bbid_t bbid_t;

void
emitAGroupOfBlocksInNewChunk(bool tryToKeepOriginalCode,
                             tempBufferEmitter &em,
                             outliningLocations &whereBlocksNowReside,
                                // not const on purpose
                             const function_t &fn,
                             const fnCodeObjects &theFnCodeObjects,
                             const pdvector<bbid_t> &blockIds);

typedef void (*pickBlockOrderingFnType)(const hotBlockCalcOracle &,
                                        const function_t &,
                                        pdvector<bbid_t> &theInlinedBlockIds,
                                        pdvector<bbid_t> &theOutlinedBlockIds);

void
pickBlockOrderingAndEmit1Fn(pickBlockOrderingFnType pickBlockOrdering,
                            unsigned groupUniqueId,
                            tempBufferEmitter &em,
                            const pdvector< pair<pdstring, const function_t *> > &allFuncsInGroup,
                            const fnBlockAndEdgeCounts &rootFnBlockAndEdgeCounts,
                            const function_t &fn,
                            const fnCodeObjects &theFnCodeObjects,
                            const pdvector<unsigned> &bbCounts,
                            hotBlockCalcOracle::ThresholdChoices);

#endif
