// blockOrderingOrigSeq.C

#include "blockOrderingOrigSeq.h"

typedef function_t::bbid_t bbid_t;

void 
pickBlockOrderingOrigSeq(const hotBlockCalcOracle &theBlockOracle,
                         const function_t &fn,
                         pdvector<bbid_t> &theInlinedBlockIds,
                         pdvector<bbid_t> &theOutlinedBlockIds) {
   theInlinedBlockIds.clear();
   theOutlinedBlockIds.clear();
   
   // NOTE: This routine isn't as simple as you would think: we want to try hard to
   // categorize basic blocks in the *same order* that they exist in the original
   // code.  We use getAllBasicBlockStartAddressesSorted() to help us with this.
   const pdvector<kptr_t> allBBAddrsSorted = fn.getAllBasicBlockStartAddressesSorted();
   pdvector<kptr_t>::const_iterator iter = allBBAddrsSorted.begin();
   pdvector<kptr_t>::const_iterator finish = allBBAddrsSorted.end();
   for (; iter != finish; ++iter) {
      const kptr_t bbStartAddr = *iter;
      
      // now get a bb_id to go along with this bbStartAddr:
      const bbid_t bb_id = fn.searchForBB(bbStartAddr);
      const basicblock_t *bb = fn.getBasicBlockFromId(bb_id);
      assert(bb->getStartAddr() == bbStartAddr &&
             "fn.getAllBasicBlockStartAddressesSorted() returned an item that doesn't start off a basic block?");

      // Is this block outlined or inlined?
      if (theBlockOracle.isBlockInlined(bb_id))
         theInlinedBlockIds += bb_id;
      else
         theOutlinedBlockIds += bb_id;
   }
}

