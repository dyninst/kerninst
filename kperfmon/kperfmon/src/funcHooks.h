#ifndef _FUNC_HOOKS_H_
#define _FUNC_HOOKS_H_

#include "kapi.h"
#include "common/h/Vector.h"

class funcHooks {
    pdvector<unsigned> hookPoints;
    // Finds a specified function, initializes kfunc
    // Returns 0 on success, -1 if func not found or the name is not unique
    int findFunction(const char *moduleFrom, const char *funcFrom,
		     kapi_function *pfunc);
 public:
    funcHooks();
    ~funcHooks();
    // Patch the entry point of funcFrom with a call to funcTo
    int hookAtEntry(const char *moduleFrom, const char *funcFrom, 
		    const char *moduleTo, const char *funcTo, unsigned id);
    // Patch the exit point of funcFrom with a call to funcTo
    int hookAtExit(const char *moduleFrom, const char *funcFrom, 
		   const char *moduleTo, const char *funcTo, unsigned id);
    // Read the file and install the hooks that it contains
    void processHooksFromFile(const char *fname);
    void uninstallHooks();
};

#endif
