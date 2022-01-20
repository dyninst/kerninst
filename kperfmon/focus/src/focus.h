// focus.h
// Our canonical representation of a focus
// This class is perpetually evolving.

#ifndef _FOCUS_H_
#define _FOCUS_H_

#include "util/h/Dictionary.h"
#include "util/h/kdrvtypes.h"
#include "common/h/String.h"
#include <inttypes.h> // uint32_t

// Non-negative cpu ids correspond to a given cpu, FUZZY means undefined,
// CPU_TOTAL -- aggregate the values across all CPUs
// Other ids are currently unused
enum {
    FIRST_REAL_CPU = 0,
    FUZZY = -1,
    CPU_TOTAL = -2,
    CPU_AVG = -3,
    CPU_MAX = -4,
    CPU_MIN = -5
};

class focus {
 public:
   typedef uint16_t bbid_t;

 private:
   // The id (assigned not via some function of the focus' contents, which would be
   // nice but impractical, but by a simple increasing unique id)
   unsigned id;

   // Choosing a particular kernel function:
   pdstring module_name;
      // used to be a module num, but that's now deprecated since module numbers can
      // change at runtime.

   kptr_t fnAddr; // 0 if not defined (whole module)
   bbid_t bbid; // (bbid_t)-1 --> no basic block (whole fn)

   // Choosing a particular location within that kernel function:
   int location; // 0 --> entry, 1 --> exit, 2 --> use insnnum below
      // This has pretty much bit-rotted and is presently unused (more specifically,
      // all of the simpleMetrics, with perhaps an exception for add1AtEntry,
      // ignore this).
   unsigned insnnum;
      // only defined if location is set to 2 and since it's not anymore, this
      // field has pretty much lost its meaning.  It'll probably be removed some day.
      // It's just that occasionally, we find it useful (for testing) to instrument
      // at a particular instruction # with an "add1"-type simpleMetric...so for
      // now, the "add1AtEntry" simpleMetric still uses this.

   // Choosing a particular caller of this kernel function (not presently used):
   unsigned calledfrom_mod;
   unsigned calledfrom_fn;

   // Choosing a particular subset of processes.  fyi: Kernel threads not bound
   // to any process seem to have a proc_t* of &proc0, so they have a pid of 0
   // (as opposed to undefined).  This is nice.
   pdvector<long> pids;

   int cpuid;

   focus(unsigned iid,
         const pdstring &imodname,
         kptr_t ifnAddr, // 0 --> no function defined (whole module)
         bbid_t ibbId, // (bbid_t)-1 --> no basic block defined (whole fn)
         int iloc, unsigned iinsnnum,
         unsigned icalledfrom_mod, unsigned icalledfrom_fn,
         const pdvector<long> &ipids,
	 int cpuid);
      // private, lest the outside world try to assign ids (the 1st param to this ctor)

   static dictionary_hash<focus, unsigned> existingFocusIds;
   static unsigned nextUniqueFocusIdToUse;

   static unsigned focusHash(const focus &);

   void makeFocus_common_id_management(focus &);
   
 public:
   // The outside world should call one of the follow routines to create a focus; it
   // creates a focus given the fields you pass in, and smartly sets the id (to an
   // existing focus if everything matches, or to a new unique focus id if this focus
   // field combo hasn't yet been seen).

   class Root {};
   static focus makeFocus(Root);
   
   static focus makeFocus(const pdstring &imodname,
                          kptr_t ifnAddr,
                          bbid_t ibbId,
                          int iloc, unsigned iinsnnum,
                          unsigned icalledfrom_mod, unsigned icalledfrom_fn,
                          const pdvector<long> &ipids,
			  int cpuid);

   // Create a copy of src, but change cpuid
   static focus makeFocus(const focus &src, int icpuid);

   focus(const focus &src);

   focus& operator=(const focus &);

   pdstring getName() const;

   unsigned getId() const { return id; }

   bool operator==(const focus &other) const;

   bool isRoot() const { return module_name.length() == 0; }
   bool isModule() const {
      return !isRoot() && fnAddr == 0;
   }

   const pdstring &getModName() const { return module_name; }
   kptr_t getFnAddr() const { return fnAddr; } // 0 --> no function (whole module)
   bbid_t getBBId() const { return bbid; } // (bbid_t)-1 --> no bb (whole fn)
   int getLocation() const { return location; }
   unsigned getInsnNum() const { return insnnum; }
   const pdvector<long> &getPids() const { return pids; }
   int getCPUId() const {
       return cpuid;
   }
};

#endif
