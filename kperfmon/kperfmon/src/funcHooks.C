#include <stdio.h>
#include <iostream>
// vtimerMgr.C

#include "util/h/out_streams.h"
#include "kapi.h"

#include "funcHooks.h"

extern kapi_manager kmgr;

funcHooks::funcHooks()
{
}

funcHooks::~funcHooks()
{
    assert(hookPoints.size() == 0);
}

void funcHooks::uninstallHooks()
{
    pdvector<unsigned>::const_iterator iter = hookPoints.begin();
    for (; iter != hookPoints.end(); iter++) {
	kmgr.removeSnippet(*iter);
    }
    hookPoints.clear();
}

// Finds a specified function, initializes kfunc
// Returns 0 on success, -1 if func not found or the name is not unique
int funcHooks::findFunction(const char *moduleName, const char *funcName,
			    kapi_function *pfunc)
{
    kapi_module kmod;
    if (kmgr.findModule(moduleName, &kmod) < 0) {
	cout << "Can not find module \"" << moduleName << "\"\n";
	return (-1);
    }
    
    if (kmod.findFunction(funcName, pfunc) < 0) {
	cout << "Can not find function \"" << funcName 
	     << "\" or the function is not unique\n";
	return (-1);
    }
    return 0;
}

// Patch the entry point of funcFrom with a call to funcTo
int funcHooks::hookAtEntry(const char *moduleFrom, const char *funcFrom, 
			   const char *moduleTo, const char *funcTo,
			   unsigned id)
{
    cout << "Inserting \"call " << funcTo << "\" at the entry point of "
	 << moduleFrom << "/" << funcFrom << endl;

    kapi_function kfuncFrom;
    if (findFunction(moduleFrom, funcFrom, &kfuncFrom) < 0) {
	return (-1);
    }
    
    kapi_vector<kapi_point> points;
    if (kfuncFrom.findEntryPoint(&points) < 0) {
	assert(false && "Internal error: entry point not found");
    }
    assert(points.size() == 1);

    kapi_vector<kapi_snippet> args;
    args.push_back(kapi_const_expr(id)); // the first arg of funcTo is "id"
    for (unsigned i=0; i<5; i++) {
	// the next five args of funcTo are the arguments of funcFrom
	args.push_back(kapi_param_expr(i));
    }

    kapi_function kfuncTo;
    if (findFunction(moduleTo, funcTo, &kfuncTo) < 0) {
	return (-1);
    }
    kapi_call_expr code(kfuncTo.getEntryAddr(), args);

    int reqid;
    if ((reqid = kmgr.insertSnippet(code, points[0])) < 0) {
	assert(false && "Internal error: insertSnippet failed");
    }
    hookPoints.push_back(reqid);

    return 0;
}

// Patch the exit point of funcFrom with a call to funcTo
int funcHooks::hookAtExit(const char *moduleFrom, const char *funcFrom, 
			  const char *moduleTo, const char *funcTo,
			  unsigned id)
{
    cout << "Inserting \"call " << funcTo << "\" at the exit point of "
	 << moduleFrom << "/" << funcFrom << endl;

    kapi_function kfuncFrom;
    if (findFunction(moduleFrom, funcFrom, &kfuncFrom) < 0) {
	return (-1);
    }
    
    kapi_vector<kapi_point> points;
    if (kfuncFrom.findExitPoints(&points) < 0) {
	assert(false && "Internal error: entry point not found");
    }

    kapi_vector<kapi_snippet> args;
    args.push_back(kapi_const_expr(id)); // the first arg of funcTo is "id"
    args.push_back(kapi_retval_expr(kfuncFrom));  // retval of funcFrom

    kapi_function kfuncTo;
    if (findFunction(moduleTo, funcTo, &kfuncTo) < 0) {
	return (-1);
    }
    kapi_call_expr code(kfuncTo.getEntryAddr(), args);

    kapi_vector<kapi_point>::const_iterator ptiter = points.begin();
    for (; ptiter != points.end(); ptiter++) {
	int reqid;
	if ((reqid = kmgr.insertSnippet(code, *ptiter)) < 0) {
	    assert(false && "Internal error: insertSnippet failed");
	}
	hookPoints.push_back(reqid);
    }

    return 0;
}

// Read the file and install the hooks that it contains
void funcHooks::processHooksFromFile(const char *fname)
{
    const unsigned int MAXLEN = 255;
    unsigned id;
    char modFrom[MAXLEN];
    char funcFrom[MAXLEN];
    char modTo[MAXLEN];
    char funcTo[MAXLEN];
    char locationTo[MAXLEN];

    FILE *f;

    if ((f = fopen(fname, "rt")) == 0) {
        perror("funcHooks::processHooksFromFile: fopen failed");
        assert(false);
    }
    while (!feof(f)) {
        if (fscanf(f, 
		   " %u %[^ \t\n] %[^ \t\n] %[^ \t\n] %[^ \t\n] %[^ \t\n]\n", 
                   &id, modFrom, funcFrom, modTo, funcTo, locationTo) != 6) {
            cout << "hook file format error, expected: \"id modFrom funcFrom "
                    "modTo funcTo entry|exit\"\n";
            assert(false);
        }
        if (!strcmp(locationTo, "entry") ||
            !strcmp(locationTo, "ENTRY")) {
            hookAtEntry(modFrom, funcFrom, modTo, funcTo, id);
        }
        else if (!strcmp(locationTo, "exit") ||
                 !strcmp(locationTo, "EXIT")) {
            hookAtExit(modFrom, funcFrom, modTo, funcTo, id);
        }
        else {
           cout << "hook file format error (location  = \"" << locationTo
                << "\"), expected ENTRY or EXIT\n";
           assert(false);
        }
    }
    fclose(f);
}
