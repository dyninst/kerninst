#include <iostream>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "kapi.h"

using namespace std;

int usage(char *basename)
{
    cerr << "Usage: " << basename << " <hostname> <port#>\n";
    return (-1);
}

kapi_manager kmgr;

void error_exit(int retval)
{
    kmgr.detach();
    exit(retval);
}

main(int argc, char **argv)
{
    int rv = 0;

    if (argc != 3) {
	return usage(argv[0]);
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    const char *modName = "kerninst";
    const char *origCalleeName = "kerninst_call_once";
    const char *newCalleeName = "kerninst_call_once_v2";

    // Attach to the kernel on the target host
    if ((rv = kmgr.attach(host, port)) < 0) {
        cerr << "attach error\n";
        return rv;
    }

    // Find the module of interest
    kapi_module origmod;
    if ((rv = kmgr.findModule(modName, &origmod)) < 0) {
        cerr << "findModule(" << modName << ") error\n";
        error_exit(rv);
    }
    
    // Find the callee functions
    kapi_function origcallee;
    if ((rv = origmod.findFunction(origCalleeName, &origcallee)) < 0) {
        cerr << "findFunction(" << origCalleeName << ") error\n";
        error_exit(rv);
    }
    kptr_t origcallee_entry = origcallee.getEntryAddr();

    kapi_function newcallee;
    if ((rv = origmod.findFunction(newCalleeName, &newcallee)) < 0) {
        cerr << "findFunction(" << newCalleeName << ") error\n";
        error_exit(rv);
    }
    kptr_t newcallee_entry = newcallee.getEntryAddr();

    // Call original function
    cout << "Calling original function\n";
    if ((rv = kmgr.callOnce()) < 0) {
        cerr << "callOnce error\n";
        error_exit(rv);
    }

    // Replace original callee with new
    cout << "Replacing original function at 0x" << std::hex 
         << origcallee_entry << " with new at 0x"
         << newcallee_entry << std::dec << endl;
    int reqid = kmgr.replaceFunction(origcallee_entry, newcallee_entry);
    if(reqid < 0) {
        cout << "replaceFunction error\n";
        error_exit(reqid);
    }

    sleep(5);

    // Call new function
    cout << "Calling new function\n";
    if ((rv = kmgr.callOnce()) < 0) {
        cerr << "callOnce error\n";
        error_exit(rv);
    }

    // unReplace callee
    cout << "Unreplacing function\n";
    if ((rv = kmgr.unreplaceFunction(reqid)) < 0) {
        cerr << "unreplaceFunction error\n";
        error_exit(rv);
    }

    sleep(5);

    // Call original function
    cout << "Calling original function\n";
    if ((rv = kmgr.callOnce()) < 0) {
        cerr << "callOnce error\n";
        error_exit(rv);
    }

    // Detach from the kernel
    if ((rv = kmgr.detach()) < 0)
        cerr << "detach error\n";
    
    return rv;
}

