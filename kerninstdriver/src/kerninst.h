#include <sys/conf.h> // kmutex_t
#include <sys/devops.h> // dev_info_t

#include "stacklist.h"
#include "hash_table.h"
#include "all_vtimers.h"

typedef struct {
   dev_info_t *dip;
   kmutex_t kerninst_lock;

   /* used to turn debug output on/off */
   unsigned give_debug_output;

   /* only handle context switches when the following is set to 1 */
   unsigned cswitch_stuff_enabled;

   /* spinlock for use in treating cswitch code as critical section */
   kmutex_t kerninst_cswitch_lock;

   /* vector of all vtimer addresses */
   struct all_vtimers av_vtimers;

   /* stacklist heap of vector-of-vtimers */
   struct stacklist sl_heap;

   /* hash table of thread/process to vtimer list */
   struct hash_table ht_vtimers;

   /* count of how many timers we can't save due to space limits */
   unsigned num_unsaved_timers;

   /* logged writes are maintained on a per-connection basis (i.e. created only
      on open() and fried on close()) */
   void *global_loggedWrites; /* actually a loggedWrites* but this is C */
   
   /* logged allocation requests are maintained on a per-connection basis */
   void *global_loggedAllocRequests; /* actually a loggedAllocRequests* */

} hwprfunit_t;
