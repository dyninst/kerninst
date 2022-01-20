// usparc_icache.C

#include "util/h/usparc_icache.h"
#include "util/h/hashFns.h"

unsigned usparc_icache::alignedVAddrHash(const kptr_t &addr) {
   kptr_t result = addr / nbytesPerICacheBlock;
   return addrHash(result);
}

usparc_icache::usparc_icache() :
   icacheBlock2HowCrowded(numBlocksIn8KICache, 0),
   placedAlignedVAddrs(alignedVAddrHash)
{
}

void usparc_icache::placeBasicBlock(uint64_t vaddr, unsigned nbytes) {
   assert(nbytes < 4096); // somewhat arbitrary
   assert(nbytes % 4 == 0);
   assert(vaddr % 4 == 0);

   while (true) {
      // place one icache block
      const unsigned curr_icacheblock = vaddr2icacheblock_8kdirect(vaddr);
      unsigned offset_within_icacheblock = vaddr2byteOffsetWithinICacheBlock(vaddr);
      unsigned num_effective_bytes_this_icache_block =
         nbytesPerICacheBlock - offset_within_icacheblock;
         // This is how much of the icache block that we can use

      assert(num_effective_bytes_this_icache_block <= nbytesPerICacheBlock);
      assert(num_effective_bytes_this_icache_block % 4 == 0);

      place1UnlessCanBeShared(vaddr, curr_icacheblock);

      if (nbytes <= num_effective_bytes_this_icache_block)
         // all done; this icache block holds the rest of the basic block
         return;
      else {
         vaddr += num_effective_bytes_this_icache_block;
         nbytes -= num_effective_bytes_this_icache_block;

         // From now on, vaddr is going to be aligned at the start of an icache
         // block, right?
         assert(vaddr % nbytesPerICacheBlock == 0);
         assert(nbytes % 4 == 0); // instruction grained, please
      }
   }
}

void usparc_icache::
place1UnlessCanBeShared(kptr_t addr, unsigned curr_icacheblock) {
   // First, round addr down to the nearest icache block boundary:
   const kptr_t origaddr = addr;
   
   addr = (addr / nbytesPerICacheBlock) * nbytesPerICacheBlock;
   assert(addr % nbytesPerICacheBlock == 0);
   assert(addr <= origaddr);
   if (addr < origaddr)
      assert(addr + nbytesPerICacheBlock > origaddr);

   if (placedAlignedVAddrs.defines(addr)) {
      assert(icacheBlock2HowCrowded[curr_icacheblock] > 0);
      return;
   }
   else {
      icacheBlock2HowCrowded[curr_icacheblock]++;
      placedAlignedVAddrs.set(addr, true);
   }
}
