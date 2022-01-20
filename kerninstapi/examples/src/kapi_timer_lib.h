/* kapi_timer_lib.h - user library to encapsulate kerninstapi handling of 
                      virtual and wall timers
*/

#ifndef _KAPI_TIMER_LIB_
#define _KAPI_TIMER_LIB_

#if !defined(i386_unknown_linux2_4) && !defined(ppc64_unknown_linux2_4) && !defined(sparc_sun_solaris2_7)
# error "a valid platform must be defined"
#endif

#include "kapi.h"
#include "vtimer.h" // in kerninst driver src directory

using namespace std;

/* ---------------- BEGIN INTERNAL LIBRARY DEFINITIONS ---------------- */

// Reference to the kapi_manager for use by library routines
kapi_manager *kapi_mgr;

// Addresses of driver context-switch handling routines
kptr_t kerninst_on_switch_in_addr, kerninst_on_switch_out_addr;

// Request id's for inserted calls to switch-in and switch-out routines
kapi_vector<int> callInReqid, callOutReqid;

// Stride between vtimer cpu elements as expected by driver context-switch
// handling routines
const unsigned bytesPerStride = 64;
const unsigned log2_cpu_stride = 6; // 2^6 == 64

// Number of cpus for current machine where kerninstd is running
unsigned machineNumCpus = 0;

// Record-keeping structures for vtimers. If a vtimer is located at index i
// in vtimer_ids, its corresponding support data items are at index i of 
// the appropriate vector
kapi_vector<int> vtimer_ids;
kapi_vector<kptr_t> vtimer_addrs;
kapi_vector< kapi_vector<int> > vtimer_start_ids;
kapi_vector< kapi_vector<int> > vtimer_stop_ids;
kapi_vector<int> vtimer_sample_ids;
kapi_vector<int> vtimer_types;

// monotonically increasing vtimer id generator
unsigned vtimer_id_counter = 0;

ierr initialize_vtimer(kptr_t vtimerAddrInKernel,
                       unsigned counter_type,
                       bool wall_timer = false)
{
   struct vtimer vt;
    
   vt.total_field = 0;
   vt.start_field.rcount = 0;
   vt.start_field.start = 0;
   vt.stop_routine = 0; // currently unused
   vt.restart_routine = 0; // currently unused
   vt.counter_type = counter_type;
   if(wall_timer)
      vt.counter_type |= WALL_TIMER;

   for(unsigned i=0; i<machineNumCpus; i++) {
       if(kapi_mgr->memcopy(&vt, vtimerAddrInKernel + (i * bytesPerStride),
                            sizeof_vtimer) < 0) {
          cerr << "ERROR: initialize_vtimer - memcopy to kernel failed\n";
          return kapi_manager::poke_failed;
       }
   }

   return 0;
}

// Return a snippet that computes (cpu_id * cpu_stride)
kapi_arith_expr computeCpuOffset()
{
    return kapi_arith_expr(kapi_shift_left, kapi_cpuid_expr(),
			   kapi_const_expr(log2_cpu_stride));
}

kapi_snippet generateVTimerStop(kptr_t vtimerAddr, kapi_hwcounter_kind type)
{
   kapi_snippet cpuOffset(computeCpuOffset());
   kapi_arith_expr start_addr(kapi_plus, kapi_const_expr(vtimerAddr),
                              cpuOffset);

   kapi_int64_variable total_field(total_field_offset, start_addr);
   kapi_int64_variable rcount_and_start(start_field_offset, start_addr);

   kapi_arith_expr old_rcount(kapi_shift_right, rcount_and_start,
                              kapi_const_expr(48));
   kapi_arith_expr old_start(kapi_shift_right, 
                             kapi_arith_expr(kapi_shift_left, 
                                             rcount_and_start,
                                             kapi_const_expr(16)),
                             kapi_const_expr(16));

   kapi_arith_expr current_time(kapi_shift_right, 
                                kapi_arith_expr(kapi_shift_left, 
						kapi_hwcounter_expr(type),
                                                kapi_const_expr(16)),
                                kapi_const_expr(16));

#if defined(sparc_sun_solaris2_7)
   // Compute the delta and truncate it to 32 bits. This approach
   // lets us handle %pic wraparound: the delta is computed correctly
   // even if %pic wrapped around, thanks to the wonders of the
   // 2-bit-complement arithmetic. NOTE: it may break delta for
   // full-64-bit registers like %tick -- won't be able to measure
   // long-running functions which delta does not fit in 32 bits.
   kapi_arith_expr time_delta(kapi_shift_right,
                              kapi_arith_expr(kapi_shift_left,
                                              kapi_arith_expr(kapi_minus,
                                                              current_time,
                                                              old_start),
                                              kapi_const_expr(32)),
                              kapi_const_expr(32));
#else
   kapi_arith_expr time_delta(kapi_minus, current_time, old_start);
#endif

   kapi_arith_expr new_rcount(kapi_minus, old_rcount, kapi_const_expr(1));
   kapi_arith_expr new_shifted_rcount(kapi_shift_left, new_rcount, 
                                      kapi_const_expr(48));
   kapi_bool_expr new_rcount_is_zero(kapi_eq, new_rcount, kapi_const_expr(0));

   kapi_if_expr new_start(new_rcount_is_zero, kapi_const_expr(0), old_start);

   kapi_arith_expr new_rcount_and_start(kapi_bit_or, 
                                        new_shifted_rcount,
                                        new_start);

   kapi_arith_expr update_control(kapi_assign, rcount_and_start,
                                  new_rcount_and_start);

   kapi_if_expr update_total(new_rcount_is_zero, 
                             kapi_arith_expr(kapi_assign,
                                             total_field,
                                             kapi_arith_expr(kapi_plus,
                                                             total_field,
                                                             time_delta)));
   
   kapi_vector<kapi_snippet> vUpdates;
   vUpdates.push_back(time_delta);
   vUpdates.push_back(update_control);
   vUpdates.push_back(update_total);
   kapi_sequence_expr updates(vUpdates);
    
   kapi_bool_expr old_rcount_non_zero(kapi_ne, old_rcount, 
                                      kapi_const_expr(0));
   kapi_if_expr code(old_rcount_non_zero, updates);

   // Finally, wrap the code with lock/unlock to protect from migration
   // in the middle of the primitive
   return kapi_mgr->getPreemptionProtectedCode(code);
}

kapi_snippet generateVTimerStart(kptr_t vtimerAddr, kapi_hwcounter_kind type)
{
   kapi_snippet cpuOffset(computeCpuOffset());
   kapi_int64_variable rcount_and_start(vtimerAddr + start_field_offset,
                                        cpuOffset);
   kapi_arith_expr old_rcount(kapi_shift_right, rcount_and_start,
                              kapi_const_expr(48));
   kapi_arith_expr new_rcount(kapi_plus, old_rcount, kapi_const_expr(1));
   kapi_arith_expr new_shifted_rcount(kapi_shift_left, new_rcount, 
                                      kapi_const_expr(48));

   kapi_arith_expr old_start(kapi_shift_right, 
                             kapi_arith_expr(kapi_shift_left, 
                                             rcount_and_start,
                                             kapi_const_expr(16)),
                             kapi_const_expr(16));
   kapi_arith_expr new_start(kapi_shift_right, 
                             kapi_arith_expr(kapi_shift_left, 
                                             kapi_hwcounter_expr(type),
                                             kapi_const_expr(16)),
                             kapi_const_expr(16));
    
   kapi_if_expr new_rcount_and_start(kapi_bool_expr(kapi_gt, old_rcount,
                                                    kapi_const_expr(0)),
                                     kapi_arith_expr(kapi_bit_or, 
                                                     new_shifted_rcount,
                                                     old_start),
                                     kapi_arith_expr(kapi_bit_or, 
                                                     new_shifted_rcount,
                                                     new_start));
   
   kapi_arith_expr code(kapi_assign, rcount_and_start,
                        new_rcount_and_start);
    
   // Finally, wrap the code with lock/unlock to protect from migration
   // in the middle of the primitive
   return kapi_mgr->getPreemptionProtectedCode(code);
}

ierr instrument_switch_points(kapi_point_location type,
			      kapi_vector<int> *pCallReqids)
{
   kapi_vector<kapi_point> cswitchPoints;
   ierr rv = kapi_mgr->findSpecialPoints(type, &cswitchPoints);
   if(rv < 0) {
      cerr << "ERROR: instrument_switch_points - findSpecialPoints failure\n";
      return rv;
   }

   kptr_t cswitchin_addr;
   if(type == kapi_cswitch_in)
      cswitchin_addr = kerninst_on_switch_in_addr;
   else
      cswitchin_addr = kerninst_on_switch_out_addr;
   
   kapi_vector<kapi_snippet> empty;
   kapi_call_expr my_call(cswitchin_addr, empty);

   kapi_vector<kapi_point>::const_iterator ipoint = cswitchPoints.begin();
   for(; ipoint != cswitchPoints.end(); ipoint++) {
      int id = kapi_mgr->insertSnippet(my_call, *ipoint);
      if(id < 0) {
         cerr << "ERROR: instrument_switch_points - insertSnippet failure\n";
         return id;
      }
      pCallReqids->push_back(id);
   }
   
   return 0;
}

ierr deinstrument_switch_points(const kapi_vector<int> &call_reqid)
{
   kapi_vector<int>::const_iterator icall = call_reqid.begin();
   for(; icall != call_reqid.end(); icall++) {
      ierr rv = kapi_mgr->removeSnippet(*icall);
      if(rv < 0) {
         cerr << "ERROR: deinstrument_switch_points - removeSnippet failure\n";
         return rv;
      }
   }
   return 0;
}

#ifdef sparc_sun_solaris2_7
extern bool usesPic(kapi_hwcounter_kind kind, int num);
#endif

/* -------------------------------------------------------------------- */

/* ----------------------- BEGIN USER-INTERFACE ----------------------- */

// Set the following to true if you want verbose output
bool verbose_timer_lib = false;

// Default sample handler
int default_sample_callback(unsigned reqid, uint64_t time, 
                            const uint64_t *values, unsigned numvalues)
{
    cout << "@ Time = " << time << " cycles\n";
    for(int i = 0; i<numvalues; i++)
       cout << "   CPU " << kapi_mgr->getIthCPUId(i) 
            << ": total = " << values[i] << endl;
}

// Initialize the vtimer environment, including inserting the calls to the
// context-switch handling routines in the kerninst driver
ierr initialize_vtimer_environment(kapi_manager &the_mgr)
{
   kapi_mgr = &the_mgr;

   if(verbose_timer_lib)
      cout << "Initializing vtimer environment\n";

   // Get number of cpus for the kerninstd machine
   machineNumCpus = kapi_mgr->getMaxCPUId() + 1;
   if(machineNumCpus == 0) {
      cerr << "ERROR: initialize_vtimer_environment - invalid number of cpus\n";
      return kapi_manager::internal_error;
   }

   // Find the vtimer context-switch routines in kerninst module
   if(verbose_timer_lib)
      cout << "- finding context-switch handlers in kerninst driver\n";

   kapi_module kerninst_mod;
   ierr rv = kapi_mgr->findModule("kerninst", &kerninst_mod);
   if(rv < 0) {
      cerr << "ERROR: initialize_vtimer_environment - unable to locate module 'kerninst'\n";
      return rv;
   }

   kapi_function kerninst_switchin;
   rv = kerninst_mod.findFunction("on_switch_in", &kerninst_switchin);
   if(rv < 0) {
      cerr << "ERROR: initialize_vtimer_environment - unable to locate function 'on_switch_in' in module 'kerninst'\n";
      return rv;
   }

   kapi_vector<kapi_point> kerninst_switchin_entries;
   rv = kerninst_switchin.findEntryPoint(&kerninst_switchin_entries);
   if(rv < 0) {
      cerr << "ERROR: initialize_vtimer_environment - failed to find entry point for function 'on_switch_in' in module 'kerninst'\n";
      return rv;
   }
   kerninst_on_switch_in_addr = kerninst_switchin_entries[0].getAddress();
   
   kapi_function kerninst_switchout;
   rv = kerninst_mod.findFunction("on_switch_out", &kerninst_switchout);
   if(rv < 0) {
      cerr << "ERROR: initialize_vtimer_environment - unable to locate function 'on_switch_out' in module 'kerninst'\n";
      return rv;
   }
   
   kapi_vector<kapi_point> kerninst_switchout_entries;
   rv = kerninst_switchout.findEntryPoint(&kerninst_switchout_entries);
   if(rv < 0) {
      cerr << "ERROR: initialize_vtimer_environment - failed to find entry point for function 'on_switch_out' in module 'kerninst'\n";
      return rv;
   }
   kerninst_on_switch_out_addr = kerninst_switchout_entries[0].getAddress();

   // Insert calls to context-switch routines
   if(verbose_timer_lib)
      cout << "- inserting calls to handlers at context-switch points\n";

   rv = instrument_switch_points(kapi_cswitch_in, &callInReqid);
   if(rv < 0) {
      cerr << "ERROR: initialize_vtimer_environment - instrument_switch_points(kapi_cswitch_in) failure\n";
      return rv;
   }
   rv = instrument_switch_points(kapi_cswitch_out, &callOutReqid);
   if(rv < 0) {
      cerr << "ERROR: initialize_vtimer_environment - instrument_switch_points(kapi_cswitch_out) failure\n";
      return rv;
   }

   return 0;
}

void remove_vtimers(const kapi_vector<int> &vtimer_id_list);

// Allocate and add a vtimer to the in-kernel list, insert start & stop 
// instumentation at the supplied points, and start sampling (if requested).
// Returns a vtimer_id on success, or a negative error code
int add_vtimer(kapi_hwcounter_kind type,
               const kapi_vector<kapi_point> &start_points,
               const kapi_vector<kapi_point> &stop_points,
               bool is_wall,
               data_callback_t sample_routine = NULL, 
               unsigned sample_freq_ms = 1000)
{
   if(verbose_timer_lib)
      cout << "Adding a " << (is_wall ? "wall timer" : "virtual timer")
           << endl;

   if(type == HWC_NONE) {
      cerr << "WARNING: ignoring request for timer type HWC_NONE\n";
      return kapi_manager::unsupported_hwcounter;
   }

   if(type != HWC_TICKS) {
      if(is_wall) {
         // wall timers not supported for anything but HWC_TICKS
         is_wall = false; 
         cerr << "WARNING: add_vtimer - request for wall timer ignored since not supported for supplied kapi_hwcounter_kind\n";
         return kapi_manager::unsupported_hwcounter;
      }

#ifdef i386_unknown_linux2_4
      // hackish method to get cpu family (.first) and model (.second) info
      kapi_vector<unsigned> cpu_model_info = kapi_mgr->getABIs();
      assert(cpu_model_info.size() == 2);

      if(cpu_model_info[0] != 15) { // P4 or Xeon
         cerr << "WARNING: add_vtimer - request for timer type "
              << type << " ignored since not supported on current machine\n";
         return kapi_manager::unsupported_hwcounter;
      }
      else {
         if((cpu_model_info[1] != 3) && (type == HWC_INSTR_RETIRED)) {
            cerr << "WARNING: add_vtimer - request for timer type HWC_INSTR_RETIRED ignored since not supported on current machine (model encoding must be 3). We suggest using HWC_INSTR_ISSUED instead.\n";
            return kapi_manager::unsupported_hwcounter;
	 }
      }
#endif
   }

   // Generate unique vtimer id
   int vt_id = vtimer_id_counter++;
   if(verbose_timer_lib)
      cout << "- timer id is " << vt_id << endl;

   // Update performance counter settings on kerninstd machine
   kapi_hwcounter_set newPcrVal;
   if(type != HWC_TICKS) {
      if(verbose_timer_lib)
         cout << "- updating performance counter settings on kerninstd machine\n";
      kapi_hwcounter_set oldPcrVal;
      if(oldPcrVal.readConfig() < 0) {
         cerr << "ERROR: add_vtimer - unable to read performance counter settings\n";
         return kapi_manager::internal_error;
      }
      newPcrVal = oldPcrVal;
      newPcrVal.insert(type);

      kapi_vector<int> disable_timers;
      for(unsigned i=0; i < vtimer_types.size(); i++) {
         if(vtimer_types[i] != -1) {
            if(newPcrVal.conflictsWith((kapi_hwcounter_kind)vtimer_types[i])) {
               cout << "WARNING: add_vtimer - disabling timer with id "
                    << vtimer_ids[i] 
                    << " since its type conflicts with newly enabled performance counter settings\n";
               disable_timers.push_back(vtimer_ids[i]);
            }
         }
      }
      if(disable_timers.size()) {
         bool tmp_verbose = verbose_timer_lib;
         verbose_timer_lib = false; // don't be verbose when removing conflicts
         remove_vtimers(disable_timers);
         verbose_timer_lib = tmp_verbose;
      }

      if(!newPcrVal.equals(oldPcrVal)) {
         if(newPcrVal.writeConfig() < 0) {
            cerr << "ERROR: add_vtimer - unable to write new performance counter settings\n";
            return kapi_manager::internal_error;
         }
      }
   }
   
   // Annotate the vtimer type with performance counter info
   unsigned vtimerType = 0;
   vtimerType = (unsigned)type;
   if(type != HWC_TICKS) {
#if defined(i386_unknown_linux2_4)  || defined(ppc64_unknown_linux2_4)
      // Set counter portion of vtimerType to perfctr number
      unsigned perfctr_num = newPcrVal.findCounter(type);
      vtimerType |= (perfctr_num << 16);
#elif defined(sparc_sun_solaris2_7)
      // Set counter portion of vtimerType to pic0/1
      if(usesPic(type, 1))
         vtimerType |= (1 << 16);
#endif
   }

   // Allocate the vtimer. Need to account for striding of per-cpu timers
   unsigned numbytes = bytesPerStride*machineNumCpus;
   kptr_t vt_addr = kapi_mgr->malloc(numbytes);
   if(vt_addr == 0) {
      cerr << "ERROR: add_vtimer - malloc(" << numbytes << ") failure\n";
      return kapi_manager::alloc_failed;
   }
   if(verbose_timer_lib)
      cout << "- timer allocated @ kernel address 0x" << std::hex 
           << vt_addr << std::dec << endl;

   // Initialize the vtimer in kernel memory
   if(verbose_timer_lib)
      cout << "- initializing timer value in memory\n";
   ierr rv = initialize_vtimer(vt_addr, vtimerType, is_wall);
   if(rv < 0) {
      cerr << "ERROR: add_vtimer - initialize_vtimer failure\n";
      return rv;
   }

   // Add the vtimer to in-kernel list
   if(verbose_timer_lib)
      cout << "- adding timer to list of timers in kerninst driver\n";   
   kapi_mgr->addTimerToAllVTimers(vt_addr);

   // Insert vtimer stop instrumentation
   if(verbose_timer_lib)
      cout << "- inserting timer stop instrumentation\n";   
   kapi_vector<int> stop_ids;
   kapi_snippet stop_expr = generateVTimerStop(vt_addr, (kapi_hwcounter_kind)vtimerType);
   kapi_vector<kapi_point>::const_iterator ptiter = stop_points.begin();
   for (; ptiter != stop_points.end(); ptiter++) {
      int id = kapi_mgr->insertSnippet(stop_expr, *ptiter);
      if (id < 0) {
         cerr << "ERROR: add_vtimer - stop instrumentation insertSnippet failure\n";
         return id;
      }
      stop_ids.push_back(id);
   }

   // Insert vtimer start instrumentation
   if(verbose_timer_lib)
      cout << "- inserting timer start instrumentation\n";   
   kapi_vector<int> start_ids;
   kapi_snippet start_expr = generateVTimerStart(vt_addr, (kapi_hwcounter_kind)vtimerType);
   ptiter = start_points.begin();
   for (; ptiter != start_points.end(); ptiter++) {
      int id = kapi_mgr->insertSnippet(start_expr, *ptiter);
      if (id < 0) {
         cerr << "ERROR: add_vtimer - start instrumentation insertSnippet failure\n";
         return id;
      }
      start_ids.push_back(id);
   }


   // Start sampling (if requested)
   int sample_id = -1;
   if(sample_routine != NULL) {
      if(verbose_timer_lib)
         cout << "- initiating sampling of timer\n";   
   
      unsigned real_num_cpus = kapi_mgr->getNumCPUs();
      kapi_mem_region *regions = (kapi_mem_region*)malloc(sizeof(kapi_mem_region)*real_num_cpus);
      for(unsigned k=0; k<real_num_cpus; k++) {
         unsigned phys_id = kapi_mgr->getIthCPUId(k);
         kptr_t timer_addr = vt_addr + (phys_id << log2_cpu_stride);
         kapi_mem_region *mr = regions + k;
         mr->addr = timer_addr + total_field_offset;
         mr->nbytes = sizeof(uint64_t);
      }
      sample_id = kapi_mgr->sample_periodically(regions, real_num_cpus, 
                                                sample_routine,
                                                sample_freq_ms);
      if(sample_id < 0) {
         cerr << "ERROR: add_vtimer - sample_periodically failure\n";
         return sample_id;
      }
   }

   vtimer_ids.push_back(vt_id);
   vtimer_addrs.push_back(vt_addr);
   vtimer_stop_ids.push_back(stop_ids);
   vtimer_start_ids.push_back(start_ids);
   vtimer_sample_ids.push_back(sample_id);
   vtimer_types.push_back(type);
   return vt_id;
}

// For each vtimer_id in the list, stop sampling the vtimer, remove its 
// start & stop instrumentation, remove it from the in-kernel list, and
// deallocate it.
void remove_vtimers(const kapi_vector<int> &vtimer_id_list)
{
   ierr rv;
   kapi_vector<unsigned> removed_id_list;

   for(unsigned i=0; i < vtimer_id_list.size(); i++) {

      // ignore deactivated timers (since vtimer_id_list could be vtimer_ids)
      if(vtimer_id_list[i] == -1)
         continue;

      // find id ndx in vtimer_ids
      unsigned j = 0;
      for(; j<vtimer_ids.size(); j++) {
         if(vtimer_ids[j] == vtimer_id_list[i])
            break;
      }
      if(j == vtimer_ids.size()) {
         cerr << "WARNING: remove_vtimers - could not find active vtimer with id "
              << vtimer_id_list[i] << ", ignoring\n";
         continue;
      }

      if(verbose_timer_lib)
         cout << "Removing timer with id " << vtimer_ids[j] << endl;   
         
      // stop sampling
      int sample_req = vtimer_sample_ids[j];
      if(sample_req != -1) {
         if(verbose_timer_lib)
            cout << "- stopping sampling for timer\n";
         rv = kapi_mgr->stop_sampling(sample_req);
         if(rv < 0) {
            cerr << "ERROR: remove_vtimers - stop_sampling(" << sample_req
                 << ") failure, retval is " << rv << endl;
         }
         vtimer_sample_ids[j] = -1;
      }
      
      // remove start instrumentation
      if(verbose_timer_lib)
         cout << "- removing timer start instrumentation\n";
      const kapi_vector<int> &start_reqs = vtimer_start_ids[j];
      kapi_vector<int>::const_iterator iter = start_reqs.begin();
      for(; iter != start_reqs.end(); iter++) {
         rv = kapi_mgr->removeSnippet(*iter);
         if(rv < 0) {
	    cerr << "ERROR: remove_vtimers - start instrumentation removeSnippet(" 
                 << *iter << ") failure, retval is " << rv << endl;
         }
      }
      vtimer_start_ids[j].clear();
      
      // remove stop instrumentation
      if(verbose_timer_lib)
         cout << "- removing timer stop instrumentation\n";
      const kapi_vector<int> &stop_reqs = vtimer_stop_ids[j];
      iter = stop_reqs.begin();
      for(; iter != stop_reqs.end(); iter++) {
         rv = kapi_mgr->removeSnippet(*iter);
         if(rv < 0) {
	    cerr << "ERROR: remove_vtimers - stop instrumentation removeSnippet(" 
                 << *iter << ") failure, retval is " << rv << endl;
         }
      }
      vtimer_stop_ids[j].clear();
      removed_id_list.push_back(j);
   }

   // wait 3 seconds before actually removing timers from in-kernel list
   // and de-allocating, since removed start/stop code may still be running
   // when removed
   if(verbose_timer_lib)
      cout << "Pausing 3 seconds to allow just removed timer(s) start/stop code to finish\n"; 
   sleep(3);

   if(verbose_timer_lib)
      cout << "Removing timer(s) from list in kernel driver and de-allocating timer(s) memory\n";   
   for(unsigned k=0; k < removed_id_list.size(); k++) {
      unsigned rvtid = removed_id_list[k];
      // remove the vtimer from in-kernel list
      kptr_t vt_addr = vtimer_addrs[rvtid];
      kapi_mgr->removeTimerFromAllVTimers(vt_addr);
      vtimer_addrs[rvtid] = (kptr_t)-1;
      
      // deallocate the vtimer
      kapi_mgr->free(vt_addr);
      vtimer_ids[rvtid] = -1; // disables this ndx, we don't actually remove
      vtimer_types[rvtid] = -1;
   }
}

// Clean up the vtimer environment, including removing any active vtimers &
// removing the calls to the context-switch handling routines in the driver
ierr cleanup_vtimer_environment(void)
{
   if(verbose_timer_lib)
      cout << "Cleaning up vtimer environment\n";

   // Remove any remaining vtimers
   bool tmp_verbose = verbose_timer_lib;
   if(verbose_timer_lib) {
      cout << "- removing any active timers\n";
      verbose_timer_lib = false; // don't be verbose about leftover timers
   }
   if(vtimer_ids.size() > 0) {
      remove_vtimers(vtimer_ids);

      // Clear the vtimer data structures
      vtimer_ids.clear();
      vtimer_addrs.clear();
      vtimer_stop_ids.clear();
      vtimer_start_ids.clear();
      vtimer_sample_ids.clear();
      vtimer_types.clear();
   }
   verbose_timer_lib = tmp_verbose;

   // Remove calls to context-switch routines
   if(verbose_timer_lib)
      cout << "- removing calls to context-switch handlers in kerninst driver\n";
   ierr rv = deinstrument_switch_points(callOutReqid);
   if(rv < 0) {
      cerr << "ERROR: cleanup_vtimer_environment - deinstrument_switch_points(kapi_cswitch_out) failure\n";
      return rv;
   }
   rv = deinstrument_switch_points(callInReqid);
   if(rv < 0) {
      cerr << "ERROR: cleanup_vtimer_environment - deinstrument_switch_points(kapi_cswitch_in) failure\n";
      return rv;
   }

   return 0;
}

/* -------------------------------------------------------------------- */

#endif // _KAPI_TIMER_LIB_
