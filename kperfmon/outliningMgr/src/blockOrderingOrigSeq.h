// blockOrderingOrigSeq.h

#ifndef _BLOCK_ORDERING_ORIG_SEQ_H_
#define _BLOCK_ORDERING_ORIG_SEQ_H_

#include "hotBlockCalcOracle.h"
#include "funkshun.h"
#include "sparcTraits.h"

void pickBlockOrderingOrigSeq(const hotBlockCalcOracle &isBlockInlined,
                              const function_t &fn,
                              pdvector<function_t::bbid_t> &theInlinedBlockIds,
                              pdvector<function_t::bbid_t> &theOutlinedBlockIds);
   // Fills in "theInlinedBlockIds[]" and "theOutlinedBlockIds[]".
   // The block ids will be in the same order that they
   // were present in the original code.

#endif
