// sparc_kmem.C

#include "sparcTraits.h"
#include "sparc_kmem.h"
#include "sparc_instr.h"
#include "kernelDriver.h"

instr_t* sparc_kmem::getinsn_general(kptr_t addr) const {
   extern kernelDriver *global_kernelDriver; // sorry for this global use.
      
   const uint32_t raw = global_kernelDriver->peek1Word(addr);
   instr_t *result = new sparc_instr(raw);
   return result;
}
