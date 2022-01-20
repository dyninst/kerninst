// traceBuffer.h

#ifndef _TRACE_BUFFER_H_
#define _TRACE_BUFFER_H_

#include <inttypes.h> // uint32_t, etc.
#include "util/h/kdrvtypes.h"
#include "tempBufferEmitter.h"

#ifdef sparc_sun_solaris2_7
#include "sparc_reg.h"
#endif

// each trace buffer entry: 64 bits (32 for kthreadid, 16 for pid, 16 for description
// of the operation being performed).
// the trace buffer iterator: the address where the iterator resides is of course
// fixed.  It normally points to somewhere within the trace buffer.  But, if it's
// zero (a null pointer), then we take it as an indication that the trace buffer
// should stop logging.

class traceBuffer {
 private:
   bool kerninstd_only;
   
   kptr_t traceBufferKernelAddr;
   dptr_t traceBufferKerninstdAddr; // so we can initialize it fairly easily

   kptr_t traceBufferIterKernelAddr; // location where the iterator ptr resides
      // (not the value of the pointer...you must do a load to get the value, which
      // will be either NULL [stopped logging] or a value that lies within the
      // trace buffer)
   dptr_t traceBufferIterKerninstdAddr;

   unsigned num_entries, lg_num_entries;
   
   static const unsigned nbytes_per_entry, lg_nbytes_per_entry;

#ifdef sparc_sun_solaris2_7
   void emit_initialization_code(tempBufferEmitter &);
#endif
   
 public:
   traceBuffer(unsigned num_entries, bool ikerninstd_only);
      // if ikerninstd_only flag is set then we only allocate kerninstd memory.
      // Otherwise we allocate mapped kernel memory.
      // Make num_entries a power of two, please.

  ~traceBuffer();
      // frees mapped kernel memory

#ifdef sparc_sun_solaris2_7
   void emit_appendx(tempBufferEmitter &em,
		     bool just_in_kerninstd,
		     sparc_reg regContainingOpData, // last 16 bits of buffer entry
		     sparc_reg reg0, // doesn't need to be 64-bit-safe
		     sparc_reg reg1, // doesn't need to be 64-bit-safe
		     sparc_reg reg2_64bitsafe,
		     const pdstring &trace_skip_labelname);
      // emitted code will log iff the iterator is non-null (null indicates that
      // we wish to stop logging, presumably because an error was detected and we
      // want to freeze the state of the log for later examination)

   void emit_stop_logging(tempBufferEmitter &em,
                          sparc_reg scratch_reg0,
                          sparc_reg scratch_reg1);
      // call this when an error is detected; we'll set the iterator
      // to NULL which will prevent any further logging.  Before that, we'll
      // copy the iterator to the post-iterator (located 4 bytes after the iterator)
      // so you can examine it.

   // The following is static & public in case somebody else wants to use it
   // In fact, emit.C in kerninstapi/codegen does use it
   static void generateGetCurrPidx(tempBufferEmitter &em,
				   bool in_kerninstd,
				   sparc_reg destreg);
#endif // sparc_solaris
};

#endif
