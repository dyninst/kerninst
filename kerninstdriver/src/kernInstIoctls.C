// kerninstIoctls.C
// implementation of various ioctls

#include "kernInstIoctls.h"
#include "loggedAllocRequests.h"
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/kobj.h>

/* These must always be included last: */
#include <sys/ddi.h>
#include <sys/sunddi.h>

void kerninst_alloc_block(void *iloggedAllocRequests,
                          kernInstAllocStruct *input_output_args) {
   loggedAllocRequests *theLoggedAllocRequests =
      (loggedAllocRequests*)iloggedAllocRequests;

   if (theLoggedAllocRequests == NULL)
      cmn_err(CE_PANIC, "kerninst_alloc_block with NULL loggedAllocRequests!");
   if (input_output_args->nbytes == 0)
      cmn_err(CE_WARN, "kerninst_alloc_block with 0 bytes seems strange");

   theLoggedAllocRequests->alloc(input_output_args->nbytes,
                                 input_output_args->requiredAlignment,
                                 input_output_args->allocType,
                                 // the following 2 params get filled in:
                                 input_output_args->result,
                                 input_output_args->result_mappable);
      // tryForNucleusText==1: if true, we try, but if we can't allocate in
      // the nucleus, we return non-nucleus memory.

   if (input_output_args->result == 0) {
      cmn_err(CE_WARN, "kerninst_alloc_block seemed to fail -- returning 0");
      return;
   }
   
   if (input_output_args->result % input_output_args->requiredAlignment != 0)
      cmn_err(CE_NOTE,
              "kerninst_alloc_block result doesn't meet your alignment specs!");

   if (input_output_args->result_mappable % input_output_args->requiredAlignment != 0)
      cmn_err(CE_NOTE, "kerninst_alloc_block result_mappable doesn't meet your alignment specs!");
}

void kerninst_free_block(void *iloggedAllocRequests,
                         kernInstFreeStruct *input_args) {
   loggedAllocRequests *theLoggedAllocRequests =
      (loggedAllocRequests*)iloggedAllocRequests;

   if (theLoggedAllocRequests == NULL)
      cmn_err(CE_PANIC, "kerninst_free_block with NULL loggedAllocRequests!");
   if (input_args->kernelAddr == 0)
      cmn_err(CE_PANIC, "kerninst_free_block passed NULL argument");
   if (input_args->nbytes == 0)
      cmn_err(CE_WARN, "kerninst_free_block passed 0 bytes to free at 0x%lx",
              input_args->kernelAddr);
   
   theLoggedAllocRequests->free(input_args->kernelAddr,
                                input_args->mappedKernelAddr,
                                input_args->nbytes);
}

extern "C" int kerninst_call_me(int i, int j, int k) {
   int iter;
   int rv = j;
   for(iter=0; iter<=i; iter++) {
      if(iter%3 == 0)
         rv += k;
      if(iter%4 == 0)
         rv -= j;
   }
   return rv; // expected return val is 19458 for (i=123, j=456, k=789) 
}

static void kerninst_call_me_x_times(unsigned x) {
   int rv;
   for (unsigned lcv=0; lcv < x; lcv++)
      rv = kerninst_call_me(123, 456, 789);
}

void kerninst_call_once() {
   cmn_err(CE_NOTE, "in kerninst_call_once");
   kerninst_call_me_x_times(1);
}

extern "C" void kerninst_call_once_v2() {
   cmn_err(CE_NOTE, "in kerninst_call_once_v2");
   kerninst_call_me_x_times(1);
}

void kerninst_call_twice() {
   kerninst_call_me_x_times(2);
}

void kerninst_call_tentimes() {
   kerninst_call_me_x_times(10);
}

void getKernInstAddressMap(struct kernInstAddressMap *bounds)
{
    uint32_t reachable = 8 * 1024 * 1024; /* bicc reachable */

    /*
      Getting the bounds of "part a" of the nucleus: the part that is
      text-only, and always mapped into the I-TLB.  We hardwire this
      information at: KERNELBASE thru KERNELBASE + 4MB.
      I got this information from reading the file startup.c in the
      solaris 7 source code 
    */
    bounds->startNucleus = KERNELBASE;
    bounds->endNucleus = bounds->startNucleus + 4*1024*1024;
    /*
      The 4MB region below the nucleus can be used to reach code
      in the nucleus
    */
    bounds->startBelow = bounds->endNucleus - reachable;
    bounds->endBelow = bounds->startNucleus;
}

// Lookup the sys_tick_freq symbol. If not found (Solaris 7 and old 
// Solaris 8), lookup system_clock_freq symbol. If not found (Solaris 7)
// return 0, otherwise, return the symbol's contents
uint32_t getSystemClockFreq()
{
    static uint32_t* rv = 0; // Cache the address of the symbol

    if (rv == 0 &&
	(rv = (uint32_t *)kobj_getsymvalue((char *)"sys_tick_freq",
					   0)) == 0 &&
	(rv = (uint32_t *)kobj_getsymvalue((char *)"system_clock_freq", 
					    0)) == 0) {
	cmn_err(CE_WARN, "getSystemClockFreq: could not find neither "
		"the sys_tick_freq nor the system_clock_freq symbol\n");
	return 0;
    }
    return *rv;
}
