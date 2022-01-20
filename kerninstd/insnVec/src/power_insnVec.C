// power_insnVec.C - implementation of arch-specific insnVec members for POWER

//#include "util/h/xdr_send_recv.h"
#include "power_insnVec.h"
#include "power_instr.h"

power_insnVec::power_insnVec(const void *raw_insn_bytes, 
			     unsigned total_num_bytes) : insnVec_t() { 
   raw_initialize(raw_insn_bytes, total_num_bytes);
}

unsigned power_insnVec::getMemUsage_exceptObjItself() const {
   return sizeof(power_insnVec) +
          (the_insns.capacity() * sizeof(instr_t*)) + 
          (the_insns.size() * sizeof(power_instr));
}

void power_insnVec::reserveBytesHint(unsigned numbytes) {
   // The caller is telling us that they plan to emit this many bytes, so we 
   // can take this opportunity to perhaps resize some invisible (to the 
   // caller) internal data structures to optimize performance for this many 
   // bytes.
   // Or, we can do nothing.  In particular, this routine should not actually
   // change the size of the insnVec; this is important to remember!

   the_insns.reserve_exact(numbytes / 4);
}

unsigned power_insnVec::insnNdx2ByteOffset(unsigned ndx, 
					   bool enforce_range) const {
   if (enforce_range)
      assert( (ndx == 0 && numInsns() == 0) || ndx < numInsns() );
   return ndx * 4;
}

unsigned power_insnVec::byteOffset2InsnNdx(unsigned byteoffset) const {
   assert(byteoffset <= numInsnBytes());
   // yes, it's permitted for "byteoffset" to be 1 byte past the highest
   // insn byte offset ("finish", STL style), in which case we'll return
   // 1 higher than the maximum insn ndx.  This is quite useful.

   assert(byteoffset % 4 == 0);
   return byteoffset / 4;
}

instr_t *power_insnVec::get_by_byteoffset(unsigned byteoffset) const {
   assert(byteoffset % 4 == 0);
   return the_insns[byteoffset / 4];
}

unsigned power_insnVec::offsetOfInsnContaining(unsigned byteoffset) const {
   return byteoffset & ~3;
}

void power_insnVec::raw_initialize(const void *raw_insn_bytes,
				   unsigned total_num_bytes) {
   assert(total_num_bytes % 4 == 0);
   const unsigned num_insns = total_num_bytes / 4;
   
   pdvector<instr_t *>::iterator iter = the_insns.reserve_for_inplace_construction(num_insns);
   pdvector<instr_t *>::iterator finish = iter + num_insns;
   assert(finish == the_insns.end());

   assert(instr_t::hasUniformInstructionLength());
   const char *raw_insn_ptr = (const char *)raw_insn_bytes;
   for (; iter != finish; ++iter) {
     const uint32_t raw = *(const uint32_t*)raw_insn_ptr; // get raw insn
      raw_insn_ptr += 4; // go to next raw insn
      *iter = new power_instr(raw); 
         // create new instr from raw, store ptr in the_insns at iter
   }

   num_bytes = total_num_bytes;
   assert((4 * the_insns.size()) == total_num_bytes);
}

power_insnVec::power_insnVec(XDR *xdr) {
   // Default ctor called on the_insns[].  This is exactly what we want, and need
   // (reserve_for_inplace_construction() isn't permitted until the default ctor
   // has been called.)

   // Recieve the total number of insns
   unsigned num_insns;
   if (!P_xdr_recv(xdr, num_insns))
      throw xdr_recv_fail();

   num_bytes = num_insns * 4;
   
   // Now receive each individual instruction, one at a time.

   if (num_insns > 0) {
      // don't call reserve_for_inplace_construction() with nelems of 0
      pdvector<instr_t *>::iterator iter = the_insns.reserve_for_inplace_construction(num_insns);
      pdvector<instr_t *>::iterator finish = iter + num_insns;
      assert(finish == the_insns.end());

      for (; iter != finish; ++iter) {
	 *iter = (instr_t*) malloc(sizeof(power_instr));
	 if(!P_xdr_recv(xdr, **iter))
	    throw xdr_recv_fail();
      }
   }
}


void power_insnVec::shrinkToBytes(unsigned newNumBytes,
				  bool aggressivelyFreeUpMemory) {
   assert(newNumBytes % 4 == 0);
   const unsigned newNumInsns = newNumBytes / 4;
      
   if (newNumInsns < the_insns.size()) {
      pdvector<instr_t*>::iterator iter = the_insns.getIter(newNumInsns);
      pdvector<instr_t*>::iterator fin = the_insns.end();
      for(; iter != fin; iter++)
         delete *iter;
      the_insns.shrink(newNumInsns);
      if (aggressivelyFreeUpMemory) {
         pdvector<instr_t *> new_insns(the_insns); // smaller in memory usage
         the_insns.swap(new_insns); // very fast
      }
   }
   num_bytes = newNumBytes;
}

void *power_insnVec::reserveForManualOverwriteEnMasse(unsigned total_bytes) {
   the_insns.zap();
   num_bytes = total_bytes;
   void *result = the_insns.reserve_for_inplace_construction(total_bytes / 4);
      // sets the_insns.size() but leaves contents as allocated-but-raw bytes.
   return result;
}
