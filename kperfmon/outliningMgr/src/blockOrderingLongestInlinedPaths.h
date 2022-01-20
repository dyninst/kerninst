// blockOrderingLongestInlinedPaths.h

#ifndef _BLOCK_ORDERING_LONGEST_INLINED_PATHS_H_
#define _BLOCK_ORDERING_LONGEST_INLINED_PATHS_H_

#include "hotBlockCalcOracle.h"
#include "funkshun.h"
#include "sparcTraits.h"

void pickBlockOrdering_longestInlinedPaths(const hotBlockCalcOracle &isBlockInlined,
                                           const function_t &fn,
                                           pdvector<function_t::bbid_t> &theInlinedBlocks,
                                           pdvector<function_t::bbid_t> &theOutlinedBlocks);
   // Consider a digraph whose edges are weighted by execution frequency.
   // This algorithm runs Dijkstra's algorithm on this graph to find the
   // most heavily weighted path, among the hot blocks.  Such a path is emitted.
   // Then (arbitrarily?) the algorithm is re-run on the remaining graph until
   // every basic block has been emitted.

#endif
