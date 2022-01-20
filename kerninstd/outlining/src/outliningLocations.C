// outliningLocations.C

#include "util/h/hashFns.h"
#include "util/h/rope-utils.h"
#include "outliningLocations.h"
#include "funkshun.h"
#include <algorithm> // STL's sort()

static bool modAndFnPtrPairCmp(const pair<pdstring, const function_t *> &fn1,
                               const pair<pdstring, const function_t *> &fn2) {
   return fn1.second->getEntryAddr() < fn2.second->getEntryAddr();
}

class addrCmp {
 public:
   bool operator()(const pair<pdstring, const function_t *> &fn1, kptr_t addr) {
      return fn1.second->getEntryAddr() < addr;
   }
   bool operator()(kptr_t addr, const pair<pdstring, const function_t *> &fn2) {
      return addr < fn2.second->getEntryAddr();
   }

   addrCmp() {}
};

outliningLocations::
outliningLocations(unsigned igroupUniqueId,
                   howFarAway iwhereOutlinedBlocksReside,
                   const pdvector< pair<pdstring, const function_t*> > &iFuncsBeingInlinedAndOutlined,
                   bool ijustTestingNoChanges,
                   const pdvector<bbid_t> &iInlinedBlocks,
                   const pdvector<bbid_t> &iOutlinedBlocks) :
      groupUniqueId(igroupUniqueId),
      whereOutlinedBlocksReside(iwhereOutlinedBlocksReside),
      funcsBeingInlinedAndOutlined(iFuncsBeingInlinedAndOutlined),
      justTestingNoChanges(ijustTestingNoChanges),
      inlinedBlocks(iInlinedBlocks),
      outlinedBlocks(iOutlinedBlocks),
      blockId2OrderingForInlinedBlocks(bbidHash),
      blockId2OrderingForOutlinedBlocks(bbidHash)
{
   sort(funcsBeingInlinedAndOutlined.begin(), funcsBeingInlinedAndOutlined.end(),
        modAndFnPtrPairCmp);

   pdvector<bbid_t>::const_iterator iter = inlinedBlocks.begin();
   pdvector<bbid_t>::const_iterator finish = inlinedBlocks.end();
   unsigned ordering = 0;
   for (; iter != finish; ++iter) {
      bbid_t id = *iter;
      blockId2OrderingForInlinedBlocks.set(id, ordering++);
   }
   
   iter = outlinedBlocks.begin();
   finish = outlinedBlocks.end();
   ordering = 0;
   for (; iter != finish; ++iter) {
      bbid_t id = *iter;
      blockId2OrderingForOutlinedBlocks.set(id, ordering++);
   }
}

outliningLocations::bbid_t
outliningLocations::whoIfAnyoneWillBeEmittedAfter(bbid_t id) const {
   // returns bb id of the block (from the same group as 'id': inlined/outlined)
   // that will be emitted after 'id'.  If none (different 'group' or different fn),
   // then we return a sentinel: (bbid_t)-1

   // TODO: we can do a bit better: if the outlined blocks of this fn immediately
   // follow the inlined block of the fn, then unfortunately we'll conservatively
   // return a sentinel instead of recognizing that the first-emitted outlined block
   // does indeed immediately follow the last-emitted inlined block.  (The reason that
   // we can't recognize this is that to be safe, we must assume that the outlined
   // blocks of this fn don't necessarily immediately follow the inlined blocks of
   // this function, now that we are in general inlining/outlining a bunch of functions
   // together in a group.)

   unsigned ordering;
   if (blockId2OrderingForInlinedBlocks.find(id, ordering)) {
      assert(ordering < inlinedBlocks.size());
      if (ordering + 1 == inlinedBlocks.size())
         return (bbid_t)-1;
      else
         return inlinedBlocks[ordering+1];

      assert(false);
   }
   else if (blockId2OrderingForOutlinedBlocks.find(id, ordering)) {
      assert(ordering < outlinedBlocks.size());
      if (ordering + 1 == outlinedBlocks.size())
         return (bbid_t)-1;
      else
         return outlinedBlocks[ordering+1];

      assert(false);
   }
   else
      assert(false);
}

bool outliningLocations::
isFuncBeingInlinedAndOutlined(kptr_t destOrigAddress) const {
   return (binary_search(funcsBeingInlinedAndOutlined.begin(),
                         funcsBeingInlinedAndOutlined.end(),
                         destOrigAddress, addrCmp()));
}

bool outliningLocations::
willInterprocBranchStillFit(const pdstring &/*modAndFnName*/,
                            const kptr_t destOrigAddr) const {
   if (justTestingNoChanges)
      return true;

   return (whereOutlinedBlocksReside == afterInlinedBlocks &&
           binary_search(funcsBeingInlinedAndOutlined.begin(),
                         funcsBeingInlinedAndOutlined.end(),
                         destOrigAddr, addrCmp()));
}

bool outliningLocations::
willInterprocBranchStillFit(kptr_t destAddr) const {
   if (justTestingNoChanges)
      return true;
   
   return (whereOutlinedBlocksReside == afterInlinedBlocks &&
           binary_search(funcsBeingInlinedAndOutlined.begin(),
                         funcsBeingInlinedAndOutlined.end(),
                         destAddr, addrCmp()));
}

pdstring outliningLocations::getBlockLabel(bbid_t id) {
   assert(id != (bbid_t)-1 && "getBlockLabel() called on interProc situation?");
   
   pdstring result("bb_id_");
   result += num2string(id);
   result += "_label";
   return result;
}

pdstring outliningLocations::getCodeObjectLabel(bbid_t bbid,
                                              unsigned codeObjectNdxWithinBB) {
   pdstring result("codeObject:bbid");
   result += num2string(bbid);
   result += "codeObjectNdx";
   assert(codeObjectNdxWithinBB < 1000); // rather arbitrary
   result += num2string(codeObjectNdxWithinBB);
   result += ";"; // unnecessary; temp for debugging hard-to-follow gdb string output

   return result;
}

pdstring outliningLocations::
getJumpTableStartOfDataLabel(bbid_t bbid) {
   pdstring result("jumptabledata_for_bb_id_");
   result += num2string(bbid);
   return result;
}

pdstring outliningLocations::
getJumpTableJumpInsnLabel(bbid_t bb_id) {
   pdstring result("jumpTableJumpInsn_for_bb_id_");
   result += num2string(bb_id);
   return result;
}

void outliningLocations::aBlockIsAboutToBeEmitted(tempBufferEmitter &em,
                                                  bbid_t bb_id) const {
   
   em.emitLabel(getBlockLabel(bb_id));
      // Now we can branch to this guy
}

void outliningLocations::
aCodeObjectIsAboutToBeEmitted(tempBufferEmitter &em,
                              bbid_t bbid,
                              unsigned codeObjectNdxWithinBB) const {
   em.emitLabel(getCodeObjectLabel(bbid, codeObjectNdxWithinBB));
}

