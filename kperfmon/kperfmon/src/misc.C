// misc.C

#include "common/h/String.h"
#include "kapi.h"

// The following is presently used by kpmChildrenOracle:
unsigned nextUnusedUniqueWhereAxisId = 2;
   // 0 is taken by 'whole prog' a.k.a. 'root'
   // 1 is taken by '/code'

pdstring module2ItsWhereAxisString(const kapi_module &mod) 
{
    const unsigned max_len = 255;
    char buffer[max_len];

    if (mod.getName(buffer, max_len) < 0) {
	assert(false && "Module's name is too long");
    }
   
    pdstring rv = pdstring(buffer);
    if (mod.getDescription(buffer, max_len) < 0) {
	assert(false && "Module's description is too long");
    }
    if (strlen(buffer) > 0) {
	rv += " (" + pdstring(buffer) + ")";
    }
    return rv;
}

pdstring fn2ItsWhereAxisString(const kapi_function &func)
{
    const unsigned max_len = 255;
    char buffer[max_len];

    // for now at least, no address
    if (func.getName(buffer, max_len) < 0) {
        assert(false && "Functions's name is too long");
    }
    pdstring rv = pdstring(buffer);
    return rv;
}
