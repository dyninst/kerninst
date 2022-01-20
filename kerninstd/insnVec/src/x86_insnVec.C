// x86_insnVec.C - implementation of arch-specific insnVec members for X86

#include "x86_insnVec.h"
#include "x86_instr.h"
#include "xdr_send_recv_common.h"
#include "pdutil/h/xdr_send_recv.h"

x86_insnVec::x86_insnVec(const void *raw_insn_bytes, 
			 unsigned total_num_bytes) : insnVec_t() { 
   raw_initialize(raw_insn_bytes, total_num_bytes);
}

unsigned x86_insnVec::getMemUsage_exceptObjItself() const {
   return sizeof(x86_insnVec) +
          (the_insns.capacity() * sizeof(instr_t*)) + 
          (the_insns.size() * sizeof(x86_instr));
}

void x86_insnVec::reserveBytesHint(unsigned numbytes) {
   // The caller is telling us that they plan to emit this many bytes, 
   // so we can take this opportunity to perhaps resize some invisible 
   // (to the caller) internal data structures to optimize performance 
   // for this many bytes.
   // Or, we can do nothing.  In particular, this routine should not 
   // actually change the size of the insnVec (as returned by 
   // numInsnBytes()); this is important to remember!

   // Since numbytes tells us nothing about the number of x86 
   // instructions, we make a conservative guess that the average x86 
   // instruction is two bytes
   
   the_insns.reserve_exact(numbytes / 2);
}

unsigned x86_insnVec::insnNdx2ByteOffset(unsigned ndx, 
					 bool enforce_range=true) const {
   if (enforce_range)
      assert( (ndx == 0 && numInsns() == 0) || ndx < numInsns() );

   unsigned offset = 0;
   unsigned counter = 0;

   pdvector<instr_t *>::const_iterator iter = the_insns.begin();
   pdvector<instr_t *>::const_iterator finish = the_insns.end();
   for (; (counter != ndx) && (iter != finish); ++iter, ++counter) {
      const instr_t *i = *iter;
      offset += i->getNumBytes();
   }
   assert(counter == ndx);
   return offset;
}

unsigned x86_insnVec::byteOffset2InsnNdx(unsigned byteoffset) const {
   assert(byteoffset <= num_bytes);
   // yes, it's permitted for "byteoffset" to be 1 byte past the highest
   // insn byte offset ("finish", STL style), in which case we'll return
   // 1 higher than the maximum insn ndx.  This is quite useful.

   unsigned ndx = 0;
   unsigned offset = 0;
   pdvector<instr_t *>::const_iterator iter = the_insns.begin();
   pdvector<instr_t *>::const_iterator finish = the_insns.end();
   for (; (offset < byteoffset) && (iter != finish); ++iter, ++ndx) {
      const instr_t *i = *iter;
      offset += i->getNumBytes();
   }
   if(offset != byteoffset) {
      cerr << "x86_insnVec::byteOffset2InsnNdx: offset=" << offset 
           << " but requested byteoffset=" << byteoffset << endl;
      cerr << "         : Here's the code:\n";
      iter = the_insns.begin();
      offset = ndx = 0;
      for (; (offset < byteoffset) && (iter != finish); ++iter, ++ndx) {
         const instr_t *i = *iter;
         offset += i->getNumBytes();
         int numbytes = i->getNumBytes();
         const char *bytes = ((const x86_instr *)i)->getRawBytes();
         for(int j=0; j<numbytes; j++)
            fprintf(stderr, "%02x ", (unsigned char)bytes[j]);      
         cerr << "(" << numbytes << ")";
         cerr << " ";
      }
      cerr << endl;
   }
   assert(offset == byteoffset); // byteoffset is at beginning of an instr
   return ndx;
}

instr_t *x86_insnVec::get_by_byteoffset(unsigned byteoffset) const {
   return the_insns[byteOffset2InsnNdx(byteoffset)];
}

unsigned x86_insnVec::offsetOfInsnContaining(unsigned byteoffset) const {
   assert(byteoffset <= num_bytes);

   unsigned offset = 0;
   unsigned found = 0;
   pdvector<instr_t *>::const_iterator iter = the_insns.begin();
   pdvector<instr_t *>::const_iterator finish = the_insns.end();
   for (; (!found) && (iter != finish); ++iter) {
      const instr_t *i = *iter;
      if( byteoffset < offset + i->getNumBytes() )
	found = 1;
   }
   return offset;
}

void x86_insnVec::raw_initialize(const void *raw_insn_bytes,
				 unsigned total_num_bytes) {
   const char *raw_insn = (const char*)raw_insn_bytes;
   assert(num_bytes == 0);
   while(num_bytes < total_num_bytes) {
      instr_t *i = new x86_instr(raw_insn);
      unsigned insn_bytes = i->getNumBytes();
      num_bytes += insn_bytes;
      raw_insn += insn_bytes;
      if(num_bytes <= total_num_bytes)
         // For the case when the raw insn bytes are a result of a 
         // peek_kernel_contig() and the true total num bytes is less
         // than total_num_bytes (a multiple of 4), we don't want to add 
         // the last insn that puts us over
         the_insns += i;
   }
   if((num_bytes != total_num_bytes) && (total_num_bytes % 4 != 0))
      cerr << "WARNING: x86_insnVec::raw_initialize()- num_bytes="
           << num_bytes << " but expecting equal to total_num_bytes="
           << total_num_bytes << endl;
}

x86_insnVec::x86_insnVec(XDR *xdr) {
   // Default ctor called on the_insns[].  This is exactly what we want, and need
   // (reserve_for_inplace_construction() isn't permitted until the default ctor
   // has been called.)

   // Receive the total number of insns
   unsigned num_insns;
   if (!P_xdr_recv(xdr, num_insns))
      throw xdr_recv_fail();

   // Now receive each individual instruction, one at a time

   if (num_insns > 0) {
      // don't call reserve_for_inplace_construction() with nelems of 0
      pdvector<instr_t *>::iterator iter = the_insns.reserve_for_inplace_construction(num_insns);
      pdvector<instr_t *>::iterator finish = iter + num_insns;
      assert(finish == the_insns.end());

      for (; iter != finish; ++iter) {
	 *iter = (instr_t*) new x86_instr(x86_instr::nop);
         ((x86_instr*)*iter)->~x86_instr();
	 if(!P_xdr_recv(xdr, **iter))
	    throw xdr_recv_fail();
	 num_bytes += (*iter)->getNumBytes();
      }
   }
}

void x86_insnVec::shrinkToBytes(unsigned newNumBytes,
				bool aggressivelyFreeUpMemory) {
   if (newNumBytes < num_bytes) {
      unsigned newNumInsns = byteOffset2InsnNdx(newNumBytes);
      if(newNumInsns < the_insns.size()) {
         pdvector<instr_t*>::iterator iter = the_insns.getIter(newNumInsns);
         pdvector<instr_t*>::iterator fin = the_insns.end();
         for(; iter != fin; iter++)
            delete *iter;
         the_insns.shrink(newNumInsns);      
      }
      if (aggressivelyFreeUpMemory) {
         pdvector<instr_t *> new_insns(the_insns); // smaller in memory usage
         the_insns.swap(new_insns); // very fast
      }
   }
   num_bytes = newNumBytes;
}

void *x86_insnVec::reserveForManualOverwriteEnMasse(unsigned total_bytes) {
   the_insns.zap();
   assert(0 && "x86_insnVec::reserveForManualOverwriteEnMasse nyi");
   void *result = the_insns.reserve_for_inplace_construction(total_bytes / 4);
      // sets the_insns.size() but leaves contents as allocated-but-raw bytes.
   return result;
}
