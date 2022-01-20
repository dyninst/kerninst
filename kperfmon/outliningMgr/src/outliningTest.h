// outliningTest.h

#ifndef _OUTLINING_TEST_H_
#define _OUTLINING_TEST_H_

#include "util/h/Dictionary.h"
#include "funkshun.h"
#include "sparcTraits.h"
#include "fnCodeObjects.h"

typedef function_t::bbid_t bbid_t;

// Here's a function that will help you figure out the "blocksToInline",
// "blocksToOutline", emitPreFnDataStartAddr, emitInlinedBlocksStartAddr,
// and emitOutlinedBlocksStartAddr params that you'll want to pass to
// testOutlineCoupledNoChanges():
void
getInlinedAndOutlinedInfoAboutFn(const function_t &,
                                 const fnCodeObjects &,
                                    // used to verify exact pre fn data num bytes
                                 pdvector<bbid_t> &blocksPresentlyInlined,
                                    // filled in.  Keeps present ordering, too.
                                 pdvector<bbid_t> &blocksPresentlyOutlined,
                                    // filled in.  Keeps present ordering, too.
                                 kptr_t &emitPreFnDataStartAddr, // filled in
                                 kptr_t &emitInlinedBlocksStartAddr, // filled in
                                 kptr_t &emitOutlinedBlocksStartAddr // filled in
                                 );

void
testOutlineCoupledNoChanges(unsigned groupUniqueId,
                            const pdstring &modName,
                            const function_t &,
                            const fnCodeObjects &,
                            const pdvector<bbid_t> &blocksToInline,
                            const pdvector<bbid_t> &blocksToOutline,
                            kptr_t emitPreFnDataStartAddr,
                            kptr_t emitInlinedBlocksStartAddr,
                            kptr_t emitOutlinedBlocksStartAddr,
                            const dictionary_hash<pdstring, kptr_t> &
                                  knownDownloadedCodeAddrsDict
                               );

int testOutliningOneFnInIsolation(
    const pdstring &modName, const function_t &fn,
    const dictionary_hash<pdstring,kptr_t> &knownDownloadedCodeAddrsDict);

#endif
