// power_kmem.C

#include "powerTraits.h"
#include "power_kmem.h"
#include "power_instr.h"
#include "kernelDriver.h"


instr_t * power_kmem::getinsn_general(kptr_t addr) const {
   extern kernelDriver *global_kernelDriver; // sorry for this global use.
      
   const uint32_t raw = global_kernelDriver->peek1Word(addr);
   power_instr *result = new power_instr(raw);
   return result;
}
