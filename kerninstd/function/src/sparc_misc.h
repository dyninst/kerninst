// sparc_misc.h

#ifndef _SPARC_MISC_H_
#define _SPARC_MISC_H_

#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"
#include "hiddenFns.h"
#include "sparc_reg.h"
#include "sparcTraits.h"

// fwd decls to avoid recursive #includes:
class function_t;
class basicblock;
class simplePathSlice;

typedef simplePathSlice simplePathSlice_t;

extern pdvector<kptr_t> functionsThatReturnForTheCaller;
extern bool verbose_fnParse;
extern bool verbose_hiddenFns;

void readFromKernelInto(pdvector<uint32_t> &result, 
			kptr_t startAddr, kptr_t end);

bool analyzeSliceForRegisterBoundsViaBranches(simplePathSlice_t &parentSlice,
                                              sparc_reg R,
                                              unsigned &min_val,
                                              unsigned &max_val);

#endif
