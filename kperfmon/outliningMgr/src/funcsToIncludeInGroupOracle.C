// funcsToIncludeInGroupOracle.C

#include "funcsToIncludeInGroupOracle.h"
#include "hotBlockCalcOracle.h"
#include "outliningMisc.h"

funcsToIncludeInGroupOracle::
funcsToIncludeInGroupOracle(const outlinedGroup &ioutlinedGroup,
                            const fnBlockAndEdgeCounts &itheRootFnBlockAndEdgeCounts) :
      theOutlinedGroup(ioutlinedGroup),
      rootFnBlockAndEdgeCounts(itheRootFnBlockAndEdgeCounts)
{
}

bool funcsToIncludeInGroupOracle::
worthQueryingCodeObjectsForFn(const pdstring &modName, const function_t &fn) const {
   // not quite the same as willFnBeInOptimizedGroup(), which adds one
   // more check: whether the code object was parsed OK for that fn.
   // Here, we can't make that check because, in practice, this method is
   // called before code objects have been parsed.
      
   // The root function is always worth querying:
   if (&fn == &theOutlinedGroup.getRootFn())
      return true;

   // Anything in the group's forceIncludeDescendants[] is worth querying:
   // (Note that forceIncludeDescendants[] contain function orig addrs, and we
   // must compensate for that if we're outlining an already-outlined fn):
   const pdvector<kptr_t> &forceIncludeDescendants =
      theOutlinedGroup.getForceIncludeDescendants();
   if (std::binary_search(forceIncludeDescendants.begin(),
                          forceIncludeDescendants.end(),
                          outliningMisc::getOutlinedFnOriginalFn(modName, fn)))
      return true;

   // Other functions are included as long as at least one of its basic blocks
   // is hot, relative to the number of executions of the entry function.
   // (5% or more)
   const hotBlockCalcOracle theBlockOracle(rootFnBlockAndEdgeCounts, fn,
                                           theOutlinedGroup.get1FnBlockCounts(fn.getEntryAddr()),
                                           theOutlinedGroup.getHotBlockThresholdChoice());
      // XXX: Not terribly fast to initialize, as fn's edge counts get calculated
      // (and they won't be used in this case, so perhaps a ctor flag for whether
      // or not to calculate edge counts is called for)

   const unsigned numBBs = fn.numBBs();
   for (bbid_t bbid = 0; bbid < numBBs; ++bbid)
      if (theBlockOracle.isBlockTrulyHot(bbid)) // YES, we must check
         // for TRULY hot block, or else we'd always return true because
         // the root basic block is always fudged to be considered "hot"!
         return true;

   return false;
}

bool funcsToIncludeInGroupOracle::
willFnBeInOptimizedGroup(const pdstring &modName, const function_t &fn) const {
   if (theOutlinedGroup.get1FnCodeObjects(fn.getEntryAddr()).isUnparsed()) {
      // Failed to properly parse code objects of a function --> don't include
      // such a function in the outlined group.  So we'll return false.
         
      // If this is the entry function, then choke.
      if (&fn == &theOutlinedGroup.getRootFn())
         assert(false && "entry function's code objects are unParsed()");
         
      return false;
   }

   return worthQueryingCodeObjectsForFn(modName, fn);
}

pdvector<funcsToIncludeInGroupOracle::bbid_t>
funcsToIncludeInGroupOracle::
getInlinedBlocksOfGroupFn(const pdstring &modName, const function_t &fn) const {
   assert(willFnBeInOptimizedGroup(modName, fn));

   pdvector<bbid_t> result;

   // A basic block is inlined if it's truly hot (relative to # of executions
   // of the group's root function), or if it's the entry block to function.

   const hotBlockCalcOracle theBlockOracle(rootFnBlockAndEdgeCounts, fn,
                                           theOutlinedGroup.get1FnBlockCounts(fn.getEntryAddr()),
                                           theOutlinedGroup.getHotBlockThresholdChoice());
      // XXX: Not terribly fast to initialize, as fn's edge counts get calculated
      // (and they won't be used in this case, so perhaps a ctor flag for whether
      // or not to calculate edge counts is called for)

   const unsigned numBBs = fn.numBBs();
   for (bbid_t bbid = 0; bbid < numBBs; ++bbid)
      if (theBlockOracle.isBlockInlined(bbid))
         result += bbid;

   return result;
}
