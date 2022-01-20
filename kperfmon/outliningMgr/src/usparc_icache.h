// usparc_icache.h
// Some utility routines for the UltraSPARC I-Cache

#ifndef _USPARC_ICACHE_H_
#define _USPARC_ICACHE_H_

#include "util/h/kdrvtypes.h"
#include "util/h/Dictionary.h"

class usparc_icache {
 public:
   static const unsigned nbytesPerICacheBlock = 32;
   static const unsigned numBlocksIn8KICache = 256;

   static unsigned vaddr2icacheblock_8kdirect(kptr_t addr) {
      // This routine assumes a direct-mapped 8k icache, having 256
      // blocks of 32 bytes each.  Although the UltraSPARC I-Cache
      // actually has 2 such banks (for 2-way set associativity), it's
      // often useful to call this routine first, to verify that
      // there's no need to rely on the 2-way associativity to guarantee
      // no conflict.

      return (addr / nbytesPerICacheBlock) % 256;
         // the "/" and "%" operations should be fast, assuming the
         // compiler can detect constants.
   }
   static unsigned vaddr2icacheblock_16kdirect(kptr_t addr) {
      return (addr / nbytesPerICacheBlock) % 512;
         // the "/" and "%" operations should be fast, assuming the
         // compiler can detect constants.
   }
   static unsigned vaddr2byteOffsetWithinICacheBlock(kptr_t addr) {
      return addr % nbytesPerICacheBlock;
   }

   usparc_icache();

   void placeBasicBlock(uint64_t vaddr, unsigned nbytes);
      // nbytes must be divisible by 4
      // note: don't forget to include annulled delay slots, even for ba,a, because
      // such insns still get fetched out of the icache, I believe.

   pdvector<unsigned> getHowCrowdedStats() const {
      // result: A vector, indexed by icache block number (256 such blocks),
      // indicating how crowded it is (how many basic blocks fall there).
      // Note that a result of up to 2 is OK, because we've got 2-way associativity.
      // Anything more than that, and we've got a conflict.

      // I don't want to return a reference, for safety's sake (dangling pointers
      // and so on).

      // The sum of icacheBlock2HowCrowded[] entries should equal
      // placedAlignedVAddrs.size()
      unsigned total = 0;
      pdvector<unsigned>::const_iterator iter = icacheBlock2HowCrowded.begin();
      pdvector<unsigned>::const_iterator finish = icacheBlock2HowCrowded.end();
      for (; iter != finish; ++iter)
         total += *iter;
      assert(total == placedAlignedVAddrs.size());
      
      return icacheBlock2HowCrowded;
   }

 private:
   // private to ensure not used:
   usparc_icache(const usparc_icache &);
   void operator=(const usparc_icache &);

   static unsigned alignedVAddrHash(const kptr_t &addr);
   void place1UnlessCanBeShared(kptr_t addr, unsigned curr_icacheblock);
   
   pdvector<unsigned> icacheBlock2HowCrowded;
      // index: icache block number (0 to 256...assuming 8K direct-mapped)
      // value: how crowded this block is (how many different basic blocks
      // were placed here).  Due to 2-way set associativity (16K icache total),
      // a value of 2 is perfectly fine, guaranteeing no conflict.  Anything
      // more than that, however, and we've got a conflict.

   dictionary_hash<kptr_t, bool> placedAlignedVAddrs;
      // value is a dummy
      // key: vaddr, rounded down to icache block bounds
      // Used to avoid adding an entry to icacheBlock2HowCrowded if
      // in fact a basic block can share a cache block line with a previously
      // placed basic block.
};

#endif
