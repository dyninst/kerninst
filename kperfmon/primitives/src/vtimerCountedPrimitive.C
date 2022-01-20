// vtimerCountedPrimitive.C

#include "vtimer.h"
#include "vtimerCountedPrimitive.h"
#include "dataHeap.h"

// Main method: generates AST tree for the primitive
kapi_snippet vtimerCountedStartPrimitive::getKapiSnippet() const
{
    kapi_snippet cpuOffset(computeCpuOffset());
    kapi_int64_variable rcount_and_start(vtimer_addr + start_field_offset,
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
					      rawCtrExpr,
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

    // Finally, wrap the code with lock/unlock to protect from preemption
    // in the middle of the primitive.
    return kmgr.getPreemptionProtectedCode(code);
}

// Return stride_size -- that many bytes separate timers for different CPUs
unsigned vtimerCountedStartPrimitive::getStrideSize() const
{
    extern dataHeap *theGlobalDataHeap;

    return theGlobalDataHeap->getBytesPerStrideVT();
}

// ---------------------------------------------------------------------------

// Main method: generates AST tree for the primitive
kapi_snippet vtimerCountedStopPrimitive::getKapiSnippet() const
{
    kapi_snippet cpuOffset(computeCpuOffset());
    kapi_arith_expr start_addr(kapi_plus, kapi_const_expr(vtimer_addr),
			       cpuOffset);
    kapi_int64_variable total_field(0, start_addr);
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
						 rawCtrExpr,
						 kapi_const_expr(16)),
				 kapi_const_expr(16));

#ifdef sparc_sun_solaris2_7
    // Compute the delta and truncate it to 32 bits. This approach
    // lets us handle %pic wraparound: the delta is computed correctly
    // even if %pic wrapped around, thanks to the wonders of the
    // 2-bit-complement arithmetic. FIXME: it may break delta for
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
    kapi_vector<kapi_snippet> updates;
    updates.push_back(time_delta);
    updates.push_back(update_control);
    updates.push_back(update_total);
    kapi_sequence_expr update_sequence(updates);

    kapi_bool_expr old_rcount_non_zero(kapi_ne, old_rcount, 
				       kapi_const_expr(0));
    kapi_if_expr code(old_rcount_non_zero, update_sequence);

    // Finally, wrap the code with lock/unlock to protect from migration
    // in the middle of the primitive
    return kmgr.getPreemptionProtectedCode(code);
}

// Return stride_size -- that many bytes separate timers for different CPUs
unsigned vtimerCountedStopPrimitive::getStrideSize() const
{
    extern dataHeap *theGlobalDataHeap;

    return theGlobalDataHeap->getBytesPerStrideVT();
}

