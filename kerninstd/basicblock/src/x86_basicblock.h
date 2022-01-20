#ifndef _X86_BASICBLOCK_H_
#define _X86_BASICBLOCK_H_

#include "basicblock.h"

class x86_basicblock : public basicblock_t {
 public:
  x86_basicblock(const basicblock_t &src) : basicblock_t(src) {}
  x86_basicblock(kptr_t bbStartAddr, bbid_t parent_bb_id) :
    basicblock_t(bbStartAddr, parent_bb_id) {}
  x86_basicblock(kptr_t bbStartAddr, kptr_t bbLastInsnAddr,
		 uint16_t bbNumInsnBytes, const fnCode_t &code) : 
    basicblock_t(bbStartAddr, bbLastInsnAddr, bbNumInsnBytes, code) {}
  x86_basicblock(basicblock_t::Split s, bbid_t parent_bb_id, 
		 basicblock_t *parent, kptr_t splitAddr, 
		 const fnCode_t &code) : 
    basicblock_t(s, parent_bb_id, parent, splitAddr, code) {}
  x86_basicblock(XDR *xdr) : basicblock_t(xdr) {}
  ~x86_basicblock() {} // nothing needs destructed
  kptr_t getExitPoint(const fnCode_t &fnCode) const;
};

#endif /* X86_BASICBLOCK_H_ */
