// blockOrderingChains.C
// Fairly close to "Algo2: Bottom-up Positioning" described in
// section 4.2.1 of Pettis & Hansen's PLDI'90 paper.

#include "blockOrderingChains.h"
#include <algorithm> // STL's sort()
#include "util/h/Dictionary.h"
#include "util/h/hashFns.h"

static bool hottest_edges_first(const edge_entry &e1, const edge_entry &e2) {
   return e1.weight > e2.weight;
}

static bool is_Sorted(pdvector<edge_entry>::const_iterator begin,
                      pdvector<edge_entry>::const_iterator end) {
  if(begin == end) return true;
  pdvector<edge_entry>::const_iterator temp = begin;
  for(++temp; temp != end; begin = temp, temp++) {
    if(hottest_edges_first(*temp, *begin)) return false;
  }
  return true;
}

static void add_to(pdvector<bbid_t> &theBlockIds,
                   const pdvector<chain_vec_entry> &the_chains_vec,
                   const bbid_t first_bbid,
                   const unsigned chain_id,
                   const hotBlockCalcOracle &theBlockOracle,
                   bool addingToHotBlocks) {
   bbid_t bbid = first_bbid;
   
   while (bbid != (bbid_t)-1) {
      // Every block in this chain should match the flag "addingToHotBlocks".
      assert(theBlockOracle.isBlockInlined(bbid) == addingToHotBlocks);
      
      theBlockIds += bbid;

      assert(the_chains_vec[bbid].chain_id == chain_id);
      bbid = the_chains_vec[bbid].next_bbid_this_chain;
   }
}

static void merge_chains(unsigned numBBs,
                         pdvector<chain_vec_entry> &the_chains_vec,
                         dictionary_hash<unsigned, bbid_t> &chainId2HeadOfChain,
                         pdvector<edge_entry> &interChainEdges, // will be sorted
                         const pdvector<edge_entry> &the_edges_sorted,
                         const hotBlockCalcOracle &theBlockOracle) {
   // Merges chains, where possible, and also fills in "interChainEdges", to denote
   // where it wasn't possible to combine chains, even though we would have liked
   // to.  Edges between inlined & outlined (hot & cold) blocks are, as always,
   // utterly ignored, so don't expect to find them in interChainEdges[] or
   // anywhere else for that matter.

   for (bbid_t bbid = 0; bbid < numBBs; ++bbid) {
      const unsigned chainid = the_chains_vec[bbid].chain_id;
      assert(chainid == bbid);
      chainId2HeadOfChain.set(chainid, bbid);
   }

   pdvector<unsigned> chainIdsThatNoLongerExist; // initially empty

   pdvector<edge_entry>::const_iterator edge_iter = the_edges_sorted.begin();
   pdvector<edge_entry>::const_iterator edge_finish = the_edges_sorted.end();
   for (; edge_iter != edge_finish; ++edge_iter) {
      const edge_entry &e = *edge_iter;
      
      chain_vec_entry &from_block_chain_entry = the_chains_vec[e.from];
      chain_vec_entry &to_block_chain_entry = the_chains_vec[e.to];

      if (from_block_chain_entry.chain_id != to_block_chain_entry.chain_id &&
          from_block_chain_entry.next_bbid_this_chain == (bbid_t)-1 &&
          to_block_chain_entry.prev_bbid_this_chain == (bbid_t)-1 &&
          theBlockOracle.isBlockInlined(e.from) == theBlockOracle.isBlockInlined(e.to)
             // check for same inlined vs. outlined (hot vs. cold)
             // given that our edge weight calculations are presently rather
             // bogus (the destination's edge weight, regardless of the
             // source's weight), this is a very nice sanity check that helps
             // make it a little less bogus
         )
      {
         // Merge chains.  The "to" chain is obliterated, swallowed up by the
         // superior might of the "from" chain.
         assert(from_block_chain_entry.next_bbid_this_chain == (bbid_t)-1);
         from_block_chain_entry.next_bbid_this_chain = e.to;
         
         assert(to_block_chain_entry.prev_bbid_this_chain == (bbid_t)-1);
         to_block_chain_entry.prev_bbid_this_chain = e.from;

         const unsigned obsolete_chain_id = to_block_chain_entry.chain_id;
         const unsigned replace_with_this_chain_id = from_block_chain_entry.chain_id;

//           cout << "Chainid " << replace_with_this_chain_id
//                << " swallowing up chainid " << obsolete_chain_id << endl;

         assert(obsolete_chain_id != replace_with_this_chain_id);

         // Change everyone else in the former "to" chain from
         // obsolete_chain_id to from_block_chain_entry.chain_id
         for (bbid_t bbid_to_move = chainId2HeadOfChain.get(obsolete_chain_id);
              bbid_to_move != (bbid_t)-1; ) {
            chain_vec_entry &cve = the_chains_vec[bbid_to_move];
            assert(cve.chain_id == obsolete_chain_id);
            cve.chain_id = replace_with_this_chain_id;
            
            bbid_to_move = cve.next_bbid_this_chain;
         }

         // Note that we don't update interChainEdges; it contains basic block
         // ids in its from & to fields, which may not be at the start of 
         // a chain.
         chainIdsThatNoLongerExist += obsolete_chain_id;
         chainId2HeadOfChain.get_and_remove(obsolete_chain_id);
      }
      else if (from_block_chain_entry.chain_id != to_block_chain_entry.chain_id &&
               theBlockOracle.isBlockInlined(e.from) ==
               theBlockOracle.isBlockInlined(e.to)) {
         // Inter-chain edge, within the same "section" (hot vs. cold).  Used later on.
         // (again: inter-chain edges connect two hot or two cold chains, never a mix)

//           cout << "Inter-chain edge [reminder: bbids] (" << e.from << ", "
//                << e.to << ")" << endl;
         
         interChainEdges += e;
      }
   }

   // Rest of the function: some assertion checking
#ifndef NDEBUG
   pdvector<unsigned> chainIdsThatDoExist; // initially empty

   pdvector<chain_vec_entry>::const_iterator chain_iter = the_chains_vec.begin();
   pdvector<chain_vec_entry>::const_iterator chain_finish = the_chains_vec.end();
   for (; chain_iter != chain_finish; ++chain_iter) {
      const unsigned chain_id = chain_iter->chain_id;
      chainIdsThatDoExist += chain_id;
      assert(chainId2HeadOfChain.defines(chain_id));
   }

   // Remove duplicates from "chainIdsThatDoExist[]":
   std::sort(chainIdsThatDoExist.begin(), chainIdsThatDoExist.end());
   pdvector<unsigned>::const_iterator new_last = 
      std::unique(chainIdsThatDoExist.begin(), chainIdsThatDoExist.end());
   chainIdsThatDoExist.resize(new_last - chainIdsThatDoExist.begin());
      // will either shrink the vector or will not change the size,
      // but certainly shouldn't grow the vector

   assert(chainId2HeadOfChain.size() == chainIdsThatDoExist.size());
   
   // OK, now we've got "chainIdsThatDoExist[]" and "chainIdsThatNoLongerExist[]".
   // Combine them, sort, and we should have a consecutive run from 0 to max_bbid-1.
   // Nice assertion check, eh?
   
   pdvector<unsigned> check_vec = chainIdsThatDoExist;
   check_vec += chainIdsThatNoLongerExist;
   std::sort(check_vec.begin(), check_vec.end());
   assert(check_vec.size() == numBBs);
   for (unsigned ndx = 0; ndx < check_vec.size(); ++ndx)
      assert(check_vec[ndx] == ndx);
#endif

   // Fix up inter-chain edges: what was an inter-chain edge at the time it was
   // created may no longer be inter-chain, due to *later* chain mergings.
   pdvector<edge_entry>::iterator interChainEdgeIter = interChainEdges.begin();
   while (interChainEdgeIter != interChainEdges.end()) { // must re-read each time!
      const edge_entry &e = *interChainEdgeIter;
      if (the_chains_vec[e.from].chain_id == the_chains_vec[e.to].chain_id) {
         // Fry this inter-chain edge
         *interChainEdgeIter = interChainEdges.back();
         // do NOT bump up interChainEdgeIter
         interChainEdges.pop_back();
      }
      else
         ++interChainEdgeIter;
   }
   std::sort(interChainEdges.begin(), interChainEdges.end(), hottest_edges_first);
   assert(interChainEdges.end() == std::adjacent_find(interChainEdges.begin(),
                                                      interChainEdges.end()));
}

static unsigned
find_next_chain(bool workingOnInlinedChains, // needed?
                const dictionary_hash<unsigned, bool> &placedChains,
                const pdvector<chain_vec_entry> &theChainsVec,
                const pdvector<edge_entry> &interChainEdges,
                const hotBlockCalcOracle &theBlockOracle) {
   // Pick the next "placeme_chain_id": find the highest-weighted edge leading
   // from an already-placed chain to one that has not already been placed.
   // (That simplifies to finding the highest-weighted edge from an
   // already-placed chain.)  Return the to-edge of that chain.
   //
   // If no such chain can be found given the interChainEdges, yet
   // interChainEdges[] is not empty, then we have an unusual situation: two
   // chains, both of which cannot be placed, are connected via an inter-proc edge,
   // but it does us no good because since neither one of those chains has been
   // placed, the inter-chain edge wasn't considered.  Probably the cause of the
   // disconnected graph is hot vs. cold blocks (an edge that would connect these
   // chains to the rest of the graph was long since blown to bits, since it was
   // from a hot to a cold block, probably).
   // So in this not-so-rare situation, we pick the highest weight inter-chain
   // edge, and choose the "from" chain of that edge.

   // return the to-chain of that edge, or UINT_MAX if no more chains can be
   // placed, presumably because there are disjoint chains (prob w/ execution
   // count zero).

   assert(is_Sorted(interChainEdges.begin(), interChainEdges.end()));
   assert(interChainEdges.end() ==
          std::adjacent_find(interChainEdges.begin(), interChainEdges.end()));
   
   pdvector<edge_entry>::const_iterator edge_iter = interChainEdges.begin();
   pdvector<edge_entry>::const_iterator edge_finish = interChainEdges.end();
   for (; edge_iter != edge_finish; ++edge_iter) {
      const unsigned from_chain = theChainsVec[edge_iter->from].chain_id;
      const unsigned to_chain = theChainsVec[edge_iter->to].chain_id;
      assert(from_chain != to_chain); // not an inter-chain edge?!

      // interChainEdges should already have removed all edges leading to chains
      // that have already been placed:
      assert(placedChains.get(to_chain) == false);

      assert(theBlockOracle.isBlockInlined(edge_iter->from) == workingOnInlinedChains);
      assert(theBlockOracle.isBlockInlined(edge_iter->to) == workingOnInlinedChains);

      if (placedChains.get(from_chain) == true)
         return to_chain;
   }

   // This is the unusual, but not-so-rare, situation of: we still have >= 1
   // inter-chain edges, but can't use any of 'em, since the "from" chains were not
   // placed yet.  A disconnected chain graph.  Probably the best thing that we can
   // do here is pick the "from" chain of the highest remaining inter-chain edge
   // to place next.
   return theChainsVec[interChainEdges[0].from].chain_id;
}

static void place_chain(unsigned placeme_chain_id,
                        bool workingOnInlinedChains,
                        const pdvector<unsigned> &allChainsToPlace,
                        pdvector<unsigned> &chainsOrdering, // for inlined or outlined
                        dictionary_hash<unsigned, bool> &placedChains,
                        const pdvector<chain_vec_entry> &theChainsVec,
                        const pdvector<edge_entry> &iInterChainEdges,
                        const hotBlockCalcOracle &theBlockOracle) {
   // NOTE: "iInterChainEdges" must be sorted
   assert(is_Sorted(iInterChainEdges.begin(), iInterChainEdges.end()));
   assert(iInterChainEdges.end() == std::adjacent_find(iInterChainEdges.begin(),
                                                       iInterChainEdges.end()));

   // First, trim from interChainEdges all those edges not wholly contained
   // within either hot blocks or within cold blocks, as specified
   // by "workingOnInlinedChains".  ACTUALLY, I don't think that inter-chain edges
   // will ever mix hot/cold blocks to begin with, so probably unnecessary.
   pdvector<edge_entry> interChainEdges;
   pdvector<edge_entry>::const_iterator cedge_iter = iInterChainEdges.begin();
   pdvector<edge_entry>::const_iterator cedge_finish = iInterChainEdges.end();
   for (; cedge_iter != cedge_finish; ++cedge_iter) {
      const edge_entry &e = *cedge_iter;
      
      if (theBlockOracle.isBlockInlined(e.from) == workingOnInlinedChains &&
          theBlockOracle.isBlockInlined(e.to) == workingOnInlinedChains)
         interChainEdges += e;
   }

   // Now, place edges until interChainEdges are empty
   // Start with placeme_chain_id
   while (true) {
      chainsOrdering += placeme_chain_id;
      assert(placedChains.get(placeme_chain_id) == false);
      placedChains.get(placeme_chain_id) = true;

      //cout << "placed chainid " << placeme_chain_id << endl;
      
      // Remove all edges from "interChainEdges" that lead "to" this chain, for it
      // it too late to take such an edge into consideration.  But don't remove
      // an edge containing "from" this chain.
      pdvector<edge_entry> remainingInterChainEdges;
      pdvector<edge_entry>::const_iterator edge_iter = interChainEdges.begin();
      pdvector<edge_entry>::const_iterator edge_finish = interChainEdges.end();
      for (; edge_iter != edge_finish; ++edge_iter) {
         if (theChainsVec[edge_iter->to].chain_id == placeme_chain_id) {
//              cout << "   frying inter-chain edge ("
//                   << edge_iter->from << ", "
//                   << edge_iter->to << ") due to placement of chainid "
//                   << placeme_chain_id << endl;
         }
         else
            remainingInterChainEdges += *edge_iter;
      }

      interChainEdges.swap(remainingInterChainEdges); // much faster than assignment

      // NOTE: "interChainEdges" must be sorted
      assert(is_Sorted(interChainEdges.begin(), interChainEdges.end()));
      
      if (interChainEdges.size() == 0) {
         // We've probably placed all of the chains.  But wait -- there's a further
         // possibility.  If the graph of chains is disconnected, then interChain
         // edges won't be enough to place them all, and we must manually place
         // the rest of the chains now.  This indeed happens, and frequently.
         // Has nothing to do with "rare" cases of a disconnected CFG.
         pdvector<unsigned>::const_iterator all_chain_ids_iter = allChainsToPlace.begin();
         pdvector<unsigned>::const_iterator all_chain_ids_finish = allChainsToPlace.end();
         for (; all_chain_ids_iter != all_chain_ids_finish; ++ all_chain_ids_iter) {
            const unsigned chain_id = *all_chain_ids_iter;
            if (placedChains.get(chain_id) == true)
               // we already placed this chain
               continue;

            assert(placedChains.get(chain_id) == false);
            //cout << "NOTE: Placing disconnected chain id " << chain_id << endl;
            
            chainsOrdering += chain_id;
            placedChains.get(placeme_chain_id) = true;
         }
         
         break;
      }
      
      // Pick the next "placeme_chain_id": find the highest-weighted edge leading
      // from an already-placed chain to one that has not already been placed.
      // (That simplifies to finding the highest-weighted edge from an
      // already-placed chain.)  Returns UINT_MAX if couldn't find one
      // (disconnected chain graph...)
      placeme_chain_id = find_next_chain(workingOnInlinedChains,
                                         placedChains, theChainsVec,
                                         interChainEdges, theBlockOracle);

      if (placeme_chain_id == UINT_MAX) {
         // could not find an inter-chain edge connecting an already-placed chain to
         // a not-yet-placed chain.  Probably because chains are disjoint.
         // But wait a minute -- wouldn't the interChainEdges.size() == 0 check
         // above have caught such a situation?
         assert(false);
      }
   }
}

static void print_1chain_bbids(unsigned chain_id, bbid_t first_bbid_in_chain,
                               const pdvector<chain_vec_entry> &v) {
   bbid_t bbid = first_bbid_in_chain;

   bool first = true;
   do {
      const chain_vec_entry &cve = v[bbid];
      assert(cve.chain_id == chain_id);

      if (!first)
         cout << ' ';
      
      cout << bbid;

      bbid = cve.next_bbid_this_chain;
      first = false;
   } while (bbid != (bbid_t)-1);
}

void pickBlockOrderingChains(const hotBlockCalcOracle &theBlockOracle,
                             const function_t &fn,
                             pdvector<bbid_t> &theInlinedBlockIds,
                             pdvector<bbid_t> &theOutlinedBlockIds) {
   // Fills in "theInlinedBlockIds[]" and "theOutlinedBlockIds[]".

   // Step 1: initialize chains vector.  (The vector, indexed by bb id,
   // contains a chain id, who's next in this chain, and who's previous in
   // this chain).  It's initialized so that each bb is in a chain of its own.

   pdvector<chain_vec_entry> the_chains_vec;
   const unsigned numBBs = fn.numBBs();
   for (bbid_t bbid = 0; bbid < numBBs; ++bbid)
      the_chains_vec += chain_vec_entry((unsigned)bbid, // chain id <-- bbid here
                                        (bbid_t)-1, (bbid_t)-1);
   
   // Step 2: Edge weights (derived as much as possible from block counts; fudged
   // when necessary).  Put into a sorted vector: highest weights first.
   // The main algorithm will loop thru this vector
   pdvector<edge_entry> the_edges_sorted;
   
   for (bbid_t from_bbid = 0; from_bbid < numBBs; ++from_bbid) {
      const basicblock_t *from_bb = fn.getBasicBlockFromId(from_bbid);
      
      basicblock_t::ConstSuccIter succ_iter = from_bb->beginSuccIter();
      basicblock_t::ConstSuccIter succ_finish = from_bb->endSuccIter();
      for (; succ_iter != succ_finish; ++succ_iter) {
         const bbid_t to_bbid = *succ_iter;
         if (to_bbid == basicblock_t::UnanalyzableBBId ||
             to_bbid == basicblock_t::interProcBranchExitBBId ||
             to_bbid == basicblock_t::xExitBBId)
            // forget about such an edge
            continue;

         const unsigned edge_weight = theBlockOracle.getEdgeWeight(from_bbid, to_bbid);
         the_edges_sorted += edge_entry(from_bbid, to_bbid, edge_weight);
      }
   }
   std::sort(the_edges_sorted.begin(), the_edges_sorted.end(), hottest_edges_first);
   
   // Step 3: loop thru all edges, highest weight first.  Merge chains where
   // possible: the two chains must fall into the same hot/cold categories
   // (That's DIFFERENT FROM PETTIS & HANSEN).
   dictionary_hash<unsigned, bbid_t> chainId2HeadOfChain(unsignedIdentityHash);
   pdvector<edge_entry> interChainEdges;
      // note that an inter-chain edge contains basic block ids, not chain ids;
      // the "from" and/or the "to" bbid's of an inter-chain edge are not
      // necessarily at the start of a chain.  To find the chains being connected
      // simply consult the_chains_vec[].

   merge_chains(numBBs,
                the_chains_vec, // updated
                chainId2HeadOfChain, // filled in
                interChainEdges, // filled in
                the_edges_sorted, // left unchanged
                theBlockOracle // left unchanged
                );

   // For later use, inter-chain edges must be sorted:
   assert(is_Sorted(interChainEdges.begin(), interChainEdges.end()));
   assert(interChainEdges.end() ==
          std::adjacent_find(interChainEdges.begin(), interChainEdges.end()));
   
   if (true) {
      cout << "Here are the hot chains of fn "
           << fn.getPrimaryName().c_str()
           << ", after merging but before placing" << endl;
      dictionary_hash<unsigned, bbid_t>::const_iterator chain_iter =
         chainId2HeadOfChain.begin();
      dictionary_hash<unsigned, bbid_t>::const_iterator chain_finish =
         chainId2HeadOfChain.end();
      for (; chain_iter != chain_finish; ++chain_iter) {
         const unsigned chain_id = chain_iter.currkey();
         const bbid_t first_bbid_in_chain = chain_iter.currval();

         if (theBlockOracle.isBlockInlined(first_bbid_in_chain)) {
            cout << "  chain " << chain_id;
            cout << " (hot)";
            cout << " has bbids (";
            print_1chain_bbids(chain_id, first_bbid_in_chain, the_chains_vec);
            cout << ")" << endl;
         }
      }
   }
   
   if (false) {
      cout << "Inter-chain edges are:" << endl;
      pdvector<edge_entry>::const_iterator interChainEdgesIter =
         interChainEdges.begin();
      pdvector<edge_entry>::const_iterator interChainEdgesFinish =
         interChainEdges.end();
      for (; interChainEdgesIter != interChainEdgesFinish; ++interChainEdgesIter) {
         const edge_entry &e = *interChainEdgesIter;

         if (theBlockOracle.isBlockInlined(e.from)) {
            assert(theBlockOracle.isBlockInlined(e.to));
            cout << "  (" << e.from << ", " << e.to << ") weight="
                 << e.weight << endl;
         }
      }
   }

   // Step 4: by considering interChainEdges[], pick an ordering among the hot
   // and among the cold chains.  WE DIFFER FROM PETTIS & HANSEN HERE; they
   // use a precedence relation hoping to make inter-chain branches forward, and
   // where not possible, try to connect chains via hottest edges.  We just use
   // that second critieria, since I don't believe it's so necessary these days
   // that the untaken conditional branches be forward.

   dictionary_hash<unsigned, bool> placedChains(unsignedIdentityHash);
   const pdvector<unsigned> all_chain_ids = chainId2HeadOfChain.keys();
   for (pdvector<unsigned>::const_iterator iter = all_chain_ids.begin();
        iter != all_chain_ids.end(); ++iter)
      placedChains.set(*iter, false);

   pdvector<unsigned> inlinedChainsOrdering;
   pdvector<unsigned> outlinedChainsOrdering;

   pdvector<unsigned> allInlinedChainsToPlace; // unordered
   pdvector<unsigned> allOutlinedChainsToPlace; // unordered

   pdvector<chain_vec_entry>::const_iterator chainsvec_iter = the_chains_vec.begin();
   pdvector<chain_vec_entry>::const_iterator chainsvec_finish = the_chains_vec.end();
   for (; chainsvec_iter != chainsvec_finish; ++chainsvec_iter) {
      const chain_vec_entry &cve = *chainsvec_iter;
      if (cve.prev_bbid_this_chain == (bbid_t)-1) {
         // This is the first bb of this chain.  We'll use this as a signal to
         // add the chain to either allInlinedChainsToPlace[] or
         // allOutlinedChainsToPlace[], as appropriate
         const unsigned chain_id = cve.chain_id;
         
         const bbid_t bbid = chainsvec_iter - the_chains_vec.begin();
         assert(&the_chains_vec[bbid] == &cve); // just a quick sanity check
         
         if (theBlockOracle.isBlockInlined(bbid))
            allInlinedChainsToPlace += chain_id; // NOT bbid
         else
            allOutlinedChainsToPlace += chain_id; // NOT bbid
      }
   }
   assert(allInlinedChainsToPlace.size() + allOutlinedChainsToPlace.size() ==
          placedChains.size());

   // Place the chain containing the entry block, then the rest of the inlined/hot
   // chains.
   const unsigned entry_chain = the_chains_vec[fn.xgetEntryBB()].chain_id;
   place_chain(entry_chain,
               true, // working on inlined, not outlined, chains
               allInlinedChainsToPlace,
               inlinedChainsOrdering, placedChains,
               the_chains_vec, interChainEdges, theBlockOracle);

   // Assert that all hot chains were placed:
#ifndef NDEBUG
   pdvector<unsigned> check_inlinedChains = inlinedChainsOrdering;
   std::sort(check_inlinedChains.begin(), check_inlinedChains.end());
   std::sort(allInlinedChainsToPlace.begin(), allInlinedChainsToPlace.end());
   assert(check_inlinedChains.size() == allInlinedChainsToPlace.size() &&
          std::equal(check_inlinedChains.begin(), check_inlinedChains.end(),
                     allInlinedChainsToPlace.begin()));
#endif   
   
   // STEP 5: Place the outlined chains.  We need one to start with.  We'll (somewhat
   // arbitrarily) choose to start off with the chain containing the hottest
   // of the outlined blocks, on the theory that, if we're going to execute
   // outlined code (which we usually shouldn't, by definition, but I digress),
   // then we might as well try to put the hottest code up towards the lower
   // addresses, which is a bit closer to the inlined code.  Obviously this is a 
   // bit of stretched reasoning, but a little xtra effort can't hurt:
   bbid_t hottestOfTheColdBlocksBBId = theBlockOracle.getHottestOfTheColdBlocks();
      // could be (bbid_t)-1 --> no cold blocks exist
   if (hottestOfTheColdBlocksBBId != (bbid_t)-1)
      place_chain(the_chains_vec[hottestOfTheColdBlocksBBId].chain_id,
                  false, // working on outlined, not inlined, chains
                  allOutlinedChainsToPlace,
                  outlinedChainsOrdering,
                  placedChains, // note that we didn't reset this to empty
                  the_chains_vec, interChainEdges, theBlockOracle);

   // Assert that all cold chains were placed:
#ifndef NDEBUG
   pdvector<unsigned> check_outlinedChains = outlinedChainsOrdering;
   std::sort(check_outlinedChains.begin(), check_outlinedChains.end());
   std::sort(allOutlinedChainsToPlace.begin(), allOutlinedChainsToPlace.end());
   assert(check_outlinedChains.size() == allOutlinedChainsToPlace.size() &&
          std::equal(check_outlinedChains.begin(), check_outlinedChains.end(),
                     allOutlinedChainsToPlace.begin()));
#endif

   // Assert that all chains, hot or cold, were placed:
#ifndef NDEBUG
   pdvector<unsigned> all_chains = inlinedChainsOrdering;
   all_chains += outlinedChainsOrdering;
   std::sort(all_chains.begin(), all_chains.end());
   assert(all_chains.end() == std::adjacent_find(all_chains.begin(), all_chains.end()));
   
   assert(inlinedChainsOrdering.size() + outlinedChainsOrdering.size() ==
          chainId2HeadOfChain.size());
#endif
   
   // STEP 6: we've got an ordering among the chains, so we're ready to
   // "emit" into theInlinedBlockIds and theOutlinedBlockIds
   pdvector<unsigned>::const_iterator chainid_iter = inlinedChainsOrdering.begin();
   pdvector<unsigned>::const_iterator chainid_finish = inlinedChainsOrdering.end();
   for (; chainid_iter != chainid_finish; ++chainid_iter) {
      const unsigned chainid = *chainid_iter;
      const bbid_t head_of_chain_bbid = chainId2HeadOfChain.get(chainid);
      
      add_to(theInlinedBlockIds, the_chains_vec,
             head_of_chain_bbid, chainid, // chainid is only used for asserts
             theBlockOracle, true);
   }

   chainid_iter = outlinedChainsOrdering.begin();
   chainid_finish = outlinedChainsOrdering.end();
   for (; chainid_iter != chainid_finish; ++chainid_iter) {
      const unsigned chainid = *chainid_iter;
      const bbid_t head_of_chain_bbid = chainId2HeadOfChain.get(chainid);
      
      add_to(theOutlinedBlockIds, the_chains_vec,
             head_of_chain_bbid, chainid, // chainid is only used for asserts
             theBlockOracle, false);
   }

   // No need to assert that all inlined blocks and all outlined blocks were
   // emitted; the caller will check for that, as it should (to be independent
   // of which block ordering fn was invoked).
}
