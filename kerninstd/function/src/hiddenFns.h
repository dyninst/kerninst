// hiddenFns.h

#ifndef _HIDDEN_FNS_H_
#define _HIDDEN_FNS_H_

#include <inttypes.h> // uint32_t
#include "common/h/String.h"
#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"

struct hiddenFn {
   kptr_t addr;
   pdstring name;

   hiddenFn(kptr_t iaddr, const pdstring &iname) : addr(iaddr), name(iname) {
   }
};

#endif
