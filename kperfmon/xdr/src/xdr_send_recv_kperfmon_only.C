// xdr_send_recv_kperfmon_only.C

#include "xdr_send_recv_kperfmon_only.h"

// Reminder: as always with (the new version of) igen, the second parameter 
// to a P_xdr_recv() will be a reference to allocated but uninitialized
// raw memory.  (Perhaps the allocation will be via new, via malloc,
// or even on the stack -- don't assume.)  Not even a default constructor
// will have been called on this variable.  So you must call the 
// constructor manually, using placement-new syntax.


