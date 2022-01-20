#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include "kapi.h"

using namespace std;

int usage(char *basename)
{
    cerr << "Usage: " << basename << " <hostname> <port#> <module> <function>\n";
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

    if (argc != 5) {
	return usage(argv[0]);
    }

    char *host = argv[1];
    int port = atoi(argv[2]);
    char *modName = argv[3];
    char *funcName = argv[4];

    // Attach to the kernel
    if ((rv = kmgr.attach(host, port)) < 0) {
        cerr << "attach error\n";
        return rv;
    }

    // make sure testing flag is not set in kerninstd
    if ((rv = kmgr.setTestingFlag(false)) < 0) {
	cerr << "setTestingFlag error\n";
        error_exit(rv);
    }

    kapi_module kmod;
    if ((rv = kmgr.findModule(modName, &kmod)) < 0) {
	cerr << "findModule error\n";
        error_exit(rv);
    }

    kapi_function kfunc;
    if ((rv = kmod.findFunction(funcName, &kfunc)) < 0) {
	cerr << "findFunction error\n";
        error_exit(rv);
    }

    kapi_vector<kptr_t> regCallsFromAddr, regCallsToAddr;
    kapi_vector<kptr_t> interprocFromAddr, interprocToAddr;
    kapi_vector<kptr_t> unanalyzableCallsFromAddr;
    
    if ((rv = kfunc.getCallees(NULL,
			       &regCallsFromAddr, &regCallsToAddr,
			       &interprocFromAddr, &interprocToAddr, 
			       &unanalyzableCallsFromAddr)) < 0) {
        cerr << "getCallees error\n";
        error_exit(rv);
    }

    cout << "Callee statistics for " << modName << "//" << funcName << "():\n"
         << "- There are " << regCallsFromAddr.size() 
         << " regular callsites\n"; 
    kapi_vector<kptr_t>::const_iterator csite = regCallsFromAddr.begin();
    kapi_vector<kptr_t>::const_iterator dsite = regCallsToAddr.begin();
    for (; csite != regCallsFromAddr.end(); csite++, dsite++) {
        cout << " callsite: 0x" << hex << *csite << ", dest: 0x" 
             << *dsite << dec << endl; 
    }

    cout << "- There are " << interprocFromAddr.size() 
         << " interprocedural branch callsites\n";
    csite = interprocFromAddr.begin();
    dsite = interprocToAddr.begin();
    for (; csite != interprocFromAddr.end(); csite++, dsite++) {
        cout << " callsite: " << hex << *csite << ", dest: " 
             << *dsite << dec << endl; 
    }

    cout << "- There are " << unanalyzableCallsFromAddr.size() 
         << " unanalyzable callsites\n";
    csite = unanalyzableCallsFromAddr.begin();
    for (; csite != unanalyzableCallsFromAddr.end(); csite++) {
        cout << " callsite: " << hex << *csite << dec << endl; 
    }

    if(unanalyzableCallsFromAddr.size() == 0) {
        cout << "No unanalyzable call sites, so nothing to watch.\n";
        error_exit(0);
    }

    cout << "Beginning to watch callees at unanalyzable callsites\n";
    kapi_vector<kptr_t>::const_iterator isite =
	unanalyzableCallsFromAddr.begin();
    for (; isite != unanalyzableCallsFromAddr.end(); isite++) {
	const kptr_t siteAddr = *isite;
	if ((rv = kmgr.watchCalleesAt(siteAddr)) < 0) {
	    cerr << "watchCalleesAt error\n";
	    error_exit(rv);
	}
    }

    cout << "Sleeping for 30 seconds\n";
    sleep(30);

    isite = unanalyzableCallsFromAddr.begin();
    for (; isite != unanalyzableCallsFromAddr.end(); isite++) {
        const kptr_t siteAddr = *isite;
	if ((rv = kmgr.unWatchCalleesAt(siteAddr)) < 0) {
	    cerr << "unWatchCalleesAt error\n";
	    error_exit(rv);
	}
	kapi_vector<kptr_t> callees;
	kapi_vector<unsigned> counts;
	if ((rv = kmgr.getCalleesForSite(siteAddr, &callees, &counts)) < 0) {
	    cerr << "getCalleesForSite error\n";
	    error_exit(rv);
	}

	cout << "Callees for callsite @ 0x" << std::hex << siteAddr << ":";
	kapi_vector<kptr_t>::const_iterator icallee = callees.begin();
	for (; icallee != callees.end(); icallee++) {
	    cout << " " << *icallee;
	}
	cout << std::dec << endl;
    }

    if ((rv = kmgr.detach() < 0))
        cerr << "detach error\n";

    return rv;
}

