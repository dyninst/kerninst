// funcsToIncludeInGroupOracle.h

#ifndef _FUNCS_TO_INCLUDE_IN_GROUP_ORACLE_H_
#define _FUNCS_TO_INCLUDE_IN_GROUP_ORACLE_H_

#include "outlinedGroup.h"

class funcsToIncludeInGroupOracle {
 private:
   typedef function_t::bbid_t bbid_t;
   
   const outlinedGroup &theOutlinedGroup;

   // Sorry, can't cache a block oracle, because each block oracle needs to be
   // different for each function.

   // But we can cache this:
   const fnBlockAndEdgeCounts &rootFnBlockAndEdgeCounts;

 public:
   funcsToIncludeInGroupOracle(const outlinedGroup &ioutlinedGroup,
                               const fnBlockAndEdgeCounts &rootFnBlockAndEdgeCounts);

   bool worthQueryingCodeObjectsForFn(const pdstring &modName,
                                      const function_t &fn) const;
   bool willFnBeInOptimizedGroup(const pdstring &modName, const function_t &fn) const;
   pdvector<bbid_t> getInlinedBlocksOfGroupFn(const pdstring &modName,
                                            const function_t &fn) const;
      // asserts that fn is in the group; don't call if you're not sure that
      // willFnBeInOptimizedGroup() has returned false.
};

#endif
