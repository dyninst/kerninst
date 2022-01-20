#include "util/h/out_streams.h"
#include "globalLock.h"
#include "abi.h"
#include "instrumenter.h"

// We have to put the virtualization code in a critical section for now --
// the stacklist and hashtable code is not safe. Let's create only one lock
// and hope to eliminate it later
static kptr_t global_lock_kernel_addr = 0;


extern instrumenter *theGlobalInstrumenter;

// Allocate 8 bytes of kernel memory and initialize them to zero
void create_global_lock()
{
   assert(global_lock_kernel_addr == 0);

   // It is likely that we'll get the whole page, but it is Ok. Remember,
   // we have only one lock
   pair<kptr_t, dptr_t> lock_addrs =
       theGlobalInstrumenter->allocateMappedKernelMemory(8, false);
   dout << "Allocated 8 bytes at <" << addr2hex(lock_addrs.first) 
	<< "," << addr2hex(lock_addrs.second) << ">\n";
   global_lock_kernel_addr = lock_addrs.first;
   
   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   sparc_tempBufferEmitter init_em(kerninstdABI);

   init_em.emitImmAddr(lock_addrs.second, sparc_reg::o0);
   instr_t *i = new sparc_instr(sparc_instr::store, sparc_instr::stExtended,
				sparc_reg::g0, sparc_reg::o0, 0);
   init_em.emit(i);
   i = new sparc_instr(sparc_instr::retl);
   init_em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   init_em.emit(i);
   init_em.complete();

   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_em);
}

// Free the allocated memory
void delete_global_lock()
{
    assert(global_lock_kernel_addr != 0);
    theGlobalInstrumenter->freeMappedKernelMemory(global_lock_kernel_addr, 8);
    global_lock_kernel_addr = 0;
}

// Emit code to set the lock to 1 atomically and spin-wait until we succeed
void emit_global_lock(tempBufferEmitter &em, sparc_reg vreg)
{
    const tempBufferEmitter::insnOffset retry_offset = em.currInsnOffset();

    assert(global_lock_kernel_addr != 0);

    em.emitImmAddr(global_lock_kernel_addr, vreg);
    instr_t *i = new sparc_instr(sparc_instr::cas, true, // extended
				 vreg, sparc_reg::g0, vreg);
    em.emit(i);
    i = new sparc_instr(sparc_instr::bpr,
			sparc_instr::regNotZero,
			false, // not annulled
			false, // predict not taken
			vreg,
			-(em.currInsnOffset() - retry_offset) // displacement
			);
    em.emit(i);
    i = new sparc_instr(sparc_instr::nop);
    em.emit(i);

    // Make sure that we locked the lock before we start accessing the
    // shared data
    i = new sparc_instr(sparc_instr::membarrier, 0, 
			sparc_instr::StoreLoad | sparc_instr::StoreStore);
    em.emit(i);
}

// Drop the lock. No need for atomicity -- we are the exclusive owner
void emit_global_unlock(tempBufferEmitter &em, sparc_reg vreg)
{
    assert(global_lock_kernel_addr != 0);

    // Make sure that our previous memory accesses have completed before
    // we unlock the lock
    instr_t *i = new sparc_instr(sparc_instr::membarrier, 0, 
				 sparc_instr::LoadStore | sparc_instr::StoreStore);
    em.emit(i);
    em.emitImmAddr(global_lock_kernel_addr, vreg);
    i = new sparc_instr(sparc_instr::store, sparc_instr::stExtended,
			sparc_reg::g0, vreg, 0);
    em.emit(i);
}
