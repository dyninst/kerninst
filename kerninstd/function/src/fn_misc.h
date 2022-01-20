// fn_misc.h

#ifndef _FN_MISC_H_
#define _FN_MISC_H_

#include <inttypes.h>
#include "util/h/StringPool.h"
#include "common/h/String.h"

const StringPool &
findModuleAndReturnStringPoolReference();

const char *
findModuleAndAddToRuntimeStringPool(const pdstring &fnName);
   // uses "presentlyReceivingModule" as the implicit module

const char *
findModuleAndAddToRuntimeStringPool(const pdstring &modName, // must already exist
                                    const pdstring &fnName);

#endif
