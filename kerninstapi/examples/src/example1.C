#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "kapi.h"

int usage(char *basename)
{
    cerr << "Usage: " << basename << " hostname portnum\n";
}

kapi_manager kmgr;

int data_callback(unsigned handle, uint64_t time,
		  const uint64_t *values, unsigned numvalues)
{
    static bool first_time = true;

    cout << "handle = " << handle << ", t = " << time;
    for (unsigned i=0; i<numvalues; i++) {
	cout << " " << values[i];
    }
    cout << endl;

    if (first_time) {
	cout << "Starting periodic sampling ...\n";
	kmgr.adjust_sampling_interval(handle, 1000);
	first_time = false;
    }
}

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

    kptr_t addr;
    unsigned size = 8;
    if ((addr = kmgr.malloc(size)) == 0) {
	abort();
    }
    cout << "Allocated " << size << " bytes at 0x" 
	 << hex << addr << dec << endl;

    void *buffer = malloc(size);
    assert(buffer);
    
    // Copy to kernel memory
    memset(buffer, 1, size);
    if (kmgr.memcopy(buffer, addr, size) < 0) {
	abort();
    }

    // Copy from kernel memory
    if (kmgr.memcopy(addr, buffer, size) < 0) {
	abort();
    }
    
    assert(*(char *)buffer == 1);

    // Test sampling
    kapi_mem_region data;
    data.addr = addr;
    data.nbytes = size;

    int reqid;
    if ((reqid = kmgr.sample_periodically(&data, 1, data_callback, 0)) < 0){
	abort();
    }

    while (kmgr.waitForEvents(UINT_MAX) > 0) {
	kmgr.handleEvents();
    }

    if (kmgr.stop_sampling(reqid) < 0) {
	abort();
    }

    if (kmgr.free(addr) < 0) {
	abort();
    }
    
    if (kmgr.detach() < 0) {
	abort();
    }
}

