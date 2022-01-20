// sparc_insnVec.h

#ifndef _SPARC_INSNVEC_H_
#define _SPARC_INSNVEC_H_

#include "insnVec.h"

class sparc_insnVec : public insnVec_t {
 private:
   void raw_initialize(const void *raw_insn_bytes, unsigned total_num_bytes);
 public:
   sparc_insnVec() : insnVec_t() {}
   sparc_insnVec(const void *raw_insn_bytes, unsigned total_num_bytes);
   sparc_insnVec(XDR *xdr);
   sparc_insnVec(const pdvector<instr_t *> &ithe_insns) : 
      insnVec_t(ithe_insns) {}

   // perhaps we should make the following ctor private, to ensure that it's
   // not used in practice.  After all, it's expensive, and would we ever really
   // need to copy an insnVec?  The same applies to operator=(), only more so.
   sparc_insnVec(const insnVec_t &src) : insnVec_t(src) {}

   ~sparc_insnVec() {}

   unsigned getMemUsage_exceptObjItself() const;

   void reserveBytesHint(unsigned numbytes);
   void shrinkToBytes(unsigned newNumBytes,
		      bool aggressivelyFreeUpMemory);

   unsigned insnNdx2ByteOffset(unsigned ndx, 
			       bool enforce_range=true) const;
   unsigned byteOffset2InsnNdx(unsigned byteoffset) const;
   instr_t * get_by_byteoffset(unsigned byteoffset) const;
   unsigned offsetOfInsnContaining(unsigned byteoffset) const;

   void * reserveForManualOverwriteEnMasse(unsigned total_bytes);
}; 

#endif
