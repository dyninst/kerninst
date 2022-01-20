#include "parseOracle.h"

unsigned parseOracle::
getMaxJumpTableNumBytesBasedOnOtherJumpTableStartAddrs(kptr_t jumpTableStartAddr) const {
   pdvector<kptr_t>::const_iterator iter = allJumpTableStartAddrs.begin();
   pdvector<kptr_t>::const_iterator finish = allJumpTableStartAddrs.end();
   for (; iter != finish; ++iter) {
      const kptr_t otherStartAddr = *iter;
      if (otherStartAddr == jumpTableStartAddr)
         // this could happen and is no big deal, since we just added
         // jumpTableStartAddr prior to calling this routine, in all likelihood.
         ;
      else if (otherStartAddr > jumpTableStartAddr)
         return otherStartAddr - jumpTableStartAddr;
   }

   return UINT_MAX; // no limit
}
