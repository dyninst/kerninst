// kmem.C

#include "kmem.h"
#include "funkshun.h"

// needs declaration of function, so goes in .C instead of .h
kmem::kmem(const function_t &ifn) : theCode(ifn.getOrigCode()) {}

instr_t* kmem::getinsn(kptr_t addr) const {
   if (theCode.enclosesAddr(addr)) {
     //since getinsn_general returns a pointer to insn on the heap,
     //we need to return a copy here as well.
     instr_t *i = instr_t::getInstr(*(theCode.get1Insn(addr)));
     return i;
    } else
      return getinsn_general(addr);
}
