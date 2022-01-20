// blockOrderingChains.h
// Fairly close to "Algo2: Bottom-up Positioning" described in
// section 4.2.1 of Pettis & Hansen's PLDI'90 paper.

#ifndef _BLOCK_ORDERING_CHAINS_H_
#define _BLOCK_ORDERING_CHAINS_H_

#include "hotBlockCalcOracle.h"
#include "funkshun.h"
#include "sparcTraits.h"

typedef function_t::bbid_t bbid_t;

struct chain_vec_entry {
   unsigned chain_id;
   bbid_t next_bbid_this_chain; // (bbid_t)-1 if none
   bbid_t prev_bbid_this_chain; // (bbid_t)-1 if none

   chain_vec_entry(unsigned ichain_id, bbid_t inext_bbid_this_chain,
                   bbid_t iprev_bbid_this_chain) :
      chain_id(ichain_id),
      next_bbid_this_chain(inext_bbid_this_chain),
      prev_bbid_this_chain(iprev_bbid_this_chain) {
   }
};

struct edge_entry {
   bbid_t from;
   bbid_t to;
   unsigned weight; // calculated as best we can from block counts; fudged if needed

   edge_entry(bbid_t ifrom, bbid_t ito, unsigned iweight) :
      from(ifrom), to(ito), weight(iweight) {
   }
   bool operator==(const edge_entry &other) const {
      return from == other.from && to == other.to;
      // XXX should we compare weight, too???
   }
};

void pickBlockOrderingChains(const hotBlockCalcOracle &isBlockInlined,
                             const function_t &fn,
                             pdvector<function_t::bbid_t> &theInlinedBlockIds,
                             pdvector<function_t::bbid_t> &theOutlinedBlockIds);
   // Fills in "theInlinedBlockIds[]" and "theOutlinedBlockIds[]".

#endif


