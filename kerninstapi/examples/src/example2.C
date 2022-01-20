#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "kapi.h"

int usage(char *basename)
{
    cerr << "Usage: " << basename << " hostname portnum\n";
}

kapi_manager kmgr;

// Return a snippet that computes (cpu_id * cpu_stride)
kapi_arith_expr computeCpuOffset()
{
    unsigned log2_cpu_stride = 6;

    return kapi_arith_expr(kapi_shift_left, kapi_cpuid_expr(),
			   kapi_const_expr(log2_cpu_stride));
}

const unsigned total_field_offset = 0;
const unsigned start_field_offset = 8;
const unsigned stop_routine_offset = 16;
const unsigned restart_routine_offset = 24;
const unsigned sizeof_vtimer = 32;

const unsigned saved_timer_address_offset = 0;
const unsigned saved_timer_auxdata_offset = 8;
const unsigned sizeof_saved_timer = 16;

const unsigned max_num_vtimers = 50;
const unsigned sizeof_allvtimers = (max_num_vtimers + 1) * sizeof(kptr_t);
const unsigned all_vtimers_offset_to_finish = max_num_vtimers * sizeof(kptr_t);

kptr_t all_vtimers_create()
{
    kptr_t rv = kmgr.malloc(sizeof_allvtimers);
    assert(rv != 0);

    // Prepare the structure locally
    char *buffer = new char[sizeof_allvtimers];
    assert(buffer != 0);
    memset(buffer, 0, sizeof_allvtimers);
    kptr_t finish = rv; // pointer to the first available entry
    kmgr.to_kernel_byte_order(&finish, sizeof(kptr_t));
    memcpy(&buffer[all_vtimers_offset_to_finish], &finish, sizeof(kptr_t));

    if (kmgr.memcopy(buffer, rv, sizeof_allvtimers) < 0) {
	assert(false);
    }
    delete[] buffer;

    return rv;
}

void all_vtimers_delete(kptr_t all_vtimers_addr)
{
    kmgr.free(all_vtimers_addr);
}

void add_timer_to_all_vtimers(kptr_t all_vtimers_addr, kptr_t this_vtimer_addr)
{
    // Read-in the pointer to the first available spot
    kptr_t finish;
    kmgr.memcopy(all_vtimers_addr + all_vtimers_offset_to_finish,
		 &finish, sizeof(kptr_t));
    kmgr.to_client_byte_order(&finish, sizeof(kptr_t));
    assert(finish >= all_vtimers_addr);
    // assert that we have enough space there
    // notice that the last entry in the list is always 0 -> hence -2
    assert(finish < all_vtimers_addr + all_vtimers_offset_to_finish -
	   2 * sizeof(kptr_t));

    // Write updated finish and this_vtimer_addr to the kernel space
    kptr_t new_finish = finish + sizeof(kptr_t);
    kmgr.to_kernel_byte_order(&new_finish, sizeof(kptr_t));
    kmgr.to_kernel_byte_order(&this_vtimer_addr, sizeof(kptr_t));

    kmgr.memcopy(&new_finish, all_vtimers_addr + all_vtimers_offset_to_finish,
		 sizeof(kptr_t));
    kmgr.memcopy(&this_vtimer_addr, finish, sizeof(kptr_t));
}

void remove_timer_from_all_vtimers(kptr_t all_vtimers_addr,
				   kptr_t this_vtimer_addr)
{
    // Read-in all_vtimers
    kptr_t *buffer = new kptr_t[max_num_vtimers+1]; // +1 for finish

    assert(sizeof_allvtimers == (max_num_vtimers+1) * sizeof(kptr_t));
    if (kmgr.memcopy(all_vtimers_addr, buffer, sizeof_allvtimers) < 0) {
	assert(false);
    }

    int last_used = -1;
    for (; last_used+1 < max_num_vtimers && buffer[last_used+1] != 0; 
	 last_used++);
    assert(last_used >= 0);

    kptr_t finish = buffer[max_num_vtimers];
    kmgr.to_client_byte_order(&finish, sizeof(kptr_t));
    finish -= sizeof(kptr_t);
    assert(finish == all_vtimers_addr + last_used * sizeof(kptr_t));
    kmgr.to_kernel_byte_order(&finish, sizeof(kptr_t));

    // Convert it once, so we can compare raw data in the buffer to it
    kmgr.to_kernel_byte_order(&this_vtimer_addr, sizeof(kptr_t));
    
    // Find the timer to delete, write the last one in its place,
    // write 0 in the last place, update the finish pointer
    for (unsigned i = 0; i < max_num_vtimers && buffer[i] != 0; i++) {
	if (buffer[i] == this_vtimer_addr) {
	    kmgr.memcopy(&buffer[last_used], all_vtimers_addr + 
			 i * sizeof(kptr_t), sizeof(kptr_t));
	    kptr_t zero = 0;
	    kmgr.memcopy(&zero, all_vtimers_addr + 
			 last_used * sizeof(kptr_t), sizeof(kptr_t));
	    kmgr.memcopy(&finish, all_vtimers_addr + 
			 all_vtimers_offset_to_finish, sizeof(kptr_t));
	    return;
	}
    }
    assert(false && "Timer not found");
}

kapi_snippet generateVTimerStop(kptr_t vtimerAddr)
{
    kapi_snippet cpuOffset(computeCpuOffset());
    kapi_arith_expr start_addr(kapi_plus, kapi_const_expr(vtimerAddr),
			       cpuOffset);
    kapi_int_variable total_field(0, start_addr);
    kapi_int_variable rcount_and_start(start_field_offset, start_addr);

    kapi_arith_expr old_rcount(kapi_shift_right, rcount_and_start,
			       kapi_const_expr(48));
    kapi_arith_expr old_start(kapi_shift_right, 
			      kapi_arith_expr(kapi_shift_left, 
					      rcount_and_start,
					      kapi_const_expr(16)),
			      kapi_const_expr(16));

    kapi_arith_expr new_rcount(kapi_minus, old_rcount, kapi_const_expr(1));
    kapi_arith_expr new_shifted_rcount(kapi_shift_left, new_rcount, 
				       kapi_const_expr(48));
    kapi_bool_expr new_rcount_is_zero(kapi_eq, new_rcount, kapi_const_expr(0));

    kapi_if_expr new_start(new_rcount_is_zero, kapi_const_expr(0), old_start);

    kapi_arith_expr new_rcount_and_start(kapi_bit_or, 
					 new_shifted_rcount,
					 new_start);

    kapi_bool_expr old_rcount_non_zero(kapi_ne, old_rcount, 
				       kapi_const_expr(0));
    // The following value corresponds to the new control field if old_rcount 
    // was not zero and to the old control field if old_rcount == 0.
    kapi_if_expr update_switch(old_rcount_non_zero,
			       new_rcount_and_start, rcount_and_start);

    kapi_vector<kapi_snippet> pre_cached_exprs;
    pre_cached_exprs.push_back(rcount_and_start);
    pre_cached_exprs.push_back(old_start);
    pre_cached_exprs.push_back(new_rcount_and_start);
    pre_cached_exprs.push_back(update_switch);
    kapi_sequence_expr update_control_value(pre_cached_exprs);

    kapi_arith_expr update_control(kapi_atomic_assign, rcount_and_start,
				   update_control_value);

    kapi_arith_expr current_time(kapi_shift_right, 
				 kapi_arith_expr(kapi_shift_left, 
						kapi_hwcounter_expr(HWC_TICKS),
						 kapi_const_expr(16)),
				 kapi_const_expr(16));
    kapi_arith_expr time_delta(kapi_minus, current_time, old_start);
    kapi_bool_expr new_is_zero_old_is_not(kapi_log_and, new_rcount_is_zero,
					  old_rcount_non_zero);

    kapi_if_expr update_total(new_is_zero_old_is_not, 
			      kapi_arith_expr(kapi_atomic_assign,
					      total_field,
					      kapi_arith_expr(kapi_plus,
							      total_field,
							      time_delta)));
    kapi_vector<kapi_snippet> vUpdates;
    vUpdates.push_back(update_control);
    vUpdates.push_back(update_total);
    kapi_sequence_expr code(vUpdates);
    
    return code;
}

kapi_snippet generateVTimerStart(kptr_t vtimerAddr)
{
    kapi_snippet cpuOffset(computeCpuOffset());
    kapi_int_variable rcount_and_start(vtimerAddr + start_field_offset,
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
					      kapi_hwcounter_expr(HWC_TICKS),
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

    kapi_arith_expr code(kapi_atomic_assign, rcount_and_start,
			 new_rcount_and_start);

    return code;
}

kapi_snippet generateMetricSpecificVRestart(
    const kapi_snippet &vtimer_addr_base,
    const kapi_snippet &vtimer_aux_data,
    const kapi_snippet &rawCtrExpr)
{
    // We need to set rcount to auxdata and start -- to new %pic/%tick
    // and (maybe) update total

    // Get a pointer to the current rcount/start
    kapi_arith_expr cpuOffset(computeCpuOffset());
    kapi_arith_expr start_addr(kapi_plus, vtimer_addr_base, cpuOffset);
    kapi_int_variable total_field(start_addr); // assume total_field_offset==0
    kapi_int_variable rcount_and_start(start_field_offset, start_addr);

    // Read new %pic/%tick and truncate it to 48 bits
    kapi_arith_expr new_start(kapi_shift_right, 
                              kapi_arith_expr(kapi_shift_left, 
                                              rawCtrExpr,
                                              kapi_const_expr(16)),
                              kapi_const_expr(16));
    // Shift new rcount into place
    kapi_arith_expr new_shifted_rcount(kapi_shift_left, 
				       kapi_arith_expr(kapi_shift_right,
						       vtimer_aux_data,
						       kapi_const_expr(48)),
				       kapi_const_expr(48));
    // Modify rcount/start with OR'ed new_rcount/new_start. No need for cas
    // since we are in the exclusive mode (only one cpu should be trying to
    // activate this thread)
    kapi_arith_expr new_rcount_and_start(kapi_bit_or, new_shifted_rcount,
					 new_start);
    kapi_arith_expr update_start(kapi_assign, rcount_and_start,
				 new_rcount_and_start);
    kapi_vector<kapi_snippet> update_start_total_ret;
    update_start_total_ret.push_back(update_start);

    // We want to include switched-out time for wall timers -- update total
    if (true) {
	// Current situation:
	//   new_rcount_and_start = (rcount, new start)
	//   vtimer_aux_data = (rcount, old stop)
	// Let's compute delta = (0, new start - old stop)
	kapi_arith_expr delta(kapi_minus, new_rcount_and_start,
			      vtimer_aux_data);
	kapi_arith_expr update_total(kapi_assign, total_field,
				     kapi_arith_expr(kapi_plus, total_field,
						     delta));
	update_start_total_ret.push_back(update_total);
    }
    update_start_total_ret.push_back(kapi_ret_expr());
    return kapi_sequence_expr(update_start_total_ret);
}

kapi_snippet generateMetricSpecificVStop(
    const kapi_snippet &vtimer_addr_base,
    const kapi_snippet &/*vtimer_start*/,
    const kapi_snippet &vtimer_iter,
    const kapi_snippet &rawCtrExpr)
{
    // We need to set rcount to auxdata and start -- to new %pic/%tick
    // and (maybe) update total

    // Get a pointer to the current rcount/start
    kapi_arith_expr cpuOffset(computeCpuOffset());
    kapi_arith_expr start_addr(kapi_plus, vtimer_addr_base, cpuOffset);

    kapi_int_variable total_field(start_addr); // assume total_field_offset==0
    kapi_int_variable rcount_and_start(start_field_offset, start_addr);

    // Phase 1: save the timer in the list to restart it on switch-in

    // mask-off the start field
    kapi_arith_expr shifted_rcount(kapi_shift_left, 
				   kapi_arith_expr(kapi_shift_right,
						   rcount_and_start,
						   kapi_const_expr(48)),
				   kapi_const_expr(48));

    // Read new %pic/%tick and truncate it to 48 bits
    kapi_arith_expr timer_at_stop(kapi_shift_right, 
				  kapi_arith_expr(kapi_shift_left, 
						  rawCtrExpr,
						  kapi_const_expr(16)),
				  kapi_const_expr(16));
    
    kapi_arith_expr value_to_save(kapi_bit_or, shifted_rcount,
				  timer_at_stop);

    // Save this vtimer's address and value
    kapi_arith_expr save_addr(kapi_assign, 
			      kapi_int_variable(saved_timer_address_offset,
						vtimer_iter),
			      vtimer_addr_base);
    kapi_arith_expr save_value(kapi_assign, 
			       kapi_int_variable(saved_timer_auxdata_offset,
						 vtimer_iter),
			       value_to_save);
    
    // Increment the iterator
    kapi_arith_expr inc_iter(kapi_assign, vtimer_iter, 
			     kapi_arith_expr(kapi_plus, vtimer_iter,
					     kapi_const_expr(
						 sizeof_saved_timer)));
    
    // Phase 2: update total with the time accrued since it started

    // mask-off the rcount field, leave start
    kapi_arith_expr old_start(kapi_shift_right, 
			      kapi_arith_expr(kapi_shift_left,
					      rcount_and_start,
					      kapi_const_expr(16)),
			      kapi_const_expr(16));
    kapi_arith_expr delta(kapi_minus, timer_at_stop, old_start);

    kapi_arith_expr update_total(kapi_assign, total_field,
				 kapi_arith_expr(kapi_plus, total_field, 
						 delta));

    // zero-out rcount/start
    kapi_arith_expr update_rcount_start(kapi_assign, rcount_and_start,
					kapi_const_expr(0));
    
    // Assemble all the pieces together
    kapi_vector<kapi_snippet> sv;
    sv.push_back(save_addr);
    sv.push_back(save_value);
    sv.push_back(inc_iter);
    sv.push_back(update_total);
    sv.push_back(update_rcount_start);
    kapi_sequence_expr do_stop(sv);

    // Do stop only if rcount is non-zero
    kapi_bool_expr rcount_is_zero(kapi_ne, shifted_rcount, kapi_const_expr(0));
    kapi_if_expr stop_if_needed(rcount_is_zero, do_stop);

    // Emit ret at the end
    kapi_vector<kapi_snippet> update_and_ret;
    update_and_ret.push_back(stop_if_needed);
    update_and_ret.push_back(kapi_ret_expr());
    
    return kapi_sequence_expr(update_and_ret);
}

void instrument_switch_points(kapi_point_location type,
			      const kapi_vtimer_cswitch_expr &cswitchin_expr,
			      kapi_vector<int> *pCallReqids,
			      int *pCswitchReqid)
{
    kapi_vector<kapi_point> cswitchPoints;
    if (kmgr.findSpecialPoints(type, &cswitchPoints) < 0) {
	assert(false);
    }

    int cswitchin_reqid = kmgr.uploadSnippet(cswitchin_expr, cswitchPoints);
    if (cswitchin_reqid < 0) {
	assert(false);
    }

    kptr_t cswitchin_addr = kmgr.getUploadedAddress(cswitchin_reqid);
    kapi_vector<kapi_snippet> empty;
    kapi_call_expr my_call(cswitchin_addr, empty);

    kapi_vector<kapi_point>::const_iterator ipoint = cswitchPoints.begin();
    for (; ipoint != cswitchPoints.end(); ipoint++) {
	int id = kmgr.insertSnippet(my_call, *ipoint);
	if (id < 0) {
	    assert(false);
	}
	pCallReqids->push_back(id);
    }
    *pCswitchReqid = cswitchin_reqid;
}

void deinstrument_switch_points(const kapi_vector<int> &call_reqid,
				int cswitchin_reqid)
{
    kapi_vector<int>::const_iterator icall = call_reqid.begin();
    for (; icall != call_reqid.end(); icall++) {
	if (kmgr.removeSnippet(*icall) < 0) {
	    assert(false);
	}
    }
    
    if (kmgr.removeUploadedSnippet(cswitchin_reqid) < 0) {
	assert(false);
    }
}

void initialize_vtimer(kptr_t vtimerAddrInKernel,
		       unsigned elemsPerVector,
		       unsigned bytesPerStride,
		       kptr_t stopRoutineKernelAddr,
		       kptr_t restartRoutineKernelAddr)
{
    struct {
	uint64_t total_field;
	uint64_t start_field;
	kptr_t stop_field;
	kptr_t restart_field;
    } buffer;
    
    buffer.total_field = 0;
    buffer.start_field = 0;
    buffer.stop_field = stopRoutineKernelAddr;
    buffer.restart_field = restartRoutineKernelAddr;
    kmgr.to_kernel_byte_order(&buffer.stop_field, sizeof(kptr_t));
    kmgr.to_kernel_byte_order(&buffer.restart_field, sizeof(kptr_t));

    for (unsigned i=0; i<elemsPerVector; i++) {
	kmgr.memcopy(&buffer, vtimerAddrInKernel + i * bytesPerStride,
		     sizeof(buffer));
    }
}

main(int argc, char **argv)
{
    if (argc != 3) {
	return usage(argv[0]);
    }

    char *host = argv[1];
    int port = atoi(argv[2]);

    // Attach to the kernel
    if (kmgr.attach(host, port) < 0) {
	abort();
    }

    if (kmgr.setTestingFlag(false) < 0) {
	abort();
    }
    
    kapi_module kmod;
    if (kmgr.findModule("genunix", &kmod) < 0) {
	abort();
    }

    kapi_function kfunc;
    if (kmod.findFunction("kmem_alloc", &kfunc) < 0) {
	abort();
    }

    kapi_vector<kapi_point> entries;
    if (kfunc.findEntryPoint(&entries) < 0) {
	abort();
    }

    kapi_vector<kapi_point> exits;
    if (kfunc.findExitPoints(&exits) < 0) {
	abort();
    }

    kptr_t addr;
    unsigned size = 8192;
    if ((addr = kmgr.malloc(size)) == 0) {
	abort();
    }
    cout << "Allocated " << size << " bytes at 0x" 
	 << hex << addr << dec << endl;

    // Initialize the kernel memory, by copying buffer from the user space
    void *buffer = malloc(size);
    assert(buffer);
    memset(buffer, 0, size);
    if (kmgr.memcopy(buffer, addr, size) < 0) {
	abort();
    }
    free(buffer);

    // Create an int variable in the allocated space
    kapi_int_variable intCounter(addr);
    
#if 0
    kapi_snippet stop_expr = generateVTimerStop(addr);
    kapi_snippet start_expr = generateVTimerStart(addr);
    int entry_id;
    kapi_vector<int> exit_ids;

    kapi_vector<kapi_point>::const_iterator ptiter = exits.begin();
    for (; ptiter != exits.end(); ptiter++) {
	int id = kmgr.insertSnippet(stop_expr, *ptiter);
	if (id < 0) {
	    abort();
	}
	exit_ids.push_back(id);
    }
    if ((entry_id = kmgr.insertSnippet(start_expr, entries[0])) < 0) {
	abort();
    }

    if (kmgr.removeSnippet(entry_id) < 0) {
	abort();
    }
    kapi_vector<int>::const_iterator iditer = exit_ids.begin();
    for (; iditer != exit_ids.end(); iditer++) {
	if (kmgr.removeSnippet(*iditer) < 0) {
	    abort();
	}
    }
    kapi_int_variable code(addr); // dummy
#elif 0
    kptr_t all_vtimers_addr = all_vtimers_create();
    for (unsigned i=1; i<=20; i++) {
	add_timer_to_all_vtimers(all_vtimers_addr, i);
    }
    for (unsigned i=1; i<=20; i++) {
	remove_timer_from_all_vtimers(all_vtimers_addr, i);
    }
    all_vtimers_delete(all_vtimers_addr);
    kapi_int_variable code(addr); // dummy
#elif 1
    kptr_t all_vtimers_addr = all_vtimers_create();

    kapi_vtimer_cswitch_expr cswitchin_expr(kapi_cswitch_in,
					    all_vtimers_addr);
    kapi_vtimer_cswitch_expr cswitchout_expr(kapi_cswitch_out,
					     all_vtimers_addr);

    kapi_vector<int> callInReqid, callOutReqid;
    int cswitchInReqid, cswitchOutReqid;

    instrument_switch_points(kapi_cswitch_in, cswitchin_expr,
			     &callInReqid, &cswitchInReqid);
    instrument_switch_points(kapi_cswitch_out, cswitchout_expr,
			     &callOutReqid, &cswitchOutReqid);

    kapi_snippet vrestart = 
	generateMetricSpecificVRestart(cswitchin_expr.getVRestartAddrExpr(),
				       cswitchin_expr.getAuxdataExpr(),
				       kapi_hwcounter_expr(HWC_TICKS));
    kapi_snippet vstop = 
	generateMetricSpecificVStop(cswitchout_expr.getVStopAddrExpr(),
				    cswitchout_expr.getStartExpr(),
				    cswitchout_expr.getIterExpr(),
				    kapi_hwcounter_expr(HWC_TICKS));


    kapi_vector<kapi_point> dummy_vrestart_point;
    dummy_vrestart_point.push_back(kapi_point(0, kapi_vrestart_routine));

    kapi_vector<kapi_point> dummy_vstop_point;
    dummy_vstop_point.push_back(kapi_point(0, kapi_vstop_routine));

    int vrestart_id;
    if ((vrestart_id = kmgr.uploadSnippet(vrestart,
					  dummy_vrestart_point)) < 0) {
	assert(false);
    }

    int vstop_id;
    if ((vstop_id = kmgr.uploadSnippet(vstop,
				       dummy_vstop_point)) < 0) {
	assert(false);
    }

    kptr_t vrestart_addr = kmgr.getUploadedAddress(vrestart_id);
    kptr_t vstop_addr = kmgr.getUploadedAddress(vstop_id);
    initialize_vtimer(addr, 1, 64, vstop_addr, vrestart_addr);

    add_timer_to_all_vtimers(all_vtimers_addr, addr);

    kapi_snippet stop_expr = generateVTimerStop(addr);
    kapi_snippet start_expr = generateVTimerStart(addr);
    int entry_id;
    kapi_vector<int> exit_ids;

    kapi_vector<kapi_point>::const_iterator ptiter = exits.begin();
    for (; ptiter != exits.end(); ptiter++) {
	int id = kmgr.insertSnippet(stop_expr, *ptiter);
	if (id < 0) {
	    abort();
	}
	exit_ids.push_back(id);
    }
    if ((entry_id = kmgr.insertSnippet(start_expr, entries[0])) < 0) {
	abort();
    }

    if (kmgr.removeSnippet(entry_id) < 0) {
	abort();
    }
    kapi_vector<int>::const_iterator iditer = exit_ids.begin();
    for (; iditer != exit_ids.end(); iditer++) {
	if (kmgr.removeSnippet(*iditer) < 0) {
	    abort();
	}
    }

    remove_timer_from_all_vtimers(all_vtimers_addr, addr);

    if (kmgr.removeUploadedSnippet(vstop_id) < 0) {
	assert(false);
    }

    if (kmgr.removeUploadedSnippet(vrestart_id) < 0) {
	assert(false);
    }

    deinstrument_switch_points(callOutReqid, cswitchOutReqid);
    deinstrument_switch_points(callInReqid, cswitchInReqid);

    all_vtimers_delete(all_vtimers_addr);

    kapi_int_variable code(addr); // dummy
#elif 0
    kapi_module toMod;
    if (kmgr.findModule("kerninst", &toMod) < 0) {
	abort();
    }

    kapi_function toFunc;
    if (toMod.findFunction("kerninst_call_me_2", &toFunc) < 0) {
	abort();
    }

    kapi_vector<kapi_snippet> args;
    kapi_const_expr arg0(10);
    kapi_const_expr arg1(11);
    args.push_back(&arg0);
    args.push_back(&arg1);

    kapi_call_expr code(toFunc.getEntryAddr(), args);
#elif 0
    kapi_arith_expr code(kapi_assign, intCounter, kapi_retval_expr(kfunc));
#elif 0
    kapi_arith_expr code(kapi_assign, intCounter, kapi_param_expr(0));
#elif 0
    kapi_snippet cpuOffset(computeCpuOffset());
    kapi_arith_expr start_addr(kapi_plus, kapi_const_expr(addr),
			       cpuOffset);
    kapi_int_variable total_field(0, start_addr);
    kapi_int_variable rcount_and_start(start_field_offset, start_addr);

    kapi_arith_expr old_rcount(kapi_shift_right, rcount_and_start,
			       kapi_const_expr(48));
    kapi_arith_expr old_start(kapi_shift_right, 
			      kapi_arith_expr(kapi_shift_left, 
					      rcount_and_start,
					      kapi_const_expr(16)),
			      kapi_const_expr(16));

    kapi_arith_expr new_rcount(kapi_minus, old_rcount, kapi_const_expr(1));
    kapi_arith_expr new_shifted_rcount(kapi_shift_left, new_rcount, 
				       kapi_const_expr(48));
    kapi_bool_expr new_rcount_is_zero(kapi_eq, new_rcount, kapi_const_expr(0));

    kapi_if_expr new_start(new_rcount_is_zero, kapi_const_expr(0), old_start);

    kapi_arith_expr new_rcount_and_start(kapi_bit_or, 
					 new_shifted_rcount,
					 new_start);

    kapi_arith_expr update_control(kapi_atomic_assign, rcount_and_start,
				   new_rcount_and_start);

    kapi_arith_expr current_time(kapi_shift_right, 
				 kapi_arith_expr(kapi_shift_left, 
						kapi_hwcounter_expr(HWC_TICKS),
						 kapi_const_expr(16)),
				 kapi_const_expr(16));
    kapi_arith_expr old_start_2(kapi_shift_right, 
				kapi_arith_expr(kapi_shift_left, 
						rcount_and_start,
						kapi_const_expr(16)),
				kapi_const_expr(16));
    kapi_arith_expr time_delta(kapi_minus, current_time, old_start_2);
    kapi_if_expr update_total(new_rcount_is_zero, 
			      kapi_arith_expr(kapi_atomic_assign,
					      total_field,
					      kapi_arith_expr(kapi_plus,
							      total_field,
							      time_delta)));
    kapi_vector<kapi_snippet> vUpdates;
    vUpdates.push_back(&update_control);
    vUpdates.push_back(&update_total);
    kapi_sequence_expr updates(vUpdates);
    
    kapi_bool_expr old_rcount_non_zero(kapi_ne, old_rcount, 
				       kapi_const_expr(0));
    kapi_if_expr code(old_rcount_non_zero, updates);
#elif 0
    kapi_snippet cpuOffset(computeCpuOffset());
    kapi_int_variable rcount_and_start(addr + start_field_offset,
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
					      kapi_hwcounter_expr(
						  kapi_hwcounter_expr::ticks),
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

    kapi_arith_expr code(kapi_atomic_assign, rcount_and_start,
			 new_rcount_and_start);
#elif 0
    kapi_int_variable jntCounter(addr, computeCpuOffset());
    kapi_arith_expr code(kapi_atomic_assign, jntCounter, 
			 kapi_arith_expr(kapi_plus, jntCounter, 
					 kapi_const_expr(1)));
#elif 0
     kapi_int_variable code(addr);
#elif 0
    kapi_int_variable startCounter(addr);
    kapi_int_variable totalCounter(addr+8);
    kapi_arith_expr code(kapi_assign, totalCounter,
			 kapi_arith_expr(kapi_plus,
					 totalCounter,
					 kapi_arith_expr(kapi_minus,
			       kapi_hwcounter_expr(kapi_hwcounter_expr::ticks),
							 startCounter)));
#elif 0
    kapi_arith_expr code(kapi_assign, intCounter, 
			 kapi_hwcounter_expr(kapi_hwcounter_expr::ticks));
#elif 0
    kapi_int_variable jntCounter(addr, 
				 kapi_arith_expr(kapi_shift_left,
						 kapi_cpuid_expr(),
						 kapi_const_expr(6)));

    kapi_arith_expr code(kapi_assign, jntCounter,
			 kapi_arith_expr(kapi_plus, jntCounter, 
					 kapi_const_expr(1)));
#elif 0
    // Initialize intCounter to equal CPUID
    kapi_arith_expr code(kapi_assign, intCounter, kapi_cpuid_expr());
#elif 0
    // Increment the variable
    kapi_arith_expr code(kapi_assign, intCounter, 
			 kapi_arith_expr(kapi_plus, intCounter, 
					 kapi_const_expr(1)));
#elif 0
    // Increment the variable atomically
    kapi_arith_expr code(kapi_atomic_assign, intCounter, 
			 kapi_arith_expr(kapi_plus, intCounter, 
					 kapi_const_expr(1)));
#elif 0
    // Test if the variable is >=0 and <= 20
    kapi_bool_expr testZero(kapi_ge, intCounter, kapi_const_expr(0));
    kapi_bool_expr testTwenty(kapi_le, intCounter, kapi_const_expr(20));
    kapi_bool_expr test(kapi_log_and, testZero, testTwenty);

    // Generate code to increment the variable
    kapi_arith_expr addOne(kapi_assign, intCounter, 
			   kapi_arith_expr(kapi_plus, intCounter, 
					   kapi_const_expr(1)));

    kapi_if_expr code(test, addOne);
#elif 0
    kapi_bool_expr test(kapi_eq, intCounter, intCounter);
    kapi_arith_expr code(kapi_assign, intCounter, test);
#elif 0
    kapi_arith_expr code(kapi_times,
			 kapi_arith_expr(kapi_plus,
					 intCounter, 
					 kapi_arith_expr(kapi_minus,
							 intCounter,
							 kapi_const_expr(1))),
			 intCounter);

#elif 0
    kapi_int_variable jntCounter(addr+8);
    kapi_arith_expr code(kapi_plus, intCounter, jntCounter);
#elif 0
    // Test if the variable x is >=1 and <= 2
    kapi_bool_expr testOne(kapi_ge, intCounter, kapi_const_expr(1));
    kapi_bool_expr testTwo(kapi_le, intCounter, kapi_const_expr(2));
    kapi_bool_expr test(kapi_log_and, testOne, testTwo);
    
    kapi_arith_expr code(kapi_assign, intCounter, test);
#elif 0
    kapi_sequence_expr code;
    kapi_label_expr label("dummy");
    kapi_arith_expr addOne(kapi_assign, intCounter, 
			   kapi_arith_expr(kapi_plus, intCounter, 
					   kapi_const_expr(1)));
    kapi_arith_expr addTwo(kapi_assign, intCounter, 
			   kapi_arith_expr(kapi_plus, intCounter, 
					   kapi_const_expr(2)));
    code.push_back(&label);
    code.push_back(&addOne);
    code.push_back(&addTwo);
#endif

    // Splice the code at specified points
    int sh;
    if ((sh = kmgr.insertSnippet(code, entries[0])) < 0) {
	abort();
    }
    while (kmgr.handleEvents() >= 0);

    if (kmgr.removeSnippet(sh) < 0) {
	abort();
    }
    
    if (kmgr.free(addr) < 0) {
	abort();
    }
    
    if (kmgr.detach() < 0) {
	abort();
    }
}

