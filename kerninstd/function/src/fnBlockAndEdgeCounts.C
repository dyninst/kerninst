// fnBlockAndEdgeCounts.C

#include "fnBlockAndEdgeCounts.h"
#include "readySet.h"
#include <algorithm>

// -1 and -2 are unused by basicblock.[hC]:
const fnBlockAndEdgeCounts::bbid_t fnBlockAndEdgeCounts::dummyEntryBlockId = (bbid_t)-1;

// class WorkingSet: we must augment class readySet because that class doesn't have
// a framework for dummyEntryBlockId and the various exit bb ids
class WorkingSet {
 private:
   typedef fnBlockAndEdgeCounts::bbid_t bbid_t;

   readySet theReadySet;
   bool isDummyEntryBlockPresent; // represents dummyEntryBlockId
   bool isInterProcBranchExitBlockPresent; // represents interProcBranchExitBBId
   bool isXExitBlockPresent; // represents xExitBBId
   
 public:
   WorkingSet(unsigned num_regular_bbs) :
      theReadySet(num_regular_bbs, true),
      isDummyEntryBlockPresent(true),
      isInterProcBranchExitBlockPresent(true),
      isXExitBlockPresent(true)
   {
   }
  ~WorkingSet() {}

   void operator+=(bbid_t bbid) {
      if (bbid == fnBlockAndEdgeCounts::dummyEntryBlockId)
         isDummyEntryBlockPresent = true;
      else if (bbid == basicblock_t::interProcBranchExitBBId)
         isInterProcBranchExitBlockPresent = true;
      else if (bbid == basicblock_t::xExitBBId)
         isXExitBlockPresent = true;
      else
         theReadySet += bbid;
   }
   void operator-=(bbid_t bbid) {
      if (bbid == fnBlockAndEdgeCounts::dummyEntryBlockId)
         isDummyEntryBlockPresent = false;
      else if (bbid == basicblock_t::interProcBranchExitBBId)
         isInterProcBranchExitBlockPresent = false;
      else if (bbid == basicblock_t::xExitBBId)
         isXExitBlockPresent = false;
      else
         theReadySet -= bbid;
   }
   bbid_t get_and_remove_any() {
      if (isDummyEntryBlockPresent) {
         isDummyEntryBlockPresent = false;
         return fnBlockAndEdgeCounts::dummyEntryBlockId;
      }
      else if (isInterProcBranchExitBlockPresent) {
         isInterProcBranchExitBlockPresent = false;
         return basicblock_t::interProcBranchExitBBId;
      }
      else if (isXExitBlockPresent) {
         isXExitBlockPresent = false;
         return basicblock_t::xExitBBId;
      }
      else
         return theReadySet.get_and_remove_any();
   }

   bool contains(bbid_t bbid) const {
      if (bbid == fnBlockAndEdgeCounts::dummyEntryBlockId)
         return isDummyEntryBlockPresent;
      else if (bbid == basicblock_t::interProcBranchExitBBId)
         return isInterProcBranchExitBlockPresent;
      else if (bbid == basicblock_t::xExitBBId)
         return isXExitBlockPresent;
      else
         return theReadySet.contains(bbid);
   }

   unsigned size() const {
      return (isDummyEntryBlockPresent ? 1 : 0) +
         (isInterProcBranchExitBlockPresent ? 1 : 0) +
         (isXExitBlockPresent ? 1 : 0) +
         theReadySet.size();
   }

   bool empty() const {
      return !isDummyEntryBlockPresent && !isInterProcBranchExitBlockPresent &&
             !isXExitBlockPresent && theReadySet.size() == 0;
   }
   
   bool verify() const {
      return theReadySet.verify();
   }
};

// ----------------------------------------------------------------------

fnBlockAndEdgeCounts::fnBlockAndEdgeCounts(const function_t &ifn,
                                           const pdvector<unsigned> &iregBlockCounts) :
   fn(ifn),
   predsOfExitBlocks(bbid_hash, 8), // not many bins are needed here!
   regBlockCounts(iregBlockCounts),
   specialBlockCounts(bbid_hash, 16), // not many bins are needed here!
   EdgeCounts(edge_id_hash, 4*iregBlockCounts.size()), // we may need lots of bins
   numFudgedEdges(UINT_MAX) // not really defined yet
{
   // succsOfEntryBlock and predsOfExitBlocks are not yet filled (until setEdgeCounts())
   assert(regBlockCounts.size() == fn.numBBs()); // excludes dummy entry/exit blocks
   setEdgeCounts();
}

fnBlockAndEdgeCounts::~fnBlockAndEdgeCounts() {
}

// ----------------------------------------------------------------------

void fnBlockAndEdgeCounts::tryToUpdateSpecialBlockCount(bbid_t bbid) {
   if (bbid == dummyEntryBlockId)
      tryToUpdateEntryBlockCount();
   else if (bbid == basicblock_t::interProcBranchExitBBId ||
            bbid == basicblock_t::xExitBBId)
      tryToUpdateExitBlockCount(bbid);
   else
      assert(false);
}

void fnBlockAndEdgeCounts::tryToUpdateEntryBlockCount() {
   assert(specialBlockCounts.get(dummyEntryBlockId).first == UINT_MAX);
   
   // If all successors edges of the entry block have a known count, then we
   // can update.

   assert(succsOfEntryBlock.size()==1); // always true for now
   const bbid_t succ_bbid = succsOfEntryBlock[0];
   const unsigned count = EdgeCounts.get(edge_id(dummyEntryBlockId, succ_bbid));
   if (count == UINT_MAX)
      // sorry, this edge is still unknown
      return;

   // Success!
   pair<unsigned, bool> &result = specialBlockCounts.get(dummyEntryBlockId);
   assert(result.second == false); // not fudged, and of course we'll keep it that way
   assert(result.first == UINT_MAX);
   result.first = count;
      
   // No need to add entryBlock's successors to the working set...because as we
   // just established, their values are all known.

   // We used to set the exit block at the same time, but now there are several
   // exit blocks, and anyway, it was never the most robust idea to begin with
   // (consider a function that never exits, due to an intentional infinite loop, as
   // in a background thread).
}

void fnBlockAndEdgeCounts::tryToUpdateExitBlockCount(bbid_t bbid) {
   assert(bbid == basicblock_t::interProcBranchExitBBId ||
          bbid == basicblock_t::xExitBBId);
   assert(specialBlockCounts.get(bbid).first == UINT_MAX);

   unsigned sum_of_known_preds = 0;

   const pdvector<bbid_t> &preds = predsOfExitBlocks.get(bbid);
   pdvector<bbid_t>::const_iterator pred_iter = preds.begin();
   pdvector<bbid_t>::const_iterator pred_finish = preds.end();
   for (; pred_iter != pred_finish; ++pred_iter) {
      const bbid_t pred_bbid = *pred_iter;
      const unsigned count = EdgeCounts.get(edge_id(pred_bbid, bbid));
      
      if (count == UINT_MAX)
         // sorry, an edge is still unknown
         return;
      
      sum_of_known_preds += count;
   }
   
   // success!
   pair<unsigned, bool> &result = specialBlockCounts.get(bbid);
   assert(result.second == false); // not fudged, and we'll of course keep it that way
   assert(result.first == UINT_MAX);
   result.first = sum_of_known_preds;

   // No need to add this exit block's preds to the working set; as just
   // established, they are all known.

   // We used to update the entry block here, but we no longer do that...was always
   // a shaky assumption that #exits == #entries, and now that there can be several
   // kinds of exit nodes, it's a really bad idea.
}

// ----------------------------------------------------------------------

void fnBlockAndEdgeCounts::proc1SuccEdge(bbid_t bbid, bbid_t succ_bbid,
                                         unsigned blockCount,
                                         unsigned &sum_of_known_succs,
                                         unsigned &num_unknown_succs,
                                         unsigned &sum_of_unknown_succs,
                                         bbid_t &an_unknown_succ_bbid) const {
   // helper fn to GetKnownSuccsInfo(), below
   assert(blockCount != UINT_MAX);
   const unsigned edge_count = EdgeCounts.get(edge_id(bbid, succ_bbid));
   if (edge_count != UINT_MAX) { // edge count is already known
      sum_of_known_succs += edge_count;
      sum_of_unknown_succs -= edge_count;
      if (sum_of_unknown_succs > blockCount)
         // underflow
         sum_of_unknown_succs = 0;
   }
   else {
      ++num_unknown_succs;
      an_unknown_succ_bbid = succ_bbid;
   }
}

bool fnBlockAndEdgeCounts::
GetKnownSuccsInfo(bbid_t bbid,
                  unsigned &sum_of_known_succs,
                  unsigned &num_unknown_succs,
                  unsigned &sum_of_unknown_succs,
                  bbid_t &an_unknown_succ_bbid,
                  bool fudgeToEnsureABlockCountIfNeeded) const {
   unsigned blockCount;
   bool isSpecialBlock = false;
   
   pair<unsigned, bool> specialBlockCount;
   if (specialBlockCounts.find(bbid, specialBlockCount)) { // fills in specialBlockCount
      // It was a special block (entry or one of the exit blocks)
      isSpecialBlock = true;
      blockCount = specialBlockCount.first;
      if (blockCount == UINT_MAX && fudgeToEnsureABlockCountIfNeeded)
         blockCount = fudgeSpecialBlockCount(bbid);
   }
   else
      blockCount = regBlockCounts[bbid];
   
   if (blockCount == UINT_MAX) {
      assert(!fudgeToEnsureABlockCountIfNeeded);
      return false;
   }

   sum_of_known_succs = 0;
   num_unknown_succs = 0;
   sum_of_unknown_succs = blockCount; // for now...

   if (isSpecialBlock) {
      if (bbid == dummyEntryBlockId) {
         assert(succsOfEntryBlock.size() == 1);
         const bbid_t succ_bbid = succsOfEntryBlock[0];
         proc1SuccEdge(bbid, succ_bbid, blockCount, sum_of_known_succs,
                       num_unknown_succs, sum_of_unknown_succs, an_unknown_succ_bbid);
      }
      else if (bbid == basicblock_t::interProcBranchExitBBId ||
               bbid == basicblock_t::xExitBBId)
         // This block has no successors
         ;
      else
         assert(false);
   }
   else {
      // A regular block
      const basicblock_t *bb = fn.getBasicBlockFromId(bbid);
      basicblock_t::ConstSuccIter siter = bb->beginSuccIter();
      basicblock_t::ConstSuccIter sfinish = bb->endSuccIter();
      for (; siter != sfinish; ++siter) {
         bbid_t succ_bbid = *siter;
         
         assert(succ_bbid != basicblock_t::UnanalyzableBBId &&
                "unanalyzable not supported in this framework");

         // Note that the following call is OK even if succ_bbid is one of the exit bb's
         proc1SuccEdge(bbid, succ_bbid, blockCount, sum_of_known_succs,
                       num_unknown_succs, sum_of_unknown_succs, an_unknown_succ_bbid);
      }
   }
   
   return true;
}

void fnBlockAndEdgeCounts::proc1PredEdge(bbid_t bbid, bbid_t pred_bbid,
                                         unsigned blockCount,
                                         unsigned &sum_of_known_preds,
                                         unsigned &num_unknown_preds,
                                         unsigned &sum_of_unknown_preds,
                                         bbid_t &an_unknown_pred_bbid) const {
   // helper fn to GetKnownPredsInfo(), below
   assert(blockCount != UINT_MAX);

   // Notice the order of args to the edge_id ctor:
   const unsigned edge_count = EdgeCounts.get(edge_id(pred_bbid, bbid));
   if (edge_count != UINT_MAX) { // edge count is already known
      sum_of_known_preds += edge_count;
      sum_of_unknown_preds -= edge_count;
      if (sum_of_unknown_preds > blockCount)
         // underflow
         sum_of_unknown_preds = 0;
   }
   else {
      ++num_unknown_preds;
      an_unknown_pred_bbid = pred_bbid;
   }
}

bool fnBlockAndEdgeCounts::
GetKnownPredsInfo(bbid_t bbid,
                  unsigned &sum_of_known_preds,
                  unsigned &num_unknown_preds,
                  unsigned &sum_of_unknown_preds,
                  bbid_t &an_unknown_pred_bbid,
                  bool fudgeToEnsureABlockCountIfNeeded) const {
   // like GetKnownSuccsInfo().  Returns false in the same circumstance.

   unsigned blockCount;
   bool isSpecialBlock = false;
   
   pair<unsigned, bool> specialBlockCount;
   if (specialBlockCounts.find(bbid, specialBlockCount)) { // fills in specialBlockCount
      // It was a special block (entry or one of the exit blocks)
      isSpecialBlock = true;
      blockCount = specialBlockCount.first;
      if (blockCount == UINT_MAX && fudgeToEnsureABlockCountIfNeeded)
         blockCount = fudgeSpecialBlockCount(bbid);
   }
   else
      blockCount = regBlockCounts[bbid];
   
   if (blockCount == UINT_MAX) {
      assert(!fudgeToEnsureABlockCountIfNeeded);
      return false;
   }

   sum_of_known_preds = 0;
   num_unknown_preds = 0;
   sum_of_unknown_preds = blockCount; // for now...

   if (isSpecialBlock) {
      if (bbid == dummyEntryBlockId)
         // this block has no preds
         ;
      else if (bbid == basicblock_t::interProcBranchExitBBId ||
               bbid == basicblock_t::xExitBBId) {
         const pdvector<bbid_t> &preds = predsOfExitBlocks.get(bbid);
         pdvector<bbid_t>::const_iterator iter = preds.begin();
         pdvector<bbid_t>::const_iterator finish = preds.end();
         for (; iter != finish; ++iter) {
            const bbid_t pred_bbid = *iter;
            proc1PredEdge(bbid, pred_bbid, blockCount, sum_of_known_preds,
                          num_unknown_preds, sum_of_unknown_preds,
                          an_unknown_pred_bbid);
         }
      }
      else
         assert(false);
   }
   else {
      // A regular node
      const basicblock_t *bb = fn.getBasicBlockFromId(bbid);
      basicblock_t::ConstPredIter piter = bb->beginPredIter();
      basicblock_t::ConstPredIter pfinish = bb->endPredIter();
      for (; piter != pfinish; ++piter) {
         const bbid_t pred_bbid = *piter;
         proc1PredEdge(bbid, pred_bbid, blockCount, sum_of_known_preds,
                       num_unknown_preds, sum_of_unknown_preds, an_unknown_pred_bbid);
      }

      // If this block is fn.getEntryBB(), then there is one more predecessor:
      // dummyEntryBlockId.  Ugly hack.  Sorry.
      if (bbid == fn.xgetEntryBB()) {
         const bbid_t pred_bbid = dummyEntryBlockId;
         proc1PredEdge(bbid, pred_bbid, blockCount, sum_of_known_preds,
                       num_unknown_preds, sum_of_unknown_preds, an_unknown_pred_bbid);
      }
   }
   
   return true;
}

void fnBlockAndEdgeCounts::
tryToUpdateExactEdgeCounts1NodeDueToSuccs(WorkingSet &theWorkingSet, bbid_t bbid) {
   unsigned sum_of_known_succs = 0;
   unsigned num_unknown_succs = 0;
   bbid_t an_unknown_succid; // uninitialized is OK
   unsigned sum_of_unknown_succs = 0;

   if (GetKnownSuccsInfo(bbid,
                         sum_of_known_succs,
                         num_unknown_succs, sum_of_unknown_succs, an_unknown_succid,
                         false) &&
       num_unknown_succs == 1)
   {
      unsigned &succ_edge_count = EdgeCounts.get(edge_id(bbid, an_unknown_succid));
      assert(succ_edge_count == UINT_MAX);
      assert(sum_of_unknown_succs != UINT_MAX);
      succ_edge_count = sum_of_unknown_succs;

      // Add the successor to the ready set...one of its pred edges was changed, so
      // another of its pred edges may be updatable now.  No need to add the
      // predecessor to the working set; sure, we've just updated one of its successor
      // edges, but we're also sure, in the body of this "if" stmt, that we're all
      // done messing with its successor edges.
      theWorkingSet += an_unknown_succid;
   }
}

void fnBlockAndEdgeCounts::
tryToUpdateExactEdgeCounts1NodeDueToPreds(WorkingSet &theWorkingSet, bbid_t bbid) {
   unsigned sum_of_known_preds = 0;
   unsigned num_unknown_preds = 0;
   unsigned sum_of_unknown_preds = 0;
   bbid_t an_unknown_predid; // uninitialized is OK

   if (GetKnownPredsInfo(bbid,
                         sum_of_known_preds,
                         num_unknown_preds, sum_of_unknown_preds, an_unknown_predid,
                         false) &&
       num_unknown_preds == 1)
   {
      // Note the ordering of args to the edge_id ctor
      unsigned &pred_edge_count = EdgeCounts.get(edge_id(an_unknown_predid, bbid));
      assert(pred_edge_count == UINT_MAX);
      assert(sum_of_unknown_preds != UINT_MAX);
      pred_edge_count = sum_of_unknown_preds;

      // Add the "from" block to the working set...of of its successor edges has
      // changed, and perhaps as a result another of its successor edges can be
      // updated.  No need to add the "to" block; yes, one of its pred edges has
      // changed, but we're sure that we've (successfully) updated all that we can
      // regarding the pred edges, and there's nothing left to do.
      theWorkingSet += an_unknown_predid;
   }
}

void fnBlockAndEdgeCounts::
tryToUpdateExactInfo1Node(WorkingSet &theWorkingSet, bbid_t bbid) {
   // Usually tries to update edge counts, but in the case of entry/exit blocks,
   // also tries to update block counts.

   if (bbid == dummyEntryBlockId || bbid == basicblock_t::interProcBranchExitBBId ||
       bbid == basicblock_t::xExitBBId)
   {
      if (specialBlockCounts.get(bbid).first == UINT_MAX)
         tryToUpdateSpecialBlockCount(bbid);
   }

   tryToUpdateExactEdgeCounts1NodeDueToSuccs(theWorkingSet, bbid);
   tryToUpdateExactEdgeCounts1NodeDueToPreds(theWorkingSet, bbid);
}

// ----------------------------------------------------------------------

void fnBlockAndEdgeCounts::
calcEdgesThatNeedFudging(pdvector<edge_id> &edgesThatNeedFudging) const {
   assert(edgesThatNeedFudging.size() == 0);
   
   dictionary_hash<edge_id, unsigned>::const_iterator eiter = EdgeCounts.begin();
   dictionary_hash<edge_id, unsigned>::const_iterator efinish = EdgeCounts.end();
   for (; eiter != efinish; ++eiter) {
      const edge_id &id = eiter.currkey();
      const unsigned count = eiter.currval();

      if (id.from_bbid != dummyEntryBlockId &&
          id.from_bbid != basicblock_t::interProcBranchExitBBId &&
          id.from_bbid != basicblock_t::xExitBBId)
         assert(fn.getBasicBlockFromId(id.from_bbid)->hasAsASuccessor(id.to_bbid));
      
      if (count == UINT_MAX)
         edgesThatNeedFudging += id;
   }
}

unsigned fnBlockAndEdgeCounts::
calc1FudgedEdgeCount(const edge_id &id) const {
   // Each edge is both a successor and a predecessor (to different nodes, unless
   // the edge represents a single-basic-block loop).  So, there are usually two
   // formulas that can help guide the fudge:
   //
   // a) the sum of this edge, and all successors of the "from" basic block, must
   // add up to the bbCount of the "from" basic block.  At least two of these
   // edges will be unknown.  The unknown edges must sum to (bbCount minus the
   // sum of the known edges).  So, any one edge cannot be larger than this sum.
   // --> Thus, we have a maximum bound.
   //
   // b) the sum of this edge, and all predecessors of the "to" basic block, must
   // add up to the bbCount of the "to" basic block.  At least two such edges will
   // be unknown.  The unknown edges must sum to (bbCount minus the sum of the
   // known edges).  So, any one edge cannot be larger than this sum.
   // --> Thus, we have a further contributer to the maximum bound.
   //
   // For now, rather arbitrarily, we'll choose the maximum value allowed under
   // these bounds.

   // ----------------------------------------------------------------------

   unsigned sum_of_known_succs = 0;
   unsigned num_unknown_succs = 0;
   bbid_t an_unknown_succid; // uninitialized is OK
   unsigned sum_of_unknown_succs = 0;

   if (false == GetKnownSuccsInfo(id.from_bbid,
                                  sum_of_known_succs,
                                  num_unknown_succs, sum_of_unknown_succs,
                                  an_unknown_succid, true))
      assert(0); // shouldn't happen since we passed true as the last param

   unsigned max_edge_count1 = sum_of_unknown_succs;
   
   assert(num_unknown_succs != 0 && "already known?");
   if (num_unknown_succs == 1 && knownBlockCount(id.from_bbid))
      assert(false && "could have been calculated?");

   // ----------------------------------------------------------------------

   unsigned sum_of_known_preds = 0;
   unsigned num_unknown_preds = 0;
   bbid_t an_unknown_predid; // uninitialized is OK
   unsigned sum_of_unknown_preds = 0;

   if (false == GetKnownPredsInfo(id.to_bbid,
                                  sum_of_known_preds,
                                  num_unknown_preds, sum_of_unknown_preds,
                                  an_unknown_predid, true))
      assert(0); // shouldn't happen since we passed true as the last param
   
   unsigned max_edge_count2 = sum_of_unknown_preds;

   assert(num_unknown_preds != 0 && "already known?");
   if (num_unknown_preds == 1 && knownBlockCount(id.to_bbid))
      assert(false && "could have been calculated?");
   
   // ----------------------------------------------------------------------
   
   // return the minimum of (a) the maximum as calculated by successors and
   // (b) the maximum as calculated by predecessors

   if (max_edge_count1 < max_edge_count2)
      return max_edge_count1;
   else
      return max_edge_count2;
}

void fnBlockAndEdgeCounts::fudgeRemainingEdges() {
   // NOTE: Also, we don't assign the fudged values to the graph until the end of
   // this routine, to avoid affecting the values of other fudges, and perhaps
   // even trip an assert that either 0 or at least 2 edges are unknown.
   
   pdvector<edge_id> edgesThatNeedFudging;
   calcEdgesThatNeedFudging(edgesThatNeedFudging); // fills in the param

   dictionary_hash<edge_id, unsigned> fudgedEdgeCounts(edge_id_hash);
   
   pdvector<edge_id>::const_iterator eiter = edgesThatNeedFudging.begin();
   pdvector<edge_id>::const_iterator efinish = edgesThatNeedFudging.end();
   for (; eiter != efinish; ++eiter) {
      const edge_id &id = *eiter;

      if (id.from_bbid != dummyEntryBlockId)
         assert(fn.getBasicBlockFromId(id.from_bbid)->hasAsASuccessor(id.to_bbid));

      if (id.to_bbid != basicblock_t::interProcBranchExitBBId &&
          id.to_bbid != basicblock_t::xExitBBId &&
          id.from_bbid != dummyEntryBlockId)
         assert(fn.getBasicBlockFromId(id.to_bbid)->hasAsAPredecessor(id.from_bbid));

      const unsigned count = calc1FudgedEdgeCount(id);
      fudgedEdgeCounts.set(id, count);
   }

   // Done fudging
   numFudgedEdges = fudgedEdgeCounts.size();
   
   // And finally, assign the fudged edges to EdgeCounts[] (we've delayed doing this
   // until now, to ensure that such fudged values do not affect calculations of
   // other fudged values)
   assert(fudgedEdgeCounts.size() == edgesThatNeedFudging.size());
   eiter = edgesThatNeedFudging.begin();
   for (; eiter != efinish; ++eiter) {
      const edge_id &id = *eiter;
      
      unsigned &count = EdgeCounts.get(id);
      assert(count == UINT_MAX);
      count = fudgedEdgeCounts.get(id);
   }
}

// ----------------------------------------------------------------------

unsigned fnBlockAndEdgeCounts::getNumUncalculatedSuccEdges(bbid_t bbid) const {
   unsigned result = 0;

   if (bbid == dummyEntryBlockId) {
      assert(succsOfEntryBlock.size() == 1); // always true in current implementation
      const bbid_t succ_bbid = succsOfEntryBlock[0];
      if (EdgeCounts.get(edge_id(bbid, succ_bbid)) == UINT_MAX)
         ++result;
      return result;
   }
   else if (bbid == basicblock_t::interProcBranchExitBBId ||
            bbid == basicblock_t::xExitBBId)
      return 0;

   // A "regular" basic block:
   const basicblock_t *bb = fn.getBasicBlockFromId(bbid);
   basicblock_t::ConstSuccIter siter = bb->beginSuccIter();
   basicblock_t::ConstSuccIter sfinish = bb->endSuccIter();
   for (; siter != sfinish; ++siter) {
      bbid_t succ_bbid = *siter;
      if (EdgeCounts.get(edge_id(bbid, succ_bbid)) == UINT_MAX)
         ++result;
   }
   return result;
}

unsigned fnBlockAndEdgeCounts::getNumUncalculatedPredEdges(bbid_t bbid) const {
   unsigned result = 0;

   if (bbid == dummyEntryBlockId)
      return 0;
   else if (bbid == basicblock_t::interProcBranchExitBBId ||
            bbid == basicblock_t::xExitBBId) {
      const pdvector<bbid_t> &preds = predsOfExitBlocks.get(bbid);
      pdvector<bbid_t>::const_iterator iter = preds.begin();
      pdvector<bbid_t>::const_iterator finish = preds.end();
      for (; iter != finish; ++iter) {
         const bbid_t pred_bbid = *iter;
         if (EdgeCounts.get(edge_id(pred_bbid, bbid)) == UINT_MAX)
            ++result;
      }
      return result;
   }

   // A "regular" basic block:
   const basicblock_t *bb = fn.getBasicBlockFromId(bbid);
   basicblock_t::ConstPredIter piter = bb->beginPredIter();
   basicblock_t::ConstPredIter pfinish = bb->endPredIter();
   for (; piter != pfinish; ++piter) {
      const bbid_t pred_bbid = *piter;
      // note the order of args to edge_id below.
      if (EdgeCounts.get(edge_id(pred_bbid, bbid)) == UINT_MAX)
         ++result;
   }

   // fn.xgetEntryBB() has 1 additional pred:
   if (bbid == fn.xgetEntryBB() &&
       EdgeCounts.get(edge_id(dummyEntryBlockId, fn.xgetEntryBB())) == UINT_MAX)
      ++result;
   
   return result;
}

bool fnBlockAndEdgeCounts::have_calculated_all_edges_possible() const {
   const unsigned num_bbs = fn.numBBs();
   for (bbid_t bbid = 0; bbid < num_bbs; ++bbid) {
      assert(regBlockCounts[bbid] != UINT_MAX);
      
      if (getNumUncalculatedSuccEdges(bbid) == 1 ||
          getNumUncalculatedPredEdges(bbid) == 1)
         return false;
   }

   if (specialBlockCounts.get(dummyEntryBlockId).first != UINT_MAX) {
      // we've got a good block count for the dummy entry block id, so we can
      // check that the # of unknown succ edges is not 1 (the usual assert for
      // this function)
      if (getNumUncalculatedSuccEdges(dummyEntryBlockId) == 1)
         return false;
   }
   
   if (specialBlockCounts.get(basicblock_t::interProcBranchExitBBId).first != UINT_MAX) {
      // we've got a good block count; # of unknown preds shouldn't be 1.
      if (getNumUncalculatedPredEdges(basicblock_t::interProcBranchExitBBId) == 1)
         return false;
   }
   
   if (specialBlockCounts.get(basicblock_t::xExitBBId).first != UINT_MAX) {
      // we've got a good block count; # of unknown preds shouldn't be 1.
      if (getNumUncalculatedPredEdges(basicblock_t::xExitBBId) == 1)
         return false;
   }
   
   // success
   return true;
}

unsigned fnBlockAndEdgeCounts::fudgeSpecialBlockCount(bbid_t bbid) const {
   if (specialBlockCounts.get(bbid).first != UINT_MAX)
      // it's already known; no need to fudge it
      return UINT_MAX;
   
   // Assert that (dummy) entry block doesn't have all known successors
   // Presently, the dummy entry block always has exactly 1 successor, as follows:
   if (bbid == dummyEntryBlockId) {
      assert(getNumUncalculatedSuccEdges(dummyEntryBlockId) != 0);

      // The dummy entry block can be at most the count of the "proper" entry block:
      assert(succsOfEntryBlock.size() == 1); // always true at present
      unsigned upper_bound = regBlockCounts[succsOfEntryBlock[0]];

      // The dummy entry block must be at least the sum of the known succ edges.
      // But for this block, there's always just 1 succ edge, and it's unknown
      // (we can assert it), so we'd just be saying that the result must be >= 0
      assert(EdgeCounts.get(edge_id(dummyEntryBlockId, succsOfEntryBlock[0])) ==
             UINT_MAX);
      
      return upper_bound;
   }
   
   if (bbid == basicblock_t::interProcBranchExitBBId ||
       bbid == basicblock_t::xExitBBId) {
      assert(getNumUncalculatedPredEdges(bbid) != 0);

      // must be at least the sum of the known pred edges
      // can be at most the sum of the known pred edges plus the sum of the
      // pred block counts for the unknown edges

      unsigned sum_known_pred_edges = 0;
      unsigned sum_pred_block_counts_for_unknown_edges = 0;

      const pdvector<bbid_t> &preds = predsOfExitBlocks.get(bbid);
      pdvector<bbid_t>::const_iterator iter = preds.begin();
      pdvector<bbid_t>::const_iterator finish = preds.end();
      for (; iter != finish; ++iter) {
         const bbid_t pred_bbid = *iter;
         const unsigned pred_edge_count = EdgeCounts.get(edge_id(pred_bbid, bbid));
         if (pred_edge_count != UINT_MAX)
            sum_known_pred_edges += pred_edge_count;
         else
            sum_pred_block_counts_for_unknown_edges += regBlockCounts[pred_bbid];
      }
      
      const unsigned lower_bound = sum_known_pred_edges;
      const unsigned upper_bound = sum_known_pred_edges + sum_pred_block_counts_for_unknown_edges;

      assert(upper_bound >= lower_bound);
      // As usual, this class (rather arbitrarily) chooses the highest possible
      // value
      return upper_bound;
   }
   else {
      assert(false);
      abort(); // placate compiler when compiling NDEBUG
   }
}

static bool no_dups(const pdvector<fnBlockAndEdgeCounts::bbid_t> &v) {
   pdvector<fnBlockAndEdgeCounts::bbid_t> check_vec(v);
   std::sort(check_vec.begin(), check_vec.end());
   return (check_vec.end() == std::adjacent_find(check_vec.begin(), check_vec.end()));
}

void fnBlockAndEdgeCounts::setEdgeCounts() {
   // Fills in EdgeCounts[].  Some fudging may be needed, because we only have
   // block counts to work with -- and we don't have the block counts for two
   // important blocks: the dummy entry/exit blocks.  We try our best.
   // Called from ctor.

   assert(EdgeCounts.size() == 0);
   assert(succsOfEntryBlock.size() == 0);
   assert(predsOfExitBlocks.size() == 0);

   // Initialize succsOfEntryBlock[] and its EdgeCounts[] entries:
   EdgeCounts.set(edge_id(dummyEntryBlockId, fn.xgetEntryBB()), UINT_MAX);
   succsOfEntryBlock += fn.xgetEntryBB();

   // Initialize predsOfExitBlocks[] and its EdgeCounts[] entries (latter part not
   // yet needed since we have no specific edges):
   predsOfExitBlocks.set(basicblock_t::interProcBranchExitBBId, pdvector<bbid_t>());
   predsOfExitBlocks.set(basicblock_t::xExitBBId, pdvector<bbid_t>());

   // Initialize regular edges:
   const unsigned num_bbs = fn.numBBs();
   for (bbid_t bbid = 0; bbid < num_bbs; ++bbid) {
      const basicblock_t *bb = fn.getBasicBlockFromId(bbid);
      
      basicblock_t::ConstSuccIter succ_iter = bb->beginSuccIter();
      basicblock_t::ConstSuccIter succ_finish = bb->endSuccIter();
      for (; succ_iter != succ_finish; ++succ_iter) {
         bbid_t succ_bbid = *succ_iter;
         assert(succ_bbid != basicblock_t::UnanalyzableBBId &&
                "unanalyzable edges unsupported in this framework");
         
         if (succ_bbid == basicblock_t::interProcBranchExitBBId ||
             succ_bbid == basicblock_t::xExitBBId)
            predsOfExitBlocks.get(succ_bbid) += bbid;
         else
            assert(fn.getBasicBlockFromId(succ_bbid)->hasAsAPredecessor(bbid));

         EdgeCounts.set(edge_id(bbid, succ_bbid), UINT_MAX);
      }
   }

   // Initialize specialBlockCounts[]:
   assert(specialBlockCounts.size() == 0);
   specialBlockCounts.set(dummyEntryBlockId, std::make_pair(UINT_MAX, false));
   specialBlockCounts.set(basicblock_t::interProcBranchExitBBId,
                          std::make_pair(UINT_MAX, false));
   specialBlockCounts.set(basicblock_t::xExitBBId, std::make_pair(UINT_MAX, false));

   assert(no_dups(predsOfExitBlocks.get(basicblock_t::interProcBranchExitBBId)));
   assert(no_dups(predsOfExitBlocks.get(basicblock_t::xExitBBId)));

   // algorithm (until convergence):
   WorkingSet theWorkingSet(num_bbs);

   while (!theWorkingSet.empty()) {
      // Find an entry in the working set
      bbid_t bbid = theWorkingSet.get_and_remove_any();
      tryToUpdateExactInfo1Node(theWorkingSet, bbid);
   }

   // Assert that the working set is really empty
   assert(theWorkingSet.verify());

   // Assert that we've calculated as much as we can (no nodes having exactly 1
   // unknown successor value or 1 unknown predecessor value)
   assert(have_calculated_all_edges_possible());

   // Now we need to fudge counts for the rest of the edges.
   pair<unsigned, bool> entryBlockCount = specialBlockCounts.get(dummyEntryBlockId);
      // note we don't use a reference...we don't want changes to take effect
      // until after the call to fudgeRemainingEdges()
   if (entryBlockCount.first == UINT_MAX) {
      // needs fudging
      entryBlockCount.first = fudgeSpecialBlockCount(dummyEntryBlockId);
      entryBlockCount.second = true;
   }
   else
      assert(entryBlockCount.second == false);
   
   pair<unsigned, bool> interProcBranchExitBlockCount =
      specialBlockCounts.get(basicblock_t::interProcBranchExitBBId);
      // note we don't use a reference...we don't want changes to take effect
      // until after the call to fudgeRemainingEdges()
   if (interProcBranchExitBlockCount.first == UINT_MAX) {
      // needs fudging
      interProcBranchExitBlockCount.first =
         fudgeSpecialBlockCount(basicblock_t::interProcBranchExitBBId);
      interProcBranchExitBlockCount.second = true;
   }
   else
      assert(interProcBranchExitBlockCount.second == false);
   
   pair<unsigned, bool> xExitBlockCount =
      specialBlockCounts.get(basicblock_t::xExitBBId);
      // note we don't use a reference...we don't want changes to take effect
      // until after the call to fudgeRemainingEdges()
   if (xExitBlockCount.first == UINT_MAX) {
      // needs fudging
      xExitBlockCount.first = fudgeSpecialBlockCount(basicblock_t::xExitBBId);
      xExitBlockCount.second = true;
   }
   else
      assert(xExitBlockCount.second == false);

   fudgeRemainingEdges();

   // And finally, apply the fudged entry/exit count...which was not applied before
   // now so as to avoid affecting fudgeRemainingEdges().
   if (entryBlockCount.second == true) {
      assert(entryBlockCount.first != UINT_MAX);
      specialBlockCounts.get(dummyEntryBlockId) = entryBlockCount;
   }

   if (interProcBranchExitBlockCount.second == true) {
      assert(interProcBranchExitBlockCount.first != UINT_MAX);
      specialBlockCounts.get(basicblock_t::interProcBranchExitBBId) =
         interProcBranchExitBlockCount;
   }
      
   if (xExitBlockCount.second == true) {
      assert(xExitBlockCount.first != UINT_MAX);
      specialBlockCounts.get(basicblock_t::xExitBBId) = xExitBlockCount;
   }
}
