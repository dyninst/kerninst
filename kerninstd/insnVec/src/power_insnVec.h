// power_insnVec.h

#ifndef _POWER_INSNVEC_H_
#define _POWER_INSNVEC_H_

#include "insnVec.h"

class power_insnVec : public insnVec_t {
 private:
   void raw_initialize(const void *raw_insn_bytes, unsigned total_num_bytes);
 public:
   power_insnVec() : insnVec_t() {}
   power_insnVec(const void *raw_insn_bytes, unsigned total_num_bytes);
   power_insnVec(XDR *xdr);
   power_insnVec(const pdvector<instr_t *> &ithe_insns) : 
      insnVec_t(the_insns) {}

   // perhaps we should make the following ctor private, to ensure that it's
   // not used in practice.  After all, it's expensive, and would we ever really
   // need to copy an insnVec?  The same applies to operator=(), only more so.
   power_insnVec(const insnVec_t &src) : insnVec_t(src) {}

   ~power_insnVec() {}

   unsigned getMemUsage_exceptObjItself() const;

   void reserveBytesHint(unsigned numbytes);
   void shrinkToBytes(unsigned newNumBytes,
		      bool aggressivelyFreeUpMemory);

   unsigned insnNdx2ByteOffset(unsigned ndx, bool enforce_range=true) const;
   unsigned byteOffset2InsnNdx(unsigned byteoffset) const;
   instr_t * get_by_byteoffset(unsigned byteoffset) const;
   unsigned offsetOfInsnContaining(unsigned byteoffset) const;

   void * reserveForManualOverwriteEnMasse(unsigned total_bytes);
}; 

#endif
