#ifndef _SPARC_TRAITS_H_
#define _SPARC_TRAITS_H_

// We have a choice here: to either declare sparc_instr & company as
// blank forward declarations, or #include "sparc_instr.h" and others.
// It seems that using only fwd declarations is the way to go to avoid
// recursive #includes. The downside is that files using sparcTraits
// should probably manually #include sparc_instr, sparc_reg.h, and
// company...  Forgetting to do so will lead to confusing compiler
// errors.

//class sparc_instr;
//class sparc_reg;
//class sparc_reg_set;
class sparc_bb;
class sparc_controlFlow;
//class sparc_relocatableCode;

#include <inttypes.h> // uint32_t etc.

template<class kernelAddress> class tagSparcTraits {
public:
   // A kernel address: uint32_t (32-bit) or uint64_t (64-bit)
   typedef kernelAddress     kaddr_t;

   //typedef sparc_instr       instr_type;
   //typedef sparc_reg         reg_type;
   //typedef sparc_reg_set     reg_set_type;
   typedef sparc_bb          arch_basicblock_type;
   typedef sparc_controlFlow arch_cfanalysis_type;
   //typedef sparc_relocatableCode arch_relocatablecode_type;
};

#ifdef _KDRV64_
typedef tagSparcTraits<uint64_t> sparcTraits;
#else
typedef tagSparcTraits<uint32_t> sparcTraits;
#endif

typedef sparcTraits::arch_basicblock_type arch_bb_t;
typedef sparcTraits::arch_cfanalysis_type arch_cfa_t;
typedef sparcTraits::kaddr_t kaddr_t;
//typedef sparcTraits::arch_relocatablecode_type arch_reloc_t;
#endif

