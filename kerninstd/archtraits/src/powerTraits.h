#ifndef _POWERTRAITS_H_
#define _POWERTRAITS_H_

#include <inttypes.h>

class power_instr;
class power_reg;
class power_reg_set;
class power_bb;
class power_controlFlow;

template<class kernelAddress> class tagPowerTraits {
public:
     typedef kernelAddress     kaddr_t;

     typedef power_instr       instr_type;
     typedef power_reg         reg_type;
     typedef power_reg_set     reg_set_type;
     typedef power_bb  arch_basicblock_type;
     typedef power_controlFlow arch_cfanalysis_type;
};

#ifdef _KDRV64_ 
typedef tagPowerTraits<uint64_t> powerTraits;
#else
typedef tagPowerTraits<uint32_t> powerTraits;
#endif

typedef powerTraits::arch_basicblock_type arch_bb_t;
typedef powerTraits::arch_cfanalysis_type arch_cfa_t;
typedef powerTraits::kaddr_t kaddr_t;

#endif
