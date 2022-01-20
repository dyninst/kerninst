#ifndef _X86_INSNVEC_H
#define _X86_INSNVEC_H

#include "insnVec.h"

class x86_insnVec : public insnVec_t {
 private:
   void raw_initialize(const void *raw_insn_bytes, unsigned total_num_bytes);
 public:
   x86_insnVec() : insnVec_t() {}
   x86_insnVec(const void *raw_insn_bytes, unsigned total_num_bytes);
   x86_insnVec(XDR *xdr);
   x86_insnVec(const pdvector<instr_t *> &ithe_insns) : 
      insnVec_t(ithe_insns) {}

   // perhaps we should make the following ctor private, to ensure that it's
   // not used in practice.  After all, it's expensive, and would we ever really
   // need to copy an insnVec?  The same applies to operator=(), only more so.
   x86_insnVec(const insnVec_t &src) : insnVec_t(src) {}

   ~x86_insnVec() {}

   unsigned getMemUsage_exceptObjItself() const;

   void reserveBytesHint(unsigned numbytes);
   void shrinkToBytes(unsigned newNumBytes,
		      bool aggressivelyFreeUpMemory);

   unsigned insnNdx2ByteOffset(unsigned ndx, bool enforce_range) const;
   unsigned byteOffset2InsnNdx(unsigned byteoffset) const;
   instr_t * get_by_byteoffset(unsigned byteoffset) const;
   unsigned offsetOfInsnContaining(unsigned byteoffset) const;

   void * reserveForManualOverwriteEnMasse(unsigned total_bytes);
}; 

#endif
