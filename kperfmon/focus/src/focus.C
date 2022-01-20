// focus.C

#include "focus.h"
#include <algorithm> // stl's sort()
#include "util/h/hashFns.h"
#include "util/h/rope-utils.h"
#include "kapi.h"

unsigned focus::focusHash(const focus &theFocus) {
   // umm, this awful hash function is just temporary

   return stringHash(theFocus.module_name) +
      addrHash4(theFocus.fnAddr) +
      theFocus.bbid +
      theFocus.location + theFocus.insnnum +
      theFocus.calledfrom_mod + theFocus.calledfrom_fn + theFocus.pids.size();
}


dictionary_hash<focus, unsigned> focus::existingFocusIds(focusHash);
unsigned focus::nextUniqueFocusIdToUse = 0;

focus::focus(unsigned iid,
             const pdstring &imodname,
             kptr_t ifnAddr,
             bbid_t ibbid,
             int iloc, unsigned iinsnnum,
             unsigned icalledfrom_mod, unsigned icalledfrom_fn,
             const pdvector<long> &ipids,
	     int icpuid) :
      module_name(imodname),
      pids(ipids) {
   id = iid;

   // if module_name.length() is 0, then the module is nonexistent (i.e. "root")

   fnAddr = ifnAddr;
   bbid = ibbid;

   location = iloc;
   insnnum = iinsnnum;

   calledfrom_mod = icalledfrom_mod;
   calledfrom_fn = icalledfrom_fn;
   
   // The following is so that operator==() will return true whenever set elements
   // are equal, regardless of order.
   std::sort(pids.begin(), pids.end());

   cpuid = icpuid;
}

focus::focus(const focus &src) :
      module_name(src.module_name),
      pids(src.pids) {
   id = src.id;

   fnAddr = src.fnAddr;
   bbid = src.bbid;

   location = src.location;
   insnnum = src.insnnum;

   calledfrom_mod = src.calledfrom_mod;
   calledfrom_fn = src.calledfrom_fn;

   cpuid = src.cpuid;
}

focus& focus::operator=(const focus &other) {
   if (this == &other) return *this;

   id = other.id;

   module_name = other.module_name;
   fnAddr = other.fnAddr;
   bbid = other.bbid;
   location = other.location;
   insnnum = other.insnnum;
   calledfrom_mod = other.calledfrom_mod;
   calledfrom_fn = other.calledfrom_fn;
   pids = other.pids;
   cpuid = other.cpuid;

   return *this;
}

bool focus::operator==(const focus &other) const {
   // Note that we don't compare ids

   if (module_name == other.module_name && // could be empty string
       fnAddr == other.fnAddr &&
       bbid == other.bbid &&
       location == other.location &&
       insnnum == other.insnnum &&
       calledfrom_mod == other.calledfrom_mod &&
       calledfrom_fn == other.calledfrom_fn &&
       pids.size() == other.pids.size() &&
       cpuid == other.cpuid) {
      return std::equal(pids.begin(), pids.end(), other.pids.begin());
   }

   return false;
}

// static method:
focus focus::makeFocus(Root) {
   pdvector<long> ipids;
   
   focus tempFocus(0, // temp id
                   "", // empty string for module name --> no module
                   0, 0, // no function or basic block
                   UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, ipids, CPU_TOTAL);

   tempFocus.makeFocus_common_id_management(tempFocus);

   return tempFocus;
}

// static method:
focus focus::makeFocus(const pdstring &imodname,
                       kptr_t ifnAddr,
                       bbid_t ibbid,
                       int iloc, unsigned iinsnnum,
                       unsigned icalledfrom_mod, unsigned icalledfrom_fn,
                       const pdvector<long> &ipids,
		       int icpuid) {
   focus tempFocus(0, // temp id
                   imodname,
                   ifnAddr,
                   ibbid,
                   iloc, iinsnnum,
                   icalledfrom_mod, icalledfrom_fn,
                   ipids,
		   icpuid);

   tempFocus.makeFocus_common_id_management(tempFocus);

   return tempFocus;
}

// Create a copy of src, but change cpuid
focus focus::makeFocus(const focus &src, int icpuid)
{
    focus tempFocus(src);
    tempFocus.cpuid = icpuid;
    tempFocus.makeFocus_common_id_management(tempFocus);

    return tempFocus;
}

// static method:
void focus::makeFocus_common_id_management(focus &theFocus) {
   unsigned focus_id = UINT_MAX;
   if (!existingFocusIds.find(theFocus, focus_id)) {
      // Assign a new unique id...
      focus_id = nextUniqueFocusIdToUse++;

      // ...and add it to existingFocusIds[]
      existingFocusIds.set(theFocus, focus_id);
   }

   theFocus.id = focus_id;
}

pdstring focus::getName() const 
{
   const unsigned max_len = 255;
   char buf[max_len];

   if (isRoot())
      return "root";

   pdstring focus_name; // the result we'll return

   extern kapi_manager kmgr;

   kapi_module kMod;
   if (kmgr.findModule(getModName().c_str(), &kMod) != 0) {
       assert(false && "Module not found");
   }
   if (kMod.getName(buf, max_len) != 0) {
       assert(false && "Module name too long");
   }
   focus_name += pdstring(buf);

   if (isModule()) {
      return focus_name;
   }

   kapi_function kFunc;
   if (kmgr.findFunctionByAddress(getFnAddr(), &kFunc) != 0) {
       assert(false && "Function not found");
   }
   if (kFunc.getName(buf, max_len) != 0) {
       assert(false && "Function name too long");
   }
   focus_name += pdstring("/") + pdstring(buf);

   if (getBBId() != (bbid_t)-1) {
       kapi_basic_block kBB;
       if (kFunc.findBasicBlockById(getBBId(), &kBB) != 0) {
	   assert(false && "Basic block not found");
       }
       focus_name += "/bb at ";
       focus_name += addr2hex(kBB.getStartAddr());
   }

   int cid = getCPUId();

   if (cid >= FIRST_REAL_CPU) {
       focus_name += "/cpu";
       focus_name += num2string(cid);
   }
   else {
       switch(cid) {
       case FUZZY:
	   break;
       case CPU_TOTAL:
	   focus_name += "/cpu total";
	   break;
       case CPU_AVG:
	   focus_name += "/avg of cpus";
	   break;
       case CPU_MAX:
	   focus_name += "/max of cpus";
	   break;
       case CPU_MIN:
	   focus_name += "/min of cpus";
	   break;
       default:
	   assert(false && "Unknown cpuid");
	   break;
       }
   }

   const pdvector<long> &pidsToMatch = getPids();
   if (pidsToMatch.size() > 0) {
      focus_name += " pid in {";
      for (pdvector<long>::const_iterator iter = pidsToMatch.begin();
           iter != pidsToMatch.end(); ++iter) {
         const long pid = *iter;
         
         focus_name += num2string(pid);
         if (iter + 1 != pidsToMatch.end())
            focus_name += "  ";
      }

      focus_name += "}";
   }

   return focus_name;
}
