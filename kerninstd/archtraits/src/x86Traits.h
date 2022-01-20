#ifndef _X86TRAITS_H_
#define _X86TRAITS_H_

class x86_instr;
class x86_reg;
class x86_reg_set;
class x86_bb;
class x86_controlFlow;

#include <inttypes.h> // uint32_t etc.

class x86Traits {
public:
   typedef uint32_t        kaddr_t;

   typedef x86_instr       instr_type;
   typedef x86_reg         reg_type;
   typedef x86_reg_set     reg_set_type;
   typedef x86_bb          arch_basicblock_type;
   typedef x86_controlFlow arch_cfanalysis_type;
};

typedef x86Traits::arch_basicblock_type arch_bb_t;
typedef x86Traits::arch_cfanalysis_type arch_cfa_t;
typedef x86Traits::kaddr_t kaddr_t;

#endif
