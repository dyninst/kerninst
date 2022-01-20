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
    cerr << "Usage: " << basename << " hostname portnum module function [--justtesting]\n";
    return (-1);
}

int main(int argc, char **argv)
{
    unsigned set_just_testing = 0;
    if (argc != 5 && argc != 6) {
	return usage(argv[0]);
    }

    if(argc == 6) {
       if(!(strncmp(argv[5], "--justtesting", 13)))
	  set_just_testing = 1;
       else
	  return usage(argv[0]);
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);
    const char *modName = argv[3];
    const char *funcName = argv[4];

    // Attach to the kernel on the target host
    if (kmgr.attach(host, port) < 0) {
       cerr << "attach error\n";
       return -1;
    }

    if(set_just_testing)
       kmgr.setTestingFlag(true);
    else
       kmgr.setTestingFlag(false);

    // Find the module of interest, initialize kmod
    kapi_module kmod;
    if (kmgr.findModule(modName, &kmod) < 0) {
       cerr << "findModule error\n";
       kmgr.detach();
       return -1;
    }

    // Find the function of interest, initialize kfunc
    kapi_function kfunc;
    if (kmod.findFunction(funcName, &kfunc) < 0) {
       cerr << "findFunction error\n";
       kmgr.detach();
       return -1;
    }

    // Find the instrumentation points: the entry to the function
    kapi_vector<kapi_point> entries;
    if (kfunc.findEntryPoint(&entries) < 0) {
       cerr << "findEntryPoint error\n";
       kmgr.detach();
       return -1;
    }

    // Find the instrumentation points: the exits from the function
    kapi_vector<kapi_point> exits;
    if (kfunc.findExitPoints(&exits) < 0) {
       cerr << "findExitPoints error\n";
       kmgr.detach();
       return -1;
    }

    verbose_timer_lib = true; // verbose output from timer lib functions

    // Initialize vtimer environment
    ierr rv = initialize_vtimer_environment(kmgr);
    if(rv < 0) {
       kmgr.detach();
       return rv;
    }
    
    // Insert vtimer at points
    int vt_id = add_vtimer(HWC_TICKS, entries, exits, false, 
                           default_sample_callback, 1000);
    if(vt_id < 0) {
       kmgr.detach();
       return vt_id;
    }

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
    rv = cleanup_vtimer_environment();
    if(rv < 0) {
       kmgr.detach();
       return rv;
    }

    // Detach from the kernel
    return kmgr.detach();
}

