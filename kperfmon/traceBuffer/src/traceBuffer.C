// traceBuffer.C

#include "traceBuffer.h"
#include "instrumenter.h"
#include "machineInfo.h"
#include "util/h/ffs.h" // ari_ffs()
#include "abi.h"

extern instrumenter *theGlobalInstrumenter;

const unsigned traceBuffer::nbytes_per_entry = 8;
const unsigned traceBuffer::lg_nbytes_per_entry = 3;

#ifdef sparc_sun_solaris2_7

void traceBuffer::emit_initialization_code(tempBufferEmitter &init_em) {

   // This code is downloaded into kerninstd
   assert(nbytes_per_entry == 8); // we can loosen this assert if it becomes necessary

   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   instr_t *i = new sparc_instr(sparc_instr::save, -kerninstdABI.getMinFrameAlignedNumBytes());
   init_em.emit(i);
   init_em.emitImm32(traceBufferKerninstdAddr, sparc_reg::o0);
   init_em.emitImm32(num_entries, sparc_reg::o1);
   
   const tempBufferEmitter::insnOffset loopstartoffset = 
	   init_em.currInsnOffset();

   // check for done
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // predict not taken
		       false, // not annulled
		       sparc_reg::o1,
		       0 // diplacement for now
		       );
   init_em.emitCondBranchToForwardLabel(i, "loop_done");
   i = new sparc_instr(sparc_instr::nop);
   init_em.emit(i);

   i = new sparc_instr(sparc_instr::store, sparc_instr::stExtended,
		       sparc_reg::g0, sparc_reg::o0, 0);
   init_em.emit(i);
   i = new sparc_instr(sparc_instr::add, sparc_reg::o0, nbytes_per_entry,
		       sparc_reg::o0);
   init_em.emit(i);

   i = new sparc_instr(sparc_instr::bicc, sparc_instr::iccAlways,
		       false, // not annulled
		       -(init_em.currInsnOffset() - loopstartoffset));
   init_em.emit(i);
   i = new sparc_instr(sparc_instr::sub, sparc_reg::o1, 1, sparc_reg::o1);
   init_em.emit(i);
      // delay slot

   init_em.emitLabel("loop_done");

   // And now initialize the memory pointed to by the iterator.  That is: the addr of
   // the iterator is fixed [traceBufferIterKernelAddr/traceBufferIterKerninstdAddr];
   // what it points to needs to be initialized
   init_em.emitImm32(traceBufferIterKerninstdAddr, sparc_reg::o0);
      // location of the iterator.  We'll write something at this addr
   init_em.emitImm32(traceBufferKernelAddr, sparc_reg::o1);
      // this is what we should write
   i = new sparc_instr(sparc_instr::store, sparc_instr::stWord,
		       sparc_reg::o1, // src data
		       sparc_reg::o0, 0);
   init_em.emit(i);
   // Also initialize the "post-stop iterator" to NULL
   i = new sparc_instr(sparc_instr::store, sparc_instr::stWord,
		       sparc_reg::g0, // src data
		       sparc_reg::o0, 4);
   init_em.emit(i);

   i = new sparc_instr(sparc_instr::retl);
   init_em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   init_em.emit(i);
}

#endif // sparc_solaris

traceBuffer::traceBuffer(unsigned inum_entries,
                         bool ikerninstd_only) {
   kerninstd_only = ikerninstd_only;
   num_entries = inum_entries;
   
   lg_num_entries = ari_ffs(num_entries);
   const unsigned cmp_num_entries = 1U << lg_num_entries;
   assert(cmp_num_entries == num_entries &&
          "pass a power of 2 for traceBuffer num_entries please");
   
   const unsigned nbytes_dataonly = nbytes_per_entry * num_entries;
   const unsigned nbytes_all = nbytes_dataonly + 4 + 4;
   // +4 for the iterator & post-stop-iterator

#ifdef sparc_sun_solaris2_7

   if (kerninstd_only) {
      dptr_t addr = theGlobalInstrumenter->allocateKerninstdMemory(nbytes_all);
      traceBufferKernelAddr = addr; // sic
      traceBufferKerninstdAddr = addr;
   }
   else {
      pair<kptr_t, dptr_t> addrs =
         theGlobalInstrumenter->allocateMappedKernelMemory(nbytes_all, false);
         // false --> don't try to allocate w/in kernel's nucleus text
      traceBufferKernelAddr = addrs.first;
      traceBufferKerninstdAddr = addrs.second;
   }
   
   traceBufferIterKernelAddr = traceBufferKernelAddr + nbytes_dataonly;
   traceBufferIterKerninstdAddr = traceBufferKerninstdAddr + nbytes_dataonly;

   const abi &kerninstdABI = theGlobalInstrumenter->getKerninstdABI();
   
   // initialize by downloading to kerninstd
   sparc_tempBufferEmitter init_em(kerninstdABI);
   emit_initialization_code(init_em);
   init_em.complete();
   theGlobalInstrumenter->downloadToKerninstdAndExecuteOnce(init_em);

     cout << "traceBuffer of " << num_entries << " at kernel " 
          << addr2hex(traceBufferKernelAddr) << " and kerninstd " 
          << addr2hex(traceBufferKerninstdAddr) << endl;
     cout << "iterator is at kernel " << addr2hex(traceBufferIterKernelAddr)
          << " and kerninstd " << addr2hex(traceBufferIterKerninstdAddr) << endl;
#endif // sparc solaris

}

traceBuffer::~traceBuffer() {
   const unsigned nbytes_dataonly = nbytes_per_entry * num_entries;
   const unsigned nbytes_all = nbytes_dataonly + 4 + 4;
      // +4 for the iterator & post-stop-iterator

#ifdef sparc_sun_solaris2_7
   if (kerninstd_only) {
      assert(traceBufferKernelAddr == traceBufferKerninstdAddr);
      theGlobalInstrumenter->freeKerninstdMemory(traceBufferKernelAddr,
                                                 nbytes_all);
   }
   else {
      theGlobalInstrumenter->freeMappedKernelMemory(traceBufferKernelAddr,
                                                    traceBufferKerninstdAddr,
                                                    nbytes_all);
   }

   traceBufferKernelAddr = traceBufferKerninstdAddr = 0;
   traceBufferIterKernelAddr= traceBufferIterKerninstdAddr = 0;

   cout << "freed trace buffer" << endl;
#endif
}

#ifdef sparc_sun_solaris2_7
void traceBuffer::emit_appendx(tempBufferEmitter &em,
                              bool just_in_kerninstd,
                              sparc_reg regContainingOpData, // last 16 bits of entry
                              sparc_reg reg0,
                              sparc_reg reg1,
                              sparc_reg reg2_64bitsafe,
                              const pdstring &trace_skip_labelname) {
   // assert that none of the parameter registers are the same register:
   sparc_reg_set s1;
   s1 += regContainingOpData;
   s1 += reg0;
   s1 += reg1;
   s1 += reg2_64bitsafe;
   assert(s1.count() == 4);

   // assert that reg2_64bitsafe is 64-bit safe
   const abi& theABI = em.getABI();
   assert(theABI.isReg64Safe(reg2_64bitsafe));
   
   em.emitImm32(traceBufferIterKernelAddr, reg0);
   instr_t *i = new sparc_instr(sparc_instr::load, sparc_instr::ldUnsignedWord,
				reg0, 0, reg1);
   em.emit(i);

   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       reg1,
		       0 // offset for now
		       );
   em.emitCondBranchToForwardLabel(i, trace_skip_labelname);

   // Loop until we are able to allocate a trace entry atomically
   const tempBufferEmitter::insnOffset loopstartoffset = em.currInsnOffset();

   i = new sparc_instr(sparc_instr::mov, reg1, reg2_64bitsafe);
   em.emit(i);

   // add [nbytes_per_entry] to reg1, "modulo num_data_bytes".  To get the
   // desired modulo effect, keep only the low (lg_nbytes_per_entry +
   // lg_num_entries) bits of reg1.  For example, with 32 (2^5) log entries
   // of 8 (2^3) bytes each, keep only the low 8 bits.  By the way, before
   // doing any of this, we need to make reg1 not an address but an offset.
   em.emitImm32(traceBufferKernelAddr, reg0);
   i = new sparc_instr(sparc_instr::sub, reg1, reg0, reg1);
   em.emit(i);

   i = new sparc_instr(sparc_instr::add, reg1, nbytes_per_entry, reg1);
   em.emit(i);

   const unsigned bits_to_keep = lg_num_entries + lg_nbytes_per_entry;
   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       false, // not extended
                       reg1, // dest
                       reg1, 32-bits_to_keep);
   em.emit(i);
   i = new sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
                       false, // not extended
                       reg1, // dest
                       reg1, 32-bits_to_keep);
   em.emit(i);

   // store new value of iterator.  At the moment, reg1 is just a
   // byte offset within the trace buffer.  Make it a proper address again.
   i = new sparc_instr(sparc_instr::add, reg0, reg1, reg1);
   em.emit(i);

   em.emitImm32(traceBufferIterKernelAddr, reg0);
   i = new sparc_instr(sparc_instr::cas, false, // not extended
		       reg0, reg2_64bitsafe, reg1);
   em.emit(i);
   i = new sparc_instr(sparc_instr::sub, true, // cc
                       false, // no carry
                       sparc_reg::g0, // dest
                       reg1, reg2_64bitsafe);
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccNotEq,
                       false, // not annulled
                       false, // predict not taken
                       true, // xcc (matters?)
                       -(em.currInsnOffset() - loopstartoffset));
   em.emit(i);

   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // Now fill in the trace buffer entry.  regContainingOpData contains what will
   // be the final 16 bits of the trace buffer entry.  (The shifts ensure that only
   // 16 bits are used).
   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       false, // not extended
                       regContainingOpData, // dest
                       regContainingOpData, 16);
   em.emit(i);
      // shift left by 16...
   i = new sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
                       false, // not extended
                       regContainingOpData, regContainingOpData, 0);
   em.emit(i);
      // ...clear the high 32 bits...
   i = new sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
                       false, // not extended
                       regContainingOpData, // dest
                       regContainingOpData, 16);
   em.emit(i);
      // ...and shift right 16 bits
   

   // put the pid into reg0, do some shifts to ensure it's just 16 bits & in its
   // proper trace buffer entry position.
   generateGetCurrPidx(em, just_in_kerninstd, reg0);
   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       false, // not extended
                       reg0, reg0, 16);
   em.emit(i);
      // shift left by 16...
   i = new sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
                       false, // not extended
                       reg0, // dest
                       reg0, 0);
   em.emit(i);
      // ...and clear the high 32 bits...
      // now the 16-bit pid is properly left-shifted by 16 bits...other bits are zeros

   // now we're pretty much ready to construct the trace buffer entry
   // (into reg2_64bitsafe).  First, clear out that register, since it may
   // contain cruft. [unneeded, right?]
   i = new sparc_instr(sparc_instr::mov, 0, reg2_64bitsafe); 
   em.emit(i);
   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       true, // extended
                       reg2_64bitsafe, // dest
                       sparc_reg::g7, 32);
   em.emit(i);
   i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_or,
                       false, // no cc
                       reg2_64bitsafe, // dest
                       reg2_64bitsafe, reg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_or,
                       false, // no cc
                       reg2_64bitsafe, // dest
                       reg2_64bitsafe, regContainingOpData);
   em.emit(i);

   // finally, store the trace buffer entry
   i = new sparc_instr(sparc_instr::store, sparc_instr::stExtended,
                       reg2_64bitsafe, reg1, 0);
   em.emit(i);

   em.emitLabel(trace_skip_labelname);
}

void traceBuffer::emit_stop_logging(tempBufferEmitter &em,
                                    sparc_reg reg0, sparc_reg reg1) {
   em.emitImm32(traceBufferIterKernelAddr, reg0);

   // load old value of the iterator...
   instr_t *i = new sparc_instr(sparc_instr::load, sparc_instr::ldUnsignedWord,
				reg0, 0, reg1);
   em.emit(i);

   // ...and write it to the post-stop-iterator:
   i = new sparc_instr(sparc_instr::store, sparc_instr::stWord,
		       reg1, reg0, 4);
   em.emit(i);
   
   // Finally, set the iterator to NULL
   i = new sparc_instr(sparc_instr::store, sparc_instr::stWord,
                       sparc_reg::g0, // src value
                       reg0, 0);
   em.emit(i);
}

void traceBuffer::generateGetCurrPidx(tempBufferEmitter &em,
				      bool in_kerninstd_only,
				       // if true, then just kludge since we're
                                       // not running in the kernel
				      sparc_reg destreg) {
   extern machineInfo *theGlobalKerninstdMachineInfo;
   const unsigned procp_offset_within_kthread = 
       theGlobalKerninstdMachineInfo->getKernelThreadProcOffset();
   const unsigned pidinfo_offset_within_proc =
       theGlobalKerninstdMachineInfo->getKernelProcPidOffset();
   const unsigned pidid_offset_within_pid =
       theGlobalKerninstdMachineInfo->getKernelPidIdOffset();

   // Emit code to get the currently running kthread's pid.  It sucks that this
   // requires 3 loads of non-contiguous addresses, meaning 3 data cache blocks.
   // Did I mention that this sucks?

   instr_t *i;
   if (in_kerninstd_only) {
      // kludge -- pretend %g7 is the pid
      i = new sparc_instr(sparc_instr::mov, sparc_reg::g7, destreg);
      em.emit(i);
   }
   else {
      // Current thread is in %g7, period.  Yea!
      em.emitLoadKernelNative(sparc_reg::g7, procp_offset_within_kthread,
			      destreg);

      em.emitLoadKernelNative(destreg, pidinfo_offset_within_proc,
			      destreg);

      // pid is a 32-bit integer, right?
      i = new sparc_instr(sparc_instr::load, sparc_instr::ldUnsignedWord,
                          destreg, pidid_offset_within_pid,
                          destreg);
      em.emit(i);
   }
}

#endif // sparc_solaris

