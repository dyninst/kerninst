// mm_map-solaris7.C

#include <sys/modctl.h> // mod_unload_lock
#include <sys/kobj.h> // struct module
#include "symbol_table.h" // getModuleByName()
#include "mm_map.h"
#include "util/h/kdrvtypes.h"

// These two must always go last
#include <sys/ddi.h>
#include <sys/sunddi.h>

static caddr_t get_mm_map_lowlevel() {
   // we'd like to return "mm_map" but it's not global in solaris 7
   // But fortunately, it does appear in kobj_lookup()

   void *mod = getModuleByName("genunix");
   if (mod == NULL) {
      cmn_err(CE_WARN, "get_mm_map(): could not find module genunix!  Returning NULL");
      return 0;
   }
   
   kptr_t mm_map_address = kobj_lookup(mod, "mm_map");
   // nice fast hash table lookup
   // kptr_t is either unsigned or unsigned long 
   // (whichever is long enough to hold a pointer)

   if (mm_map_address == 0) {
      cmn_err(CE_WARN, "get_mm_map():could not find mm_map in module genunix!  Trying exhaustive search");
      
      mm_map_address = kobj_getsymvalue("mm_map", 0);
      
      if (mm_map_address == 0) {
         cmn_err(CE_WARN, "get_mm_map(): even exhaustive search returned 0");

         return 0;
      }
      else {
         cmn_err(CE_CONT, "get_mm_map(): exhaustive search returning 0x%lx\n",
                 mm_map_address);
      }
   }

   kptr_t *mm_map_ptr = (kptr_t*)mm_map_address;
   kptr_t result = *mm_map_ptr;

   cmn_err(CE_CONT, "get_mm_map() returning 0x%lx for mm_map\n", result);
   
   return (caddr_t)result;
}

caddr_t get_mm_map() {
   static caddr_t mm_map_cachedvalue = 0;
   if (mm_map_cachedvalue == 0)
      mm_map_cachedvalue = get_mm_map_lowlevel();
      // could set mm_map_cachedvalue to zero, if an error resulted

   return mm_map_cachedvalue;
}
