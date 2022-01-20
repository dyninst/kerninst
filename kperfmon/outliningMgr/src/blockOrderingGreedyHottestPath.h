// blockOrderingGreedyHottestPath.h

#ifndef _BLOCK_ORDERING_GREEDY_HOTTEST_PATH_H_
#define _BLOCK_ORDERING_GREEDY_HOTTEST_PATH_H_

#include "hotBlockCalcOracle.h"
#include "funkshun.h"
#include "sparcTraits.h"

void 
pickBlockOrdering_greedyHottestPaths(const hotBlockCalcOracle &isBlockInlined,
                                     const function_t &fn,
                                     pdvector<function_t::bbid_t> &theInlinedBlocks,
                                     pdvector<function_t::bbid_t> &theOutlinedBlocks);

#endif
