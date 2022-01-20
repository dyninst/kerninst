#include <iostream>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "kapi.h"
#include "kernInstBits.h"

using namespace std;

int usage(char *basename)
{
    cerr << "Usage: " << basename << " <hostname> <port#>\n";
    return (-1);
}

main(int argc, char **argv)
{
    kapi_manager kmgr;

    if (argc != 3) {
	return usage(argv[0]);
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);

    // Attach to the kernel on the target host
    if (kmgr.attach(host, port) < 0) {
	abort();
    }

    // invoke callOnce before enabling debug output
    kmgr.callOnce();

    // set debug output for ioctls
    kmgr.toggleDebugBits(KERNINST_DEBUG_IOCTLS);

    // invoke callOnce with debug output enabled
    kmgr.callOnce();

    // turn off debug output
    kmgr.toggleDebugBits(KERNINST_DEBUG_IOCTLS);

    // invoke callOnce without debug output
    kmgr.callOnce();

    // Detach from the kernel
    if (kmgr.detach() < 0) {
	abort();
    }
}

