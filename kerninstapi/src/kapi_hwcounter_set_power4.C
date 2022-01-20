#include "instrumenter.h"
#include "kapi.h"

//------------------------------------------------------------------------


std::set <kapi_hwcounter_kind> kapi_hwcounter_set::active_metrics;

#define NUM_EVENT_GROUPS 8
typedef struct ppc64_event {
   kapi_hwcounter_kind id;
   char *name;
   char *description;
   signed ctr_at_grp[NUM_EVENT_GROUPS];
} ppc64_event_t;
//ctr_at_grp member maps event to a counter in a given group.
//set to -1 if event does not exist within the group
//e.g. power4_events[PROC_CYCLES].ctr_at_grp[3] will give 
//the counter that contains processor cycles in group 3.


typedef struct ppc64_event_setting {
   unsigned grp_num;
   unsigned ctr_num;
} ppc64_event_setting_t;

enum ppc64_event_id {  RUN_CYCLES=0, PROC_CYCLES,
                       INST_CMPL, DL1_MISS, DL2_INV, INST_DISP,
                       DL1_STORE,DL1_LOAD,DL3_LOAD, MEM_LOAD,
                       DL3_LOAD_MCM, DL2_LOAD, DL2_SHR,
                       DL2_SHR_MCM, DL2_MOD, DL2_MOD_MCM, DL1_VALID,
                       DL2_VALID, MEM_INST, DL2_INST,
                       DL2_INST_MCM, DL3_INST, DL3_INST_MCM, DL1_INST, 
                       PREFETCH_INST, NO_INST, NULL_EVENT
 };

const ppc64_event_t power4_events [] = {
   { HWC_NONE, "None", "None", {-1, -1, -1, -1, -1, -1, -1, -1 } },
   { HWC_TICKS, "Elapsed cycles", "Elapsed cycles", 
     {-1, -1, -1, -1, -1, -1, -1, -1} 
   },
   { HWC_RUN_CYCLES, "Run Cycles",  "Processor Cycles gated by the run latch", 
     {0, -1, -1, -1, -1, -1, -1, -1} 
   },
   { HWC_PROCESSOR_CYCLES, "Cycles", "Processor Cycles",
     {1, 1, -1, 2, 5, -1, 1, 6  } 
   },
   { HWC_INSTRUCTIONS_COMPLETED, "Instructions completed", 
     "Number of completed instructions",
     {-1, 0, -1, -1, 7, -1, 0, 7 }
   },
   { HWC_L1_DATA_MISS, "Data Miss L1", 
     "Total load references that miss first level data cache", 
     {-1, 2, -1, -1, -1, -1, -1, -1 }
   },
   { HWC_L2_DATA_INVALIDATION, "Data Invalidation L2", 
     "A Dcache invalidated was received from the L2 because a line in L2 was castout" ,
     {-1, 3, -1, -1, -1, -1, -1, -1 }
   },
   { HWC_INSTRUCTIONS_DISPATCHED, "Instructions dispatched","Number of instructions dispatched",
     {-1, 4, -1, -1, -1, -1, -1, -1 } 
   },
   { HWC_L1_DATA_STORE, "Store references L1", 
     "Total number of first-level data cache store references",
     {-1, 6, -1, -1, -1, -1, -1, -1 }
   },
   { HWC_L1_DATA_LOAD, "Load references L1",
     "Total number of first-level data cache load references",
     {-1, 7, -1, -1, -1, -1, -1, -1 }
   },
   { HWC_L3_DATA_LOAD, "Demand load L3", 
     "Data loaded from L3 cache due to a demand load", 
     { -1, -1, 0, -1, 0, -1, -1, -1 } 
   },
   { HWC_DATA_MEMORY_LOAD, "Demand load from memory",
     "Data loaded from MEM due to a demand load",
     {-1, -1, 1, -1, 1, -1, -1, -1 }
   },
   { HWC_L3_DATA_LOAD_MCM, "Load from L3 of another MCM", 
     "Data loaded from L3 cache of another multi-chip module",
     {-1, -1, 2, -1, 2, -1, -1, -1 }
   },
   { HWC_L2_DATA_LOAD, "Demand load L2",
     "Data loaded from L2 cache due to a demand load",
     {-1, -1, 3, 3, 3, -1, -1, -1 }
   },
   { HWC_L2_DATA_LOAD_SHARED, "Shared L2 load", 
     "Data reloaded with shared (T) data from the L2 of a chip on this multi-chip module",
     { -1, -1, 4, 4, -1, -1, -1, -1 },
   },

   {HWC_L2_DATA_LOAD_MCM, "Shared load from another MCM",
    "Data reloaded with shared (T) data from another multi-chip unit",
    {-1, -1, 5, 5, -1, -1 , -1 , -1 }
   },
   {HWC_L2_DATA_LOAD_MODIFIED, "Modified L2 load", 
    "Data reloaded with modified (M) data from the L2 of a chip on this multi-chip module",
    {-1, -1, 6, 6, 6, -1, -1, -1 }
   },
   {HWC_L2_DATA_LOAD_MODIFIED_MCM, "Modified load from another MCM",
    "Data reloaded with modified (M) data from another multi-chip module due to a demand load",
    {-1, -1, 7, -1, -1, -1, -1, -1 }
   },
   {HWC_L1_DATA_VALID, "Data source information is valid L1",
    "Data source for an L1 data cache load is valid", 
    {-1, -1, -1, -1, 4, -1, -1, -1 }  
   },
   {HWC_L2_DATA_VALID, "Data source information is valid L2",
    "Data source for an L2 data cache load is valid", 
    {-1, -1, -1, 1, -1, -1, -1, -1 }  
   },
   {HWC_MEMORY_INSTRUCTIONS_LOAD, "Instructions from memory", "Instructions fetched from memory",
    {-1, -1, -1, -1, -1, 0, -1, 0 }
   },
   {HWC_L2_INSTRUCTIONS_LOAD, "L2 Instructions", "Instructions fetched from L2",
    {-1, -1, -1, -1, -1, 2, 2, 2 }
   },
   {HWC_L2_INSTRUCTIONS_LOAD_MCM, "Instructions from L2 MCM", 
    "Instructions fetched from L2 of another multi-chip module",
    {-1, -1, -1, -1, -1, 1, 1, 1}
   },
   {HWC_L3_INSTRUCTIONS_LOAD, "L3 Instructions", "Instructions fetched from L3",
    {-1, -1, -1, -1, -1, 4, 4, 4}
   },
   {HWC_L3_INSTRUCTIONS_LOAD_MCM, "Instructions from L3 MCM", 
    "Instructions fetched from L3 of another multi-chip module",
    {-1,-1, -1, -1, -1, 3, 3, 3 }
   },
   {HWC_L1_INSTRUCTIONS_LOAD, "L1 Instructions", "Instructions fetched from L1", 
    {-1, -1, -1, -1, -1, 5, 5, 5 }
   },
   {HWC_PREFETCHED_INSTRUCTIONS, "Prefetched instructions", 
    "Instructions fetched from prefetch",
    {-1, -1, -1, -1, -1, 6, 6, -1 }
   },
   {HWC_NO_INSTRUCTIONS, "No instructions", 
    "No instructions fetched this cycle due to IFU hold, redirect, or I-cache miss",
    {-1, -1, -1, -1, -1, 7, 7, -1 }
   }, 
   {HWC_MAX_EVENT, "Null", "non-existent/invalid event used as a placeholder", 
    {-1, -1, -1, -1, -1, -1, -1, -1}
   } 
};

const kapi_hwcounter_set::ppc64_event_group_t power4_event_groups [] = {
   {0, 0x00000D0E, 0x000000004A5675AC, 0x00022001 },
   {1, 0x0000090E, 0x1003400045F29420, 0x00002001 },
   {2, 0x00000E1C, 0x0010C000739CE738, 0x00002001 },
   {3, 0x00000938, 0x0010C0003B9CE738, 0x00002001 },
   {4, 0x00000E1C, 0x0010C00073B87724, 0x00002001 },
   {5,  0x00000F1E, 0x800000007BDEF7BC, 0x00002201 },
   {6,  0x0000090E, 0x800000007BDEF7BC, 0x00002201 },
   {7, 0x00000F1E, 0x800000007BDEF3A4, 0x00002201 }
};

//------------------------------------------------------------------------

// Create an empty counter set
kapi_hwcounter_set::kapi_hwcounter_set()
{
   
   this->state = (power4_event_groups)[0]; //default group
}

//This returns the group that is "best fit", i.e. it includes the new
//metric AND rejects the least number of currently active metrics.  
//The second element of the pair is the list of rejected metrics
pair<int, std::set<kapi_hwcounter_kind> >findBestGroup(
                           kapi_hwcounter_kind new_metric, 
                           const std::set<kapi_hwcounter_kind> &active_metrics,
                           int curr_group) {
   int best_group = -1;
   unsigned num_metrics_in_best_group = 0;
   std::set<kapi_hwcounter_kind> rejected_metrics;
   if (  power4_events[new_metric].ctr_at_grp[curr_group]  != -1 )
      return pair <int, std::set<kapi_hwcounter_kind> >(curr_group, rejected_metrics);
 
   for (int grp = 0 ; grp < NUM_EVENT_GROUPS; grp++) { 
       if (  power4_events[new_metric].ctr_at_grp[grp]  != -1 ) {
          unsigned compat_metrics = 0;
          std::set<kapi_hwcounter_kind>::const_iterator am_end = active_metrics.end();
          std::set<kapi_hwcounter_kind>::const_iterator am_iter = active_metrics.begin();
          for (; am_iter != am_end; am_iter++) {
              kapi_hwcounter_kind curr_metric = *am_iter;
              if (  power4_events[curr_metric].ctr_at_grp[grp]  != -1 ) 
                 compat_metrics++; 
          }
          if (compat_metrics == active_metrics.size() )
              return pair <int, std::set<kapi_hwcounter_kind> >(grp, rejected_metrics);
          if ( (compat_metrics > num_metrics_in_best_group) ||
               (best_group == -1) )
             best_group = grp; 
             num_metrics_in_best_group = compat_metrics;
      }
   }
   std::set<kapi_hwcounter_kind>::const_iterator am_end = active_metrics.end();
   std::set<kapi_hwcounter_kind>::const_iterator am_iter = active_metrics.begin();
   for (; am_iter != am_end; am_iter++) {
      kapi_hwcounter_kind curr_metric = *am_iter;
      if (  power4_events[curr_metric].ctr_at_grp[best_group]  == -1 )
         rejected_metrics.insert(curr_metric);   
   }
    
   return pair <int, std::set<kapi_hwcounter_kind> >(best_group, rejected_metrics);
   

}
    
// Insert a counter in the set. If its slot is already occupied,
// force our counter in this slot. 
//NOTE: unlike other platforms, we return HWC_NONE if nothing was
//forced out, or an arbitrary other value if we forced things out,
//i.e. if we switched to a new group.  Eventually, we'll keep
//track of usable events, and return a sensible value 
//(still we could be forcing out multiple events at a time...)
kapi_hwcounter_kind kapi_hwcounter_set::insert(kapi_hwcounter_kind kind)
{
   kapi_hwcounter_kind ret = HWC_NONE;
   if (kind == HWC_TICKS)
      return ret;

   pair<int, std::set<kapi_hwcounter_kind> > grp_and_rejects = 
                          findBestGroup(kind, active_metrics, state.group_num);
   int grp = grp_and_rejects.first;

   assert ( power4_events[kind].ctr_at_grp[grp] != -1 );

   state = power4_event_groups[grp];
   active_metrics.insert(kind);
 
   std::set<kapi_hwcounter_kind> &rejects = grp_and_rejects.second;
   if (rejects.size() > 0) {
      return *(rejects.begin()); 
   }   
   return ret;
}


// Check to see that the supplied kind does not conflict with the 
// current settings (i.e., it's still enabled)
bool kapi_hwcounter_set::conflictsWith(kapi_hwcounter_kind kind) const
{
   pair<int, std::set<kapi_hwcounter_kind> > grp_and_rejects = 
                         findBestGroup(kind, active_metrics, state.group_num);

   return  (  grp_and_rejects.second.size() > 0 );
}

// Check to see if two sets are equal
bool kapi_hwcounter_set::equals(const kapi_hwcounter_set &hwset) const
{
   return (hwset.state.group_num == state.group_num);
}


// Find an enabled counter for this kind of counter, assert if not found
unsigned kapi_hwcounter_set::findCounter(kapi_hwcounter_kind kind, 
                                         bool must_find) const
{
   int ctr_num = power4_events[kind].ctr_at_grp[state.group_num];
   if(must_find && (ctr_num == -1)) {
      cerr << "kapi_hwcounter_set::findCounter(" << kind 
	   << ") - no counter of this kind enabled\n";
      assert(false);
   }
   if (ctr_num == -1) 
      return (unsigned) - 1;
   return ctr_num+1;//counters are actually numbered from 1, not 0
}


// Free a slot occupied by the counter
void kapi_hwcounter_set::free(kapi_hwcounter_kind kind)
{
   if ( (kind == HWC_TICKS) || (kind == HWC_NONE) )
      return;
   std::set<kapi_hwcounter_kind>::iterator am_iter;
   std::set<kapi_hwcounter_kind>::iterator am_end = active_metrics.end();
   am_iter = active_metrics.find(kind);
   assert( (am_iter != am_end) && "freeing inactive metric");
   active_metrics.erase(am_iter);
}

static int matchEventGroup(pdvector<uint64_t> &curr_state) {
   for (int grp=0;grp<NUM_EVENT_GROUPS;grp++) {
      if ( (curr_state[0] == power4_event_groups[grp].mmcr0) &&
           (curr_state[1] == power4_event_groups[grp].mmcr1) &&
           (curr_state[2] == power4_event_groups[grp].mmcra) )
         return grp;
   }
   return -1;

}

extern instrumenter *theGlobalInstrumenter;

// Find what counters are enabled in the hardware
ierr kapi_hwcounter_set::readConfig()
{
   assert(theGlobalInstrumenter != 0);
   
   pdvector<uint64_t> curr_state(3); //an array containing mmcr0, mmcr1, mmcra 
   curr_state = theGlobalInstrumenter->getPowerPerfCtrState();
   
   int curr_group = matchEventGroup(curr_state);
   if (curr_group != -1)
      state = power4_event_groups[curr_group];
   
   return 0;
}

// Do the actual programming of the counters
ierr kapi_hwcounter_set::writeConfig()
{
   assert(theGlobalInstrumenter != 0);
   pdvector <uint64_t> curr_state(3);  
   curr_state = theGlobalInstrumenter->getPowerPerfCtrState();
   unsigned curr_group = matchEventGroup(curr_state);

   if (curr_group != state.group_num) {
      pdvector<uint64_t> new_state(3);
      new_state[0] =  state.mmcr0;
      new_state[1] = state.mmcr1;
      new_state[2] = state.mmcra;
      theGlobalInstrumenter->setPowerPerfCtrState(new_state);
   }
   return 0;
}
