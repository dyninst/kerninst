#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <libcpc.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include "cpcInterface.h"
#include "pcrRegister.h"
#include "kernelDriver.h"

// Solaris 7 does not have libcpc, so we provide a few dummy functions
extern "C" int dummy_shared_open(void)
{
    return 0;
}

extern "C" int dummy_shared_bind_event(int /*fd*/, cpc_event_t */*event*/, 
				       int /*flags*/)
{
    return 0;
}

extern "C" void dummy_shared_close(int /*fd*/)
{
}

extern "C" int dummy_strtoevent(int /*cpuver*/, const char */*spec*/,
				cpc_event_t */*event*/)
{
    return 0;
}

extern "C" uint_t dummy_version(uint_t version)
{
    return version;
}

extern "C" int dummy_access(void)
{
    return 0;
}

extern "C" int dummy_getcpuver(void)
{
    return 0;
}

// Performs assorted functions (OPEN,BIND,RAW_SET,CLOSE) on each CPU in the 
// system. Binds itself to each CPU successively
void cpcInterface::doAllCPUs(const machineInfo &mi, action_t action, void *ev)
{
    // BIND and RAW_SET interpret ev differently
    cpc_event_t *event = (cpc_event_t *)ev;
    uint64_t *raw = (uint64_t *)ev;

    int num_cpus = mi.getNumCPUs();
    for (int i=0; i<num_cpus; i++) {
	unsigned cpuid = mi.getIthCPUId(i);
	if (processor_bind(P_PID, P_MYID, cpuid, NULL) < 0) {
	    perror("processor_bind error");
	    assert(false);
	}
	// sched_yield(); // We want to be rescheduled on a new CPU
	switch(action) {
	case OPEN:
	    if ((fds[i] = my_shared_open()) < 0) {
		perror("cpc_shared_open error");
		assert(false);
	    }
	    break;
	case BIND:
	    // Used on Solaris 8 -- program %pcr with libcpc
	    if (my_shared_bind_event(fds[i], event, 0) < 0) {
		perror("cpc_shared_bind_event error");
		assert(false);
	    }
	    break;
	case RAW_SET:
	    // Used on Solaris 7 -- program %pcr directly
	    extern kernelDriver *global_kernelDriver;
	    assert(global_kernelDriver != 0);
	    global_kernelDriver->setPcrRawCurrentCPU(*raw);
	    break;
	case CLOSE:
	    my_shared_close(fds[i]);
	    break;
	}
    }
    // Remove the binding
    if (processor_bind(P_PID, P_MYID, PBIND_NONE, NULL) < 0) {
	perror("processor_bind error");
	assert(false);
    }
}

cpcInterface::cpcInterface(const machineInfo &ami) : mi(ami)
{
    // Solaris 7 does not have libcpc, so we provide a few dummy functions
    if ((dlhandle = dlopen("libcpc.so", RTLD_GLOBAL | RTLD_LAZY)) == 0) {
	cout << "Failed to find libcpc, will program hardware counters "
	     << "manually\n";
	my_shared_open = dummy_shared_open;
	my_shared_bind_event = dummy_shared_bind_event;
	my_shared_close = dummy_shared_close;
	my_strtoevent = dummy_strtoevent;
	my_version = dummy_version;
	my_access = dummy_access;
	my_getcpuver = dummy_getcpuver;
	cpcPresent = false;
    }
    else {
	if ((my_shared_open = (my_shared_open_t)
	     dlsym(dlhandle, "cpc_shared_open")) == 0 ||
	    (my_shared_bind_event = (my_shared_bind_event_t)
	     dlsym(dlhandle, "cpc_shared_bind_event")) == 0 ||
	    (my_shared_close = (my_shared_close_t)
	     dlsym(dlhandle, "cpc_shared_close")) == 0 ||
	    (my_strtoevent = (my_strtoevent_t)
	     dlsym(dlhandle, "cpc_strtoevent")) == 0 ||
	    (my_version = (my_version_t)
	     dlsym(dlhandle, "cpc_version")) == 0 ||
	    (my_access = (my_access_t)
	     dlsym(dlhandle, "cpc_access")) == 0 ||
	    (my_getcpuver = (my_getcpuver_t)
	     dlsym(dlhandle, "cpc_getcpuver")) == 0) {
	    cout << "Symbol lookup failure\n";
	    assert(false);
	}
	cpcPresent = true;
    }
    
    if (my_version(CPC_VER_CURRENT) == CPC_VER_NONE) {
	perror("cpc_version error");
	assert(false);
    }
    if (my_access() < 0) {
	perror("cpc_access error");
	assert(false);
    }
    int num_cpus = mi.getNumCPUs();
    fds = new int[num_cpus];
    doAllCPUs(mi, OPEN, 0);
}

// Program PCR registers of all CPUs
void cpcInterface::changePcrAllCPUs(const pcr_union &pcru)
{
    int cpuver = my_getcpuver();
    if (cpuver < 0) {
	perror("cpc_getcpuver error");
	assert(false);
    }

    // Create the config string
    char str[100];
    sprintf(str, "pic0=0x%x,pic1=0x%x", pcru.f.s0, pcru.f.s1);
    if (pcru.f.st) {
	strcat(str, ",sys");
    }
    if (!pcru.f.ut) {
	strcat(str, ",nouser");
    }

    // Convert the string into the event structure
    cpc_event_t event;
    if (my_strtoevent(cpuver, str, &event) < 0) {
	perror("cpc_strtoevent error");
	assert(false);
    }
    // Program PCR or all CPUs
    if (cpcPresent) {
	doAllCPUs(mi, BIND, &event);
    }
    else {
	uint64_t raw = pcru.rawdata;
	doAllCPUs(mi, RAW_SET, &raw);
    }
}

cpcInterface::~cpcInterface()
{
    // Close fds on all CPUs (probably no need to reschedule for that, but
    // let's be safe here)
    doAllCPUs(mi, CLOSE, 0);
    if (dlhandle != 0) {
	dlclose(dlhandle);
    }
    delete[] fds;
}
