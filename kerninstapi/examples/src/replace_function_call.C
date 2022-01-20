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

int main(int argc, char **argv)
{
    int rv = 0;
    if (argc != 3) {
	return usage(argv[0]);
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    const char *modName = "kerninst";
    const char *origcalleeName = "kerninst_call_once";
    const char *repcalleeName = "kerninst_call_once_v2";
    const char *funcName = "kerninst_ioctl";

    // Attach to the kernel on the target host
    if ((rv = kmgr.attach(host, port)) < 0) {
       cerr << "attach error\n";
       return rv;
    }

    // Find the module of interest, initialize kmod
    kapi_module kmod;
    if ((rv = kmgr.findModule(modName, &kmod)) < 0) {
       cerr << "findModule(" << modName << ") error\n";
       error_exit(rv);
    }

    // Find the function of interest, initialize kfunc
    kapi_function kfunc;
    if ((rv = kmod.findFunction(funcName, &kfunc)) < 0) {
       cerr << "findFunction(" << funcName << ") error\n";
       error_exit(rv);
    }

    // Find the callee functions
    kapi_function origcallee;
    if ((rv = kmod.findFunction(origcalleeName, &origcallee)) < 0) {
       cerr << "findFunction(" << origcalleeName << ") error\n";
       error_exit(rv);
    }
    kptr_t origcallee_entry = origcallee.getEntryAddr();

    kapi_function repcallee;
    if ((rv = kmod.findFunction(repcalleeName, &repcallee)) < 0) {
       cerr << "findFunction(" << repcalleeName << ") error\n";
       error_exit(rv);
    }
    kptr_t repcallee_entry = repcallee.getEntryAddr();

    // Find address of callsite to origcallee in kfunc
    kapi_vector<kptr_t> callsFromAddr, callsToAddr;
    kapi_vector<kptr_t> interprocBranchesFromAddr, interprocBranchesToAddr;
    kapi_vector<kptr_t> unanalyzableCallsFromAddr;
    
    if ((rv = kfunc.getCallees(NULL,
                               &callsFromAddr, &callsToAddr,
                               &interprocBranchesFromAddr, 
                               &interprocBranchesToAddr, 
                               &unanalyzableCallsFromAddr)) < 0) {
        cerr << "getCallees() error\n";
        error_exit(rv);
    }

    kptr_t callsite = 0;

    kapi_vector<kptr_t>::const_iterator csite = callsFromAddr.begin();
    kapi_vector<kptr_t>::const_iterator dsite = callsToAddr.begin();
    for (; csite != callsFromAddr.end(); csite++, dsite++) {
        if(*dsite == origcallee_entry) {
            callsite = *csite;
            break;
        }
    }

    if(callsite == 0) {
        cerr << "ERROR: Unable to locate call to kerninst_call_once() in kerninst_ioctl()\n";
        return kmgr.detach();
    }

    // Call original callee
    cout << "Triggering original call\n";
    kmgr.callOnce();

    sleep(1);

    // Replace original callee with new
    cout << "Replacing call at 0x" << std::hex
         << callsite << " to have new destination 0x"
         << repcallee_entry << std::dec << endl;
    int reqid = kmgr.replaceFunctionCall(callsite, repcallee_entry);
    if(reqid < 0) {
        cerr << "replaceFunctionCall error\n";
        error_exit(reqid);
    }

    sleep(5);

    // Call new callee
    cout << "Triggering the replaced call\n";
    kmgr.callOnce();

    sleep(1);

    // unReplace callee
    cout << "Unreplacing call\n";
    if ((rv = kmgr.unreplaceFunctionCall(reqid)) < 0) {
        cerr << "unreplaceFunctionCall error\n";
        error_exit(rv);
    }

    sleep(5);

    // Call original callee
    cout << "Triggering original call\n";
    kmgr.callOnce();

    // Detach from the kernel
    if (kmgr.detach() < 0)
        cerr << "detach error\n";

    return rv;
}

