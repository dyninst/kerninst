#include <iostream>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "kapi.h"

using namespace std;

int usage(char *arg0)
{
    cerr << "Usage: " << arg0 << " <hostname> <port#> <module> <function>\n";
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

    if (argc != 5) {
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

    // Find the instrumentation point: the entry to the function
    kapi_vector<kapi_point> entries;
    if ((rv = kfunc.findEntryPoint(&entries)) < 0) {
        cerr << "findEntryPoint error\n";
	error_exit(rv);
    }

    // Allocate space for the counter in the kernel space
    kptr_t addr;
    unsigned size = sizeof(kapi_int_t);
    if ((addr = kmgr.malloc(size)) == 0) {
	cerr << "malloc error\n";
	error_exit(addr);
    }

    // Initialize the kernel memory, by copying buffer from the user space
    kapi_int_t user_copy = 0;
    if ((rv = kmgr.memcopy(&user_copy, addr, size)) < 0) {
	cerr << "memcopy error\n";
	error_exit(rv);
    }

    // Create an int variable in the allocated space
    kapi_int_variable intCounter(addr);
    
    // Generate code to increment the variable atomically
    kapi_arith_expr code(kapi_atomic_assign, intCounter, 
			 kapi_arith_expr(kapi_plus, intCounter, 
					 kapi_const_expr(1)));

    // Splice the code at specified points
    int sh;
    if ((sh = kmgr.insertSnippet(code, entries[0])) < 0) {
	cerr << "insertSnippet error\n";
	error_exit(sh);
    }

    // Loop for some time
    time_t t = time(NULL);
    while (time(NULL) - t < 20) {
	kmgr.handleEvents();
	if ((rv = kmgr.memcopy(addr, &user_copy, size)) < 0) {
	    cerr << "memcopy error\n";
	    error_exit(rv);
	}
	// The client's byte ordering may not match that of the kernel
	kmgr.to_client_byte_order(&user_copy, size);

	cout << "# Entries = " << user_copy << endl;
	sleep(1);
    }

    // Clean-up
    if ((rv = kmgr.removeSnippet(sh)) < 0) {
	cerr << "removeSnippet error\n";
	error_exit(rv);
    }
    
    // Free the kernel memory
    if ((rv = kmgr.free(addr)) < 0) {
	cerr << "free error\n";
	error_exit(rv);
    }
    
    // Detach from the kernel
    if ((rv = kmgr.detach()) < 0) {
	cerr << "detach error\n";
    }

    return rv;
}

