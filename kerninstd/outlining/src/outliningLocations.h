// outliningLocations.h
// Tells us which blocks are inlined/outlined, and their precise ordering
// Useful in two ways: directs the algorithm of emitting outlined/inlined code,
// and allows us to query where particular blocks reside or will reside (needed
// for branches and so forth between blocks).
//
// Also tells us which functions will be inlined & outlined, so you will be able
// to tell whether an interprocedural branch to such a function will be within range
// (provided whereOutlinedBlocksReside==afterInlinedBlocks; if not, no guarantees.)
//
// TODO: We should also keep a copy of branch counts here, so codeObjects can query
// the counts to determine, for example, how best to set branch predict bits in the
// instruction.

#ifndef _OUTLINING_LOCATIONS_H_
#define _OUTLINING_LOCATIONS_H_

#include "util/h/Dictionary.h"
#include "tempBufferEmitter.h"
#include "funkshun.h" // to get its bbid_t; otherwise, a fwd decl would be OK

class outliningLocations {
 public:
   typedef basicblock_t::bbid_t bbid_t;

   enum howFarAway {afterInlinedBlocks, farFarAway};   

 private:
   static unsigned bbidHash(const bbid_t &id) {
      return id;
   }

   // 0) Uniqifying the name of this outlined group:
   const unsigned groupUniqueId;
   
   // 1) General information:
   howFarAway whereOutlinedBlocksReside;
   pdvector< pair<pdstring, const function_t*> > funcsBeingInlinedAndOutlined;
      // NOTE: must be kept sorted (by the function's entryAddr)
      // If whereOutlinedBlocksReside==farFarAway then this tells us nothing.
      // On the other hand, if whereOutlinedBlocksReside==afterInlinedBlocks, then
      // this tells us whether an interprocedural branch to such a function will be
      // within range after relocation.
   bool justTestingNoChanges;
      // if true, then we automatically say that all conditional that used to be within
      // range are still within range.

   // 2) Emit ordering.  Note that we only specify blocks OF A SINGLE FUNCTION.
   // (And curiously, we don't even need to store that particular function_t.)
   // "funcsBeingInlinedAndOutlined[]" is as far as we go toward a multiple-fn
   // flavoring in this class, and that was the exception.  The core of this class,
   // the next two variables, are STRICTLY (and intentionally) block ids of ONE AND
   // THE SAME FUNCTION.  We note that, fortunately, this in no way precludes the
   // ability to outline multiple functions at the same time.
   pdvector<bbid_t> inlinedBlocks;
   pdvector<bbid_t> outlinedBlocks;

   dictionary_hash<bbid_t, unsigned> blockId2OrderingForInlinedBlocks;
      // .value: ordering within inlinedBlocks[]
   dictionary_hash<bbid_t, unsigned> blockId2OrderingForOutlinedBlocks;
      // .value: ordering within outlinedBlocks[]
   
   outliningLocations(const outliningLocations &src);
   outliningLocations& operator=(const outliningLocations &);
   
 public:
   outliningLocations(unsigned groupUniqueId,
                      howFarAway iwhereOutlinedBlocksReside,
                      const pdvector< pair<pdstring, const function_t*> >&iFuncsBeingInlinedAndOutlined,
                      bool ijustTestingNoChanges,
                      const pdvector<bbid_t> &inlinedBlocks,
                      const pdvector<bbid_t> &outlinedBlocks);
  ~outliningLocations() {}

   unsigned getGroupUniqueId() const {
      return groupUniqueId;
   }
   
   howFarAway getWhereOutlinedBlocksReside() const {
      return whereOutlinedBlocksReside;
   }

   bool isBlockInlined(bbid_t id) const {
      return blockId2OrderingForInlinedBlocks.defines(id);
   }
   bool isBlockOutlined(bbid_t id) const {
      return blockId2OrderingForOutlinedBlocks.defines(id);
   }

   bbid_t whoIfAnyoneWillBeEmittedAfter(bbid_t bb_id) const;
      // NOTE: returns (bbid_t)-1 if no block (of the same function) will be emitted
      // after bb_id

   bool isFuncBeingInlinedAndOutlined(kptr_t destOriginalAddress) const;
   bool willInterprocBranchStillFit(const pdstring &modAndFnName, // "modNm/fnName"
                                    kptr_t destOriginalAddress) const;
   bool willInterprocBranchStillFit(kptr_t destAddr) const;

   // --------------------

   static pdstring getBlockLabel(bbid_t);
      // this one may go away soon (but offset32JumpTableCodeObject likes it)
   static pdstring getCodeObjectLabel(bbid_t, unsigned codeObjectNdxWithinBB);

   static pdstring getJumpTableStartOfDataLabel(bbid_t bbContainingJmpInsn);
   static pdstring getJumpTableJumpInsnLabel(bbid_t);
   static pdstring getInterProcBranchDestSymNameFromAddr(kptr_t destAddr);

   void aBlockIsAboutToBeEmitted(tempBufferEmitter &em, bbid_t) const;
   void aCodeObjectIsAboutToBeEmitted(tempBufferEmitter &em,
                                      bbid_t bbid,
                                      unsigned codeObjectNdxWithinBB) const;
};

#endif
