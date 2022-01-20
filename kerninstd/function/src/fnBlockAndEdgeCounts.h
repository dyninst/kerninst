// fnBlockAndEdgeCounts.h

#ifndef _FN_BLOCK_AND_EDGE_COUNTS_H_
#define _FN_BLOCK_AND_EDGE_COUNTS_H_

#include "funkshun.h"
#include "util/h/Dictionary.h"
#include "util/h/hashFns.h"

class WorkingSet; // fwd decl; not defined until .C file

class fnBlockAndEdgeCounts {
 public:
   typedef function_t::bbid_t bbid_t;
   static const bbid_t dummyEntryBlockId;

   struct edge_id {
      bbid_t from_bbid; // could be dummyEntryBlockId or one of the exit bb ids
      bbid_t to_bbid; // could be dummyEntryBlockId or one of the exit bb ids
      
      edge_id(bbid_t ifrom_bbid, bbid_t ito_bbid) :
         from_bbid(ifrom_bbid), to_bbid(ito_bbid) {
      }
      bool operator==(const edge_id &other) const {
         return from_bbid == other.from_bbid && to_bbid == other.to_bbid;
      }
   };

 private:
   static unsigned bbid_hash(const bbid_t &id) { return id; }

   const function_t &fn;
   pdvector<bbid_t> succsOfEntryBlock; // useful since this info isn't stored elsewhere
      // Will contain exactly one entry, since there's just 1 entry point for now
      // (fn.getEntryBB())
   dictionary_hash<bbid_t, pdvector<bbid_t> > predsOfExitBlocks;
      // useful since this info isn't stored elsewhere.
      // A dictionary: the keys can be, e.g. interProcBranchExitBBId or
      // xExitBBId (actually, right now, those are the only two possibilities)

   const pdvector<unsigned> regBlockCounts;
      // Indexed by bbid.
      // reg --> "regular", meaning that block counts for the dummy
      // entry and exit basic blocks are not present in this vector
   dictionary_hash<bbid_t, pair<unsigned, bool> > specialBlockCounts;
      // complements regBlockCounts
      // key can be, e.g. dummyEntryBlockId, interProcBranchExitBBId, or
      // xExitBBId.  (Right now those are the only 3 possibilities)
      // value.first: block count (UINT_MAX if unknown)
      // value.second: true iff this value is fudged

   static unsigned edge_id_hash(const edge_id &id) {
      return addrHash((uint32_t)id.from_bbid) + addrHash((uint32_t)id.to_bbid);
         // not addrHash4 or anything like that...we want all bits to be considered
   }
   static bool edgeid_lessthan(const edge_id &e1, const edge_id &e2) {
      if (e1.from_bbid < e2.from_bbid)
         return true;
      else if (e1.from_bbid > e2.from_bbid)
         return false;
      
      // equal "from", so check "to"
      return (e1.to_bbid < e2.to_bbid);
   }
   
   dictionary_hash<edge_id, unsigned> EdgeCounts;
      // includes edges to/from the dummy nodes (entry & the several kinds of
      // exit nodes)
   unsigned numFudgedEdges;

   bool knownBlockCount(bbid_t bbid) const {
      if (bbid == dummyEntryBlockId ||
          bbid == basicblock_t::interProcBranchExitBBId ||
          bbid == basicblock_t::xExitBBId)
         return specialBlockCounts.get(bbid).first != UINT_MAX;
      else
         return true;
   }

   unsigned fudgeSpecialBlockCount(bbid_t bbid) const;
      // bbid: entry or one of the exit blocks

   void proc1SuccEdge(bbid_t bbid, bbid_t succ_bbid, unsigned blockCount,
                      unsigned &sum_of_known_succs,
                      unsigned &num_unknown_succs,
                      unsigned &sum_of_unknown_succs,
                      bbid_t &an_unknown_succ_bbid) const;
      // helper fn to GetKnownSuccsInfo(), below

   bool GetKnownSuccsInfo(bbid_t, unsigned &sum_of_known_succs,
                          unsigned &num_unknown_succs, unsigned &sum_of_unknown_succs,
                          bbid_t &an_unknown_succ_bbid,
                          bool fudgeToEnsureABlockCountIfNeeded) const;
      // returns false iff the block count for the bbid is unknown (prob. entry/exit)

   void proc1PredEdge(bbid_t bbid, bbid_t pred_bbid, unsigned blockCount,
                      unsigned &sum_of_known_preds,
                      unsigned &num_unknown_preds,
                      unsigned &sum_of_unknown_preds,
                      bbid_t &an_unknown_pred_bbid) const;
      // helper fn to GetKnownPredsInfo(), below

   bool GetKnownPredsInfo(bbid_t bbid,
                          unsigned &sum_of_known_preds,
                          unsigned &num_unknown_preds,
                          unsigned &sum_of_unknown_preds,
                          bbid_t &an_unknown_pred_bbid,
                          bool fudgeToEnsureABlockCountIfNeeded) const;
      // returns false iff the block count for the bbid is unknown (prob. entry/exit)

   void tryToUpdateSpecialBlockCount(bbid_t bbid);
   void tryToUpdateEntryBlockCount();
   void tryToUpdateExitBlockCount(bbid_t theExitBlockId);
      // 2d param is needed because now there can be several kinds of exit blocks
      // (well, two at the moment)
   void tryToUpdateExactEdgeCounts1NodeDueToPreds(WorkingSet &, bbid_t);
   void tryToUpdateExactEdgeCounts1NodeDueToSuccs(WorkingSet &, bbid_t);
   void tryToUpdateExactInfo1Node(WorkingSet &, bbid_t);
      // Usually tries to update edge counts, but in the case of entry/exit blocks,
      // also tries to update block counts.

   void calcEdgesThatNeedFudging(pdvector<edge_id> &) const;
   unsigned calc1FudgedEdgeCount(const edge_id &id) const;
   void fudgeRemainingEdges();
   
   void setEdgeCounts(); // calculates where possible, fudges where necessary

   unsigned getNumUncalculatedSuccEdges(bbid_t bbid) const;
   unsigned getNumUncalculatedPredEdges(bbid_t bbid) const;
   bool have_calculated_all_edges_possible() const;

 public:
   fnBlockAndEdgeCounts(const function_t &,
                        const pdvector<unsigned> &regBlockCounts);
  ~fnBlockAndEdgeCounts();
   
   unsigned getEdgeWeight(bbid_t from_bbid, bbid_t to_bbid) const {
      // Note that "dummyEntryBlockId" or one of the exit bb ids are allowed for params.
      const unsigned result = EdgeCounts.get(edge_id(from_bbid, to_bbid));
      assert(result != UINT_MAX);
      return result;
   }

   unsigned getNumTimesFnWasCalled() const {
      // we're clever here: not just bbCounts[fn.getEntryBB()], because that would
      // be wrong in the case of recursion, e.g. (when the entry block has a
      // successor other than the dummy entry block).  So, we get an edge
      // count: the edge from the dummy entry block to fn.getEntryBB().

      const unsigned result_via_block = specialBlockCounts.get(dummyEntryBlockId).first;
         // .second tells us whether the value had to be fudged
      assert(result_via_block != UINT_MAX);

      assert(succsOfEntryBlock.size() == 1 && succsOfEntryBlock[0] == fn.xgetEntryBB());

#ifndef NDEBUG      
      const unsigned result_via_edge = 
         EdgeCounts.get(edge_id(dummyEntryBlockId, succsOfEntryBlock[0]));
      assert(result_via_edge != UINT_MAX);

      assert(result_via_block == result_via_edge);
      assert(result_via_block <= regBlockCounts[fn.xgetEntryBB()]);
#endif

      return result_via_block;
   }

   bool hadToFudgeEntryBlockCount() const {
      return specialBlockCounts.get(dummyEntryBlockId).second;
   }
   
   unsigned getNumFudgedEdges() const {
      // includes edges to/from the "dummy" entry & exit blocks.  Is this desirable?
      assert(numFudgedEdges <= getTotalNumEdges());
      return numFudgedEdges;
   }
   
   unsigned getTotalNumEdges() const {
      // includes edges to/from the "dummy" entry & exit blocks.  Is this desirable?
      return EdgeCounts.size();
   }

   unsigned getNumRegularBlocks() const {
      const unsigned result = regBlockCounts.size();
      assert(result == fn.numBBs());
      return result;
   }

   unsigned getBlockCount(bbid_t bbid) const {
      if (bbid == dummyEntryBlockId || bbid == basicblock_t::interProcBranchExitBBId ||
          bbid == basicblock_t::xExitBBId)
         return specialBlockCounts.get(bbid).first;
      else
         return regBlockCounts[bbid];
   }

   typedef dictionary_hash<edge_id, unsigned>::const_iterator const_iterator;
   
   const_iterator begin() const { return EdgeCounts.begin(); }
   const_iterator end() const { return EdgeCounts.end(); }
};

#endif
