#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include "kapi.h"

int usage(char *basename)
{
    cerr << "Usage: " << basename << " hostname portnum\n";
}

kapi_manager kmgr;

main(int argc, char **argv)
{
    if (argc != 3) {
	return usage(argv[0]);
    }

    char *host = argv[1];
    int port = atoi(argv[2]);

    // Attach to the kernel
    if (kmgr.attach(host, port) < 0) {
	abort();
    }

    if (kmgr.setTestingFlag(false) < 0) {
	abort();
    }

    kapi_module kmod;
    if (kmgr.findModule("genunix", &kmod) < 0) {
	abort();
    }

    kapi_function kfunc;
    if (kmod.findFunction("pvn_getpages", &kfunc) < 0) {
	abort();
    }

    kapi_vector<kptr_t> regCallsFromAddr, regCallsToAddr;
    kapi_vector<kptr_t> interprocFromAddr, interprocToAddr;
    kapi_vector<kptr_t> unanalyzableCallsFromAddr;
    
    if (kfunc.getCallees(NULL,
			 &regCallsFromAddr, &regCallsToAddr,
			 &interprocFromAddr, &interprocToAddr, 
			 &unanalyzableCallsFromAddr) < 0) {
	abort();
    }

    kapi_vector<kptr_t>::const_iterator isite =
	unanalyzableCallsFromAddr.begin();
    for (; isite != unanalyzableCallsFromAddr.end(); isite++) {
	const kptr_t siteAddr = *isite; // 0x100b5b18;
	if (kmgr.watchCalleesAt(siteAddr) < 0) {
	    abort();
	}
    }
    sleep(30);
    
    if (kmgr.callOnce() < 0) {
	abort();
    }

    isite = unanalyzableCallsFromAddr.begin();
    for (; isite != unanalyzableCallsFromAddr.end(); isite++) {
	const kptr_t siteAddr = *isite; // 0x100b5b18;
	if (kmgr.unWatchCalleesAt(siteAddr) < 0) {
	    abort();
	}
	kapi_vector<kptr_t> callees;
	kapi_vector<unsigned> counts;
	if (kmgr.getCalleesForSite(siteAddr, &callees, &counts) < 0) {
	    abort();
	}

	cout << "Callees at " << hex << siteAddr << " are:";
	kapi_vector<kptr_t>::const_iterator icallee = callees.begin();
	for (; icallee != callees.end(); icallee++) {
	    cout << " " << *icallee;
	}
	cout << endl;
    }
    if (kmgr.detach() < 0) {
	abort();
    }
}

