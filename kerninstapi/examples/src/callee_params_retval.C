#include <iostream>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "kapi.h"

using namespace std;

#ifdef _KDRV64_ 
typedef uint64_t my_int_t;
#else
typedef uint32_t my_int_t;
#endif

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
    const char *funcName = "kerninst_call_me";

    // Attach to the kernel on the target host
    if ((rv = kmgr.attach(host, port)) < 0) {
        cerr << "attach error\n";
        return rv;
    }

    // Set testing flag to false, just in case it was set true by a
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

    // Find the instrumentation point: the entry to the function
    kapi_vector<kapi_point> entries;
    if ((rv = kfunc.findEntryPoint(&entries)) < 0) {
	cerr << "findEntryPoint error\n";
	error_exit(rv);
    }

    // Find the instrumentation point: the entry to the function
    kapi_vector<kapi_point> exits;
    if ((rv = kfunc.findExitPoints(&exits)) < 0) {
	cerr << "findExitPoints error\n";
	error_exit(rv);
    }    

    // Allocate space for the counter in the kernel space
    kptr_t addr;
    unsigned size = 4*sizeof(my_int_t);
    if ((addr = kmgr.malloc(size)) == 0) {
	cerr << "malloc error\n";
	error_exit(addr);
    }
    cerr << "addr is 0x" << hex << (unsigned long)addr << dec << endl;

    // Initialize the kernel memory, by copying buffer from the user space
    my_int_t user_copy[4] = {0,0,0,0};
    if ((rv = kmgr.memcopy(&user_copy[0], addr, size)) < 0) {
	cerr << "memcopy error\n";
	error_exit(rv);
    }

    // Create an int variable in the allocated space
    kapi_int_variable param0(addr);
    kapi_int_variable param1(addr+sizeof(my_int_t));
    kapi_int_variable param2(addr+2*sizeof(my_int_t));
    kapi_int_variable retval(addr+3*sizeof(my_int_t));

    // Generate code to get the callee params
    kapi_vector<kapi_snippet> set_params;
    set_params.push_back(kapi_arith_expr(kapi_atomic_assign, param0, 
                                         kapi_param_expr(0)));
    set_params.push_back(kapi_arith_expr(kapi_atomic_assign, param1, 
                                         kapi_param_expr(1)));
    set_params.push_back(kapi_arith_expr(kapi_atomic_assign, param2, 
                                         kapi_param_expr(2)));
    kapi_sequence_expr entry_code(set_params);

    // Generate code to get the callee retval
    kapi_arith_expr exit_code(kapi_atomic_assign, retval, 
                              kapi_retval_expr(kfunc));

    // Splice the code at specified points
    int entry_h;
    if ((entry_h = kmgr.insertSnippet(entry_code, entries[0])) < 0) {
	cerr << "insertSnippet error\n";
	error_exit(entry_h);
    }

    kapi_vector<int> exit_hs;
    for(unsigned n=0; n<exits.size(); n++) {
        int exit_h;
        if ((exit_h = kmgr.insertSnippet(exit_code, exits[n])) < 0) {
            cerr << "insertSnippet error\n";
	    error_exit(exit_h);
        }
        exit_hs.push_back(exit_h);
    }

    // Call the function
    kmgr.callOnce();
    sleep(2);

    // Read the data
    if ((rv = kmgr.memcopy(addr, &user_copy, size)) < 0) {
        cerr << "memcopy error\n";
	error_exit(rv);
    }

    // Print the results
    cout << "For kerninst//kerninst_call_me():\n- param 0 is " << user_copy[0]
         << "\n- param 1 is " << user_copy[1] << "\n- param 2 is "
         << user_copy[2] << "\n- retval is " << user_copy[3] << endl;


    // Clean-up
    for(unsigned n=0; n<exits.size(); n++) {
       int exit_h = exit_hs[n];
       if ((rv = kmgr.removeSnippet(exit_h)) < 0) {
           cerr << "removeSnippet error\n";
	   error_exit(rv);
       }
    }
    if ((rv = kmgr.removeSnippet(entry_h)) < 0) {
	cerr << "removeSnippet error\n";
	error_exit(rv);
    }
    
    // Free the kernel memory
    if ((rv = kmgr.free(addr)) < 0) {
	cerr << "free error\n";
	error_exit(rv);
    }
    
    // Detach from the kernel
    if ((rv = kmgr.detach()) < 0)
	cerr << "detach error\n";

    return rv;
}

