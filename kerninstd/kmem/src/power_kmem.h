#ifndef _POWER_KMEM_H_
#define _POWER_KMEM_H_

#include "kmem.h"

class power_kmem : public kmem {
 public:
   ~power_kmem() {}
   power_kmem(const function_t &ifn) : kmem(ifn) {}
   instr_t * power_kmem::getinsn_general(kptr_t addr) const;
   unsigned numInsnBytes(kptr_t /*addr*/) const {
      return 4;
   }
   unsigned numInsns(kptr_t saddr, kptr_t eaddr) const {
      return (eaddr - saddr) / 4;
   }
};


#endif /* _POWER_KMEM_H_ */
