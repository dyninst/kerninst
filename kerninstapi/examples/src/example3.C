#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include "kapi.h"

int usage(char *basename)
{
    cerr << "Usage: " << basename << " hostname portnum\n";
}

static void timer_handler(ClientData cd) 
{
    kapi_manager *pmgr = (kapi_manager *)cd;
    pmgr->handleEvents();
    Tcl_CreateTimerHandler(100 /* 100 millisecs */, timer_handler, pmgr);
}

kapi_manager kmgr; // Have to make it global
int main(int argc, char **argv)
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
    bool withGUI = true;

    // Once the lib gets rid of TCL usage, we will be able to create a real
    // interpreter here
    Tcl_Interp *interp = (Tcl_Interp *)kmgr.getTclInterpHack();
 
    if (withGUI && Tk_Init(interp) == TCL_ERROR) {
	cerr << "call to Tk_Init() failed: \"" 
	     << Tcl_GetStringResult(interp) << "\""
	     << "(Attempting) to continue with no GUI..." << endl;
	withGUI = false;
    }

    // Get the number of CPUs on the remote (kerninstd) machine
    int numCPUs = kmgr.getNumCPUs();

    char buffer[100]; // Tcl_Eval writes to it!
    sprintf(buffer, "initializeKperfmon %d %d", 
	    (withGUI ? 1 : 0), numCPUs);
    if (Tcl_Eval(interp, buffer) != TCL_OK) {
	cout << interp->result << endl;
	abort();
    }

    if (withGUI)
	Tk_SetClass(Tk_MainWindow(interp), "Kperfmon");

    if (withGUI) {
	// We need to call kmgr.handleEvents periodically
	Tcl_CreateTimerHandler(100 /* 100 millisecs */, timer_handler, &kmgr);
	Tk_MainLoop();
    }
    else {
	// non-GUI main loop.
	// kmgr.handleEvents returns (-1) on quit
	while (kmgr.handleEvents() != -1);
    }
    
    if (kmgr.detach() < 0) {
	abort();
    }
}

