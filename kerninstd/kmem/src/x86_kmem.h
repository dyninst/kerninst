#ifndef _X86_KMEM_H_
#define _X86_KMEM_H_

#include <inttypes.h> // uint32_t
#include "x86_instr.h"
#include "kmem.h"

class x86_kmem : public kmem {
 public:
   ~x86_kmem() {}
   x86_kmem(const function_t &ifn) : kmem(ifn) {}
   instr_t* getinsn_general(kptr_t addr) const;
   uint32_t getdbwd(kptr_t addr) const;
   unsigned numInsnBytes(kptr_t addr) const {
      return getinsn(addr)->getNumBytes();
   }
   unsigned numInsns(kptr_t saddr, kptr_t eddr) const;
};

#endif /* _X86_KMEM_H_ */
