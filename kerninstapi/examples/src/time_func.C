#include <iostream>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "kapi_timer_lib.h"

kapi_manager kmgr;

int usage(char *basename)
{
    cerr << "Usage: " << basename << " <hostname> <port#> <module> <function> [--walltime]\n";
    return (-1);
}

void error_exit(int retval)
{
    kmgr.detach();
    exit(retval);
}

int main(int argc, char **argv)
{
    int rv = 0;
    bool set_wall_time = false;
    if (argc != 5 && argc != 6) {
	return usage(argv[0]);
    }

    if(argc == 6) {
        if(!(strcmp(argv[5], "--walltime")))
	    set_wall_time = true;
        else
	    return usage(argv[0]);
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    const char *modName = argv[3];
    const char *funcName = argv[4];

    // Attach to the kernel on the target host
    if ((rv = kmgr.attach(host, port)) < 0) {
        cerr << "attach error\n";
        return rv;
    }

    // Turn off testing mode, just in case it was turned on by a 
    // previous kerninstd client
    kmgr.setTestingFlag(false);

    // Find the module of interest, initialize kmod
    kapi_module kmod;
    if ((rv = kmgr.findModule(modName, &kmod)) < 0) {
        cerr << "findModule error\n";
        error_exit(rv);
    }

    // Find the function of interest, initialize kfunc
    kapi_function kfunc;
    if ((rv = kmod.findFunction(funcName, &kfunc)) < 0) {
        cerr << "findFunction error\n";
        error_exit(rv);
    }

    // Find the instrumentation points: the entries to the function
    kapi_vector<kapi_point> entries;
    if ((rv = kfunc.findEntryPoint(&entries)) < 0) {
        cerr << "findEntryPoint error\n";
        error_exit(rv);
    }

    // Find the instrumentation points: the exits from the function
    kapi_vector<kapi_point> exits;
    if ((rv = kfunc.findExitPoints(&exits)) < 0) {
        cerr << "findExitPoints error\n";
        error_exit(rv);
    }

    verbose_timer_lib = true; // verbose output from timer lib functions

    // Initialize vtimer environment
    if ((rv = initialize_vtimer_environment(kmgr)) < 0)
        error_exit(rv);
    
    // Insert vtimer at points
    int vt_id = add_vtimer(HWC_TICKS, entries, exits, set_wall_time, 
                           default_sample_callback, 1000);
    if(vt_id < 0)
        error_exit(vt_id);

    // Loop for some time
    time_t t = time(NULL);
    while(time(NULL) - t < 60) {
       if(kmgr.waitForEvents(UINT_MAX) < 0 ||
           kmgr.handleEvents() < 0) {
          cerr << "waitForEvents || handleEvents error\n";
          kmgr.detach();
          return -1;
       }
    }

    // Remove vtimer
    kapi_vector<int> vids;
    vids.push_back(vt_id);
    remove_vtimers(vids);

    // Cleanup vtimer environment
    if ((rv = cleanup_vtimer_environment()) < 0)
       error_exit(rv);

    // Detach from the kernel
    return kmgr.detach();
}

