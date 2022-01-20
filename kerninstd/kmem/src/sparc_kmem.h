#ifndef _SPARC_KMEM_H_
#define _SPARC_KMEM_H_

#include "kmem.h"

class sparc_kmem : public kmem {
 public:
   ~sparc_kmem() {}
   sparc_kmem(const function_t &ifn) : kmem(ifn) {}
   instr_t* getinsn_general(kptr_t addr) const;
   unsigned numInsnBytes(kptr_t /*addr*/) const { return 4; }
   unsigned numInsns(kptr_t saddr, kptr_t eaddr) const {
      return (eaddr - saddr) / 4;
   }
};


#endif /* _SPARC_KMEM_H_ */
