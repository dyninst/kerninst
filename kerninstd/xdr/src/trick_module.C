// trick_module.C
// kerninstd's

#include "trick_module.h"
#include "loadedModule.h" // kerninstd's loadedModule

trick_module::trick_module(const loadedModule *imod) : mod(imod) {}

trick_module::~trick_module() {}

const loadedModule* trick_module::get() const {
   return mod;
}

