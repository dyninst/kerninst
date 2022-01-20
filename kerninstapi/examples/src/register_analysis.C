#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "kapi.h"

using std::cerr;
using std::cout;
using std::endl;
using std::hex;
using std::dec;

int usage(char *basename)
{
    cerr << "Usage: " << basename << " <hostname> <port#> <module>\n";
    return 0;
}

kapi_manager kmgr;

void error_exit(int retval)
{
    kmgr.detach();
    exit(retval);
}

int main(int argc, char **argv)
{
    int rv = 0;

    if (argc != 4) {
	return usage(argv[0]);
    }

    char *host = argv[1];
    int port = atoi(argv[2]);
    char *modname = argv[3];

    // Attach to the kernel
    if ((rv = kmgr.attach(host, port)) < 0) {
        cerr << "attach error\n";
	return rv;
    }

    // Make sure testing flag not set
    if ((rv = kmgr.setTestingFlag(false)) < 0) {
	cerr << "setTestingFlag error\n";
        error_exit(rv);
    }
    
    // Find requested module
    kapi_module kmod;
    if ((rv = kmgr.findModule(modname, &kmod)) < 0) {
	cerr << "findModule error\n";
        error_exit(rv);
    }

    // Get all functions in module
    kapi_vector<kapi_function> kfuncs;
    if ((rv = kmod.getAllFunctions(&kfuncs)) < 0) {
	cerr << "getAllFunctions error\n";
        error_exit(rv);
    }
    unsigned numFuncs = kfuncs.size();

    char func_name[31];
    char free_regs_string[1024];

    cout << "Printing register analysis results for module " << modname
	 << ".\n";

    // For each function in module, find and print the register analysis
    // results for the entry point(s)
    for(int i=0; i<numFuncs; i++) {
       kapi_vector<kapi_point> func_entries;
       unsigned numEntries;
       
       kapi_function &kfunc = kfuncs[i];
       kfunc.getName(&func_name[0], 30);
       func_name[30] = '\0';
       cout << "\n<function " << &func_name[0] << ">\n";
       if ((rv = kfunc.findEntryPoint(&func_entries)) < 0) {
           cerr << "findEntryPoint error\n";
           error_exit(rv);
       }
       numEntries = func_entries.size();
       for(int j=0; j<numEntries; j++) {
           if ((rv = kmgr.getRegAnalysisResults(func_entries[j].getAddress(), 
                                                false, true,
                                         &free_regs_string[0], 1023)) < 0) {
               cerr << "getRegAnalysisResults error\n";
               error_exit(rv);
           }
           cout << "- entry @ 0x" << std::hex << func_entries[j].getAddress()
	        << std::dec << ":\n" << free_regs_string << endl;
       }
    }

    if ((rv = kmgr.detach()) < 0)
        cerr << "detach error\n";

    return rv;
}

