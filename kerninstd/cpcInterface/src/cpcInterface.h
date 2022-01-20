#include <libcpc.h>
#include "machineInfo.h"
#include "pcrRegister.h"

class cpcInterface {
    typedef enum {OPEN, BIND, RAW_SET, CLOSE} action_t;
    int *fds;
    machineInfo mi;
    void *dlhandle;
    bool cpcPresent;

    typedef int (*my_shared_open_t)(void);
    typedef int (*my_shared_bind_event_t)(int /*fd*/, cpc_event_t */*event*/, 
					  int /*flags*/);
    typedef void (*my_shared_close_t)(int /*fd*/);
    typedef int (*my_strtoevent_t)(int /*cpuver*/, const char */*spec*/,
				   cpc_event_t */*event*/);
    typedef uint_t (*my_version_t)(uint_t version);
    typedef int (*my_access_t)(void);
    typedef int (*my_getcpuver_t)(void);

    my_shared_open_t my_shared_open;
    my_shared_bind_event_t my_shared_bind_event;
    my_shared_close_t my_shared_close;
    my_strtoevent_t my_strtoevent;
    my_version_t my_version;
    my_access_t my_access;
    my_getcpuver_t my_getcpuver;

    // Routine to perform an action (OPEN,BIND,RAW_SET,CLOSE) on all CPUs
    void doAllCPUs(const machineInfo &ami, action_t action, void *ev = NULL);
 public:
    cpcInterface(const machineInfo &ami);
    // Program PCR registers of all CPUs
    void changePcrAllCPUs(const pcr_union &pcru);
    ~cpcInterface();
};
