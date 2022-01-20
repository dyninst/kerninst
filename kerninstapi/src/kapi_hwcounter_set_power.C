#include "instrumenter.h"
#include "kapi.h"

//------------------------------------------------------------------------

typedef struct component_event {
   unsigned nr_ndx; // non-retirement map index, if applicable, -1 else
   unsigned ar_ndx; // at-retirement map index, if applicable, -1 else
   unsigned base_cccr_val;
   unsigned base_escr_val;
} component_event_t;

typedef struct hwcounter_setting {
   const char *ctr_kind;
   const component_event_t evts[2];
} hwcounter_setting_t;

const unsigned invalid = (unsigned)-1;

#define CCCR_BASE (CCCR_ENABLE | CCCR_ACTIVE_THREAD(0x3))


//------------------------------------------------------------------------

// Create an empty counter set
kapi_hwcounter_set::kapi_hwcounter_set()
{
}

    
// Insert a counter in the set. If its slot is already occupied,
// force our counter in this slot. Return the old value of the slot
kapi_hwcounter_kind kapi_hwcounter_set::insert(kapi_hwcounter_kind kind)
{
  return HWC_NONE;  
}


// Check to see that the supplied kind does not conflict with the 
// current settings (i.e., it's still enabled)
bool kapi_hwcounter_set::conflictsWith(kapi_hwcounter_kind kind) const
{
  assert(false && "conflictsWith() nyi on power");
}

// Check to see if two sets are equal
bool kapi_hwcounter_set::equals(const kapi_hwcounter_set &hwset) const
{
    return true;
}


// Find an enabled counter for this kind of counter, assert if not found
unsigned kapi_hwcounter_set::findCounter(kapi_hwcounter_kind kind, 
                                         bool must_find) const
{
   assert(false && "findCounter() nyi on power");
}

// Free a slot occupied by the counter
void kapi_hwcounter_set::free(kapi_hwcounter_kind kind)
{
}

extern instrumenter *theGlobalInstrumenter;

// Find what counters are enabled in the hardware
ierr kapi_hwcounter_set::readConfig()
{
    return 0;
}

// Do the actual programming of the counters
ierr kapi_hwcounter_set::writeConfig()
{
   return 0;
}
