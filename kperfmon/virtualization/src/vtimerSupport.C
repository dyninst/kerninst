// vtimerSupport.C
// some fns needed by both vtimerMgr.C and vtimerTest.C

#include "vtimerSupport.h"
#include "regSetManager.h"
#include "stacklist.h" // stacklistitem_nbytes_dataonly
#include "instrumenter.h"

// --------------------------------------------------------------------------------

void emit_addNewEntryToAllVTimers(tempBufferEmitter &em,
                                  kptr_t vtimerAddrInKernel,
                                  dptr_t all_vtimers_kerninstdAddr,
                                  dptr_t all_vtimers_add_kerninstdAddr) {
   // we need to emit a save since we're making a non-tail call.
   extern instrumenter *theGlobalInstrumenter;
   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();

   instr_t *i = new sparc_instr(sparc_instr::save, 
				-kerninstdABI.getMinFrameAlignedNumBytes());
   em.emit(i);

   // argument #1: all_vtimers_kerninstdAddr (not all_vtimers_kernelAddr)
   assert(all_vtimers_kerninstdAddr != 0);
   em.emitImmAddr(all_vtimers_kerninstdAddr, sparc_reg::o0);

   // argument #2: address of new vtimer (kernel address)
   assert(vtimerAddrInKernel != 0);
   em.emitImmAddr(vtimerAddrInKernel, sparc_reg::o1);
   
   // set %o2 to the address of the fn to call
   em.emitImmAddr(all_vtimers_add_kerninstdAddr, sparc_reg::o2);
   
   // make the call, assert the return value is 1
   i = new sparc_instr(sparc_instr::callandlink, sparc_reg::o2);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // result value should be 1
   i = new sparc_instr(sparc_instr::sub, true, // modify icc/xcc
                       false, // no carry
                       sparc_reg::o0, // dest
                       sparc_reg::o0, 0x1);
   em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                       false, // not extended
                       0x34);
   em.emit(i);

   // return
   i = new sparc_instr(sparc_instr::ret);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
}
