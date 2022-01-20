// sparc_basicblock.h

#include "basicblock.h"

class sparc_basicblock : public basicblock_t {
 public:
  sparc_basicblock(const basicblock_t &src) : basicblock_t(src) {}
  sparc_basicblock(kptr_t bbStartAddr, bbid_t parent_bb_id) :
    basicblock_t(bbStartAddr, parent_bb_id) {}
  sparc_basicblock(kptr_t bbStartAddr, kptr_t bbLastInsnAddr, 
		   uint16_t bbNumInsnBytes, const fnCode_t &code) : 
    basicblock_t(bbStartAddr, bbLastInsnAddr, bbNumInsnBytes, code) {}
  sparc_basicblock(basicblock_t::Split s, bbid_t parent_bb_id, 
		   basicblock_t *parent, kptr_t splitAddr, 
		   const fnCode_t &code) : 
    basicblock_t(s, parent_bb_id, parent, splitAddr, code) {}
  sparc_basicblock(XDR *xdr) : basicblock_t(xdr) {}
  ~sparc_basicblock() {} // nothing needs destructed
  kptr_t getExitPoint(const fnCode_t &fnCode) const;
};
