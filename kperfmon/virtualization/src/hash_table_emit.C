// hash_table_emit.C

#include "hash_table_emit.h"
#include "hash_table.h" // for hash_table_num_bins
#include "regSetManager.h"
#include "util/h/minmax.h"
#include "fnRegInfo.h"
#include "sparc_instr.h"

void emit_initialize_hashtable(tempBufferEmitter &em,
                               dptr_t hashTable_kerninstdAddr) {
   // set %o0 to hash table address [in kerninstd]
   em.emitImmAddr(hashTable_kerninstdAddr, sparc_reg::o0);

   // set %o1 to hash_table_offset_to_numelems
   em.emitImm32(hash_table_offset_to_numelems, sparc_reg::o1);

   // set num_elems to zero
   em.emitStoreKernelNative(sparc_reg::g0, // src value
			    sparc_reg::o0, sparc_reg::o1);

   // set %o2 to hash_table_offset_to_bins
   em.emitImm32(hash_table_offset_to_bins, sparc_reg::o2);

   // %o3 will be bins_iter
   instr_t *i = new sparc_instr(sparc_instr::add, sparc_reg::o0, sparc_reg::o2,
				sparc_reg::o3);
   em.emit(i);

   // %o4 will be bins_finish
   em.emitImm32(hashtable_bin_nbytes*hash_table_num_bins, sparc_reg::o4);
   i = new sparc_instr(sparc_instr::add, sparc_reg::o3, sparc_reg::o4, 
		       sparc_reg::o4);
   em.emit(i);
   
   // while bins_iter != bins_finish do: *bins_iter++ = NULL

   const tempBufferEmitter::insnOffset loopstart_offset = em.currInsnOffset();
   
   // bins_iter == bins_finish?
   i = new sparc_instr(sparc_instr::sub, sparc_reg::o4, sparc_reg::o3, 
		       sparc_reg::o5);
   em.emit(i);
      // bins_finish minus bins_iter into %o5
   
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regLessOrEqZero,
		       false, // not annulled
		       false, // predict not taken
		       sparc_reg::o5,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "done");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   em.emitStoreKernelNative(sparc_reg::g0, // the constant "NULL"
			    sparc_reg::o3, 0); // write to *bins_iter

   // advance bins_iter:
   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
                       false, // NOT annulled
                       true, // predict taken
                       true, // xcc (matters?)
                       -(em.currInsnOffset() - loopstart_offset));
   em.emit(i);
   i = new sparc_instr(sparc_instr::add,
                       sparc_reg::o3, hashtable_bin_nbytes, sparc_reg::o3);
   em.emit(i);
      // delay slot -- bump up the iterator
   
   em.emitLabel("done");

   i = new sparc_instr(sparc_instr::retl);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
}

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------

static unsigned hashFuncNumVReg32sNeeded() {
   return 2;
}

void emit_hashfunc(tempBufferEmitter &em,
                   bool inlinedEmit,
                   const fnRegInfo &theFnRegInfo) {
   const sparc_reg ikey_reg = theFnRegInfo.getParamRegByName("key");
   const sparc_reg result_reg = theFnRegInfo.getParamRegByName("result");

   const sparc_reg_set &availRegs = theFnRegInfo.getAvailRegsForFn();
   assert(!availRegs.exists(ikey_reg));
   assert(!availRegs.exists(result_reg));
   
   const regSetManager theRegSetMgr(availRegs,
                                    false, // don't save; assert enough
                                    0, // no vreg64's needed
                                    2, // num vreg32's needed
                                    0, // num robust reg32's needed
                                    sparc_reg_set::emptyset,
                                    em.getABI());
   
   const sparc_reg key_reg = theRegSetMgr.getNthVReg32(0);
   const sparc_reg vreg32_1 = theRegSetMgr.getNthVReg32(1);
   
   // Because result_reg may equal ikey_reg, we must move ikey_reg to a safe spot
   // (another reg: key_reg) before initializing result_reg to zero.
   
   instr_t *i = new sparc_instr(sparc_instr::mov, ikey_reg, key_reg);
   em.emit(i);
   i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, result_reg);
   em.emit(i);

   const tempBufferEmitter::insnOffset loopstart_offset = em.currInsnOffset();
   
   // if key == 0 then goto done
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       key_reg,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "hashfunc_done");
   // haven't forgotten the delay slot; see next insn
   
   // result = (result << 1) + result + (key & 0x3)
   // or, result += (result << 1) + (key & 0x3)
   // or: (1) result += (result << 1); (2) result += (key & 0x3)
   
   // set vreg32_1 to (result << 1)
   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       false, // not extended
                       vreg32_1, // dest
                       result_reg, 1);
   em.emit(i);
   i = new sparc_instr(sparc_instr::add, result_reg, vreg32_1, result_reg);
   em.emit(i);
   
   // set vreg32_1 to (key & 0x3)
   i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_and, 
		       false, // no cc
                       vreg32_1, // dest
                       key_reg, 0x3);
   em.emit(i);
      // delay slot of the above bpr -- only needed if falling thru, but harmless if not
   i = new sparc_instr(sparc_instr::add, result_reg, vreg32_1, result_reg);
   em.emit(i);

   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
                       false, // not annulled
                       true, // predict taken
                       true, // xcc (matters?)
                       -(em.currInsnOffset() - loopstart_offset));
   em.emit(i);
   // delay slot: key >>= 2
   i = new sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
                       false, // not extended
                       key_reg, // dest
                       key_reg, 2);
   em.emit(i);
     // delay slot insn

   em.emitLabel("hashfunc_done");

   if (!inlinedEmit) {
      i = new sparc_instr(sparc_instr::retl);
      em.emit(i);
      i = new sparc_instr(sparc_instr::nop);
      em.emit(i);
         // result_reg has already been set
   }
}

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------

void emit_hashtable_pack(tempBufferEmitter &em,
                         kptr_t hashTable_kernelAddr,
                         const fnRegInfo &theFnRegInfo) {
   // We actually *want* to emit a save, since pack isn't called often, so best to
   // be extra safe with register usage.
   // (Note that emitting a save does *not* relieve add() of the need to save & restore
   // its %o7 register around the call to pack()!)
   const regSetManager theRegSetMgr(sparc_reg_set::emptyset,
                                    true, // save if needed
                                    0, // vreg64's desired
                                    5, // vreg32's desired
                                    0, // robust reg32's desired
                                    sparc_reg_set::emptyset,
                                    em.getABI());

   sparc_reg dest_reg = theRegSetMgr.getNthVReg32(0);
   sparc_reg src_reg = theRegSetMgr.getNthVReg32(1);
   
   sparc_reg vreg0 = theRegSetMgr.getNthVReg32(2);
   sparc_reg vreg1 = theRegSetMgr.getNthVReg32(3);
   sparc_reg vreg2 = theRegSetMgr.getNthVReg32(4);

   sparc_reg_set checkset;
   checkset += dest_reg;
   checkset += src_reg;
   checkset += vreg0;
   checkset += vreg1;
   checkset += vreg2;
   assert(checkset.countIntRegs() == 5);
   assert(checkset.countIntRegs() == checkset.count());
   
   // ready to start emitting
   instr_t *i;
   if (theRegSetMgr.needsSaveRestore()) {
      i = new sparc_instr(sparc_instr::save,
                          -em.getABI().getMinFrameAlignedNumBytes());
      em.emit(i);   
   }
   // assert(currnumelems == maxnumelems)
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_numelems, vreg0);
   em.emitLoadKernelNative(vreg0, 0, vreg0);
   em.emitImm32(hash_table_max_num_elems, vreg1);
   i = new sparc_instr(sparc_instr::sub, true, // cc
                       false, // no carry
                       vreg0, // dest
                       vreg1, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                       false, // not extended
                       0x34);
   em.emit(i);
   
   // initialize dest and src robust registers
   em.emitImmAddr(hashTable_kernelAddr, dest_reg);
   i = new sparc_instr(sparc_instr::mov, dest_reg, src_reg);
   em.emit(i);

   // clear out all of the bins: vreg0 will be the iterator; vreg1 will be finish
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_bins, vreg0);
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_bins +
		  hashtable_bin_nbytes*hash_table_num_bins, vreg1);
   
   const tempBufferEmitter::insnOffset clearbins_startoffset = em.currInsnOffset();

   // if vreg0 >= vreg1 then goto after_clearbins

   i = new sparc_instr(sparc_instr::sub, vreg1, vreg0, vreg2);
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regLessOrEqZero,
		       false, // not annulled
		       false, // predict not taken
		       vreg2, 0);
   em.emitCondBranchToForwardLabel(i, "after_clearbins");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   em.emitStoreKernelNative(sparc_reg::g0, // src value: NULL
			    vreg0, 0);
   
   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
                       false, // not annulled
                       true, true,
                          // predict taken, xcc
                       -(em.currInsnOffset() - clearbins_startoffset));
   em.emit(i);
   i = new sparc_instr(sparc_instr::add, vreg0, hashtable_bin_nbytes, vreg0);
   em.emit(i);
      // delay slot -- bump up bins_iter

   em.emitLabel("after_clearbins");
   
   // main loop here

   const tempBufferEmitter::insnOffset loopstartoffset = em.currInsnOffset();

   // if src == finish then goto after_loop
   em.emitImmAddr(hashTable_kernelAddr + 
		  hash_table_elem_nbytes*hash_table_max_num_elems, vreg0);
   i = new sparc_instr(sparc_instr::sub, vreg0, src_reg, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       vreg0, 0);
   em.emitCondBranchToForwardLabel(i, "after_loop");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // if src->removed then goto nextsrc
   em.emitLoadKernelNative(src_reg, hash_table_elem_removed_field_offset, 
			   vreg0);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regNotZero,
		       false, // not annulled
		       false, // predict not taken
		       vreg0, 0);
   em.emitCondBranchToForwardLabel(i, "nextsrc");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   // dest->key = src->key; dest->val = src->val; dest->removed = 0
   em.emitLoadKernelNative(src_reg, hash_table_elem_key_field_offset, vreg1);
      // keep src->key in vreg1, in preparation for call to hash fn, below

   em.emitStoreKernelNative(vreg1, dest_reg, hash_table_elem_key_field_offset);
   
   em.emitLoadKernelNative(src_reg, hash_table_elem_data_field_offset, vreg0);
   em.emitStoreKernelNative(vreg0, 
			    dest_reg, hash_table_elem_data_field_offset);

   em.emitStoreKernelNative(sparc_reg::g0, 
			    dest_reg, hash_table_elem_removed_field_offset);
   
   // Make an inlined call to the hash function.
   // avail regs: avail_int_regs on entry minus dest_reg, src_reg

   const sparc_reg_set avail_int_regs_for_hashfunc =
      theFnRegInfo.getAvailRegsForFn() - dest_reg - src_reg;
   
   emit_hashfunc(em, true, // inlined emit
                 fnRegInfo(true, // inlined
                           fnRegInfo::regsAvailAtCallTo,
                           avail_int_regs_for_hashfunc,
                              // vreg1 param will get subtracted for us
                           fnRegInfo::blankParamInfo +
                           make_pair(pdstring("key"), vreg1) +
                           make_pair(pdstring("result"), vreg1), // put result here
                           sparc_reg::g0));

   em.emitImm32(hash_table_mask_for_mod_numbins, vreg0);
   i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_and,
                       false, // no cc
                       vreg0, // destination -- the bin number
                       vreg1, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       false, // not extended
                       vreg0, // dest
                       vreg0, hashtable_bin_log_nbytes);
   em.emit(i);
   assert(hashtable_bin_nbytes == sizeof(kptr_t));
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_bins, vreg1);
   i = new sparc_instr(sparc_instr::add, vreg0, vreg1, vreg0);
   em.emit(i);
   
   // vreg0 contains &bins[bin]

   em.emitLoadKernelNative(vreg0, 0, vreg1);
   
   // vreg1 contains bins[bin]

   em.emitStoreKernelNative(vreg1,
			    dest_reg, hash_table_elem_next_field_offset);
   em.emitStoreKernelNative(dest_reg,
			    vreg0, 0);
   
   i = new sparc_instr(sparc_instr::add, dest_reg, hash_table_elem_nbytes,
		       dest_reg);
   em.emit(i);
   
   em.emitLabel("nextsrc");
   
   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
                       false, // not annulled
                       true, // predict taken
                       true, // xcc
                       -(em.currInsnOffset() - loopstartoffset));
   em.emit(i);
   i = new sparc_instr(sparc_instr::add, src_reg, hash_table_elem_nbytes, 
		       src_reg);
   em.emit(i);
      // delay slot -- bump up src_reg

   em.emitLabel("after_loop");
   
   // left to do: currnumelems = dest - &all_elems[0].
   // We get all_elems[0] by simply doing a setImm32() of the hash table's addr

   // assert that src >= dest...so src-dest should be >= 0
   i = new sparc_instr(sparc_instr::sub, src_reg, dest_reg, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNegative,
                       false, // not extended
                       0x34);
   em.emit(i);
   
   em.emitImmAddr(hashTable_kernelAddr, vreg0);
   i = new sparc_instr(sparc_instr::sub, dest_reg, vreg0, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::shift, sparc_instr::rightLogical,
                       false, // not extended
                       vreg0, // dest
                       vreg0, hash_table_elem_log_nbytes); 
   em.emit(i); // divide 
   
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_numelems, vreg1);
   em.emitStoreKernelNative(vreg0, // src
			    vreg1, 0);

   if (theRegSetMgr.needsSaveRestore()) {
      i = new sparc_instr(sparc_instr::ret);
      em.emit(i);
      i = new sparc_instr(sparc_instr::restore);
      em.emit(i);
   }
   else {
      i = new sparc_instr(sparc_instr::retl);
      em.emit(i);
      i = new sparc_instr(sparc_instr::nop);
      em.emit(i);
   }
}

static unsigned addPart1NumVReg32sNeeded() {
   return max(hashFuncNumVReg32sNeeded(), 2U);
}

static void emit_hashtable_add_part1(tempBufferEmitter &em,
                                     kptr_t hashTable_kernelAddr,
                                     const fnRegInfo &theFnRegInfo,
                                     bool emittedASave) {
   // In "part 1" of add, we emit code to search for an existing element with
   // matching key.  If found and the removed flag is clear, then twiddle the flag,
   // write the new value, and return success.  (No need to update binning, since the
   // element was never un-binned when the removed flag was set to true.)

   sparc_reg key_reg = theFnRegInfo.getParamRegByName("key");
   sparc_reg value_reg = theFnRegInfo.getParamRegByName("value");
   sparc_reg binptr_reg = theFnRegInfo.getParamRegByName("binptr");
   sparc_reg result_reg = theFnRegInfo.getResultReg();

   // emit hash function (inlined), putting result into binptr_reg
   emit_hashfunc(em, true, // inlined emit
                 fnRegInfo(true, // inlined, so don't assume %o7 isn't free
                           fnRegInfo::regsAvailAtCallTo,
                           theFnRegInfo.getAvailRegsForFn(),
                           fnRegInfo::blankParamInfo +
                           make_pair(pdstring("key"), key_reg) +
                           make_pair(pdstring("result"), binptr_reg), // put result here
                           sparc_reg::g0));

   const sparc_reg_set &availRegs = theFnRegInfo.getAvailRegsForFn();
   assert(!availRegs.exists(key_reg));
   assert(!availRegs.exists(binptr_reg));
   const regSetManager theRegSetMgr(availRegs,
                                    false, // don't save; assert enough regs, since
                                       // after a save, key_reg and/or value_reg would
                                       // be unreachable unless they were each %o regs.
                                    0, // num vreg64's needed
                                    2, // num vreg32's needed
                                    0, // num robust reg32's needed
                                    sparc_reg_set::emptyset,
                                       // we don't allocate the result reg if needed,
                                       // because we're not going to use it until
                                       // the last moment anyway.
                                    em.getABI());

   sparc_reg vreg32_0 = theRegSetMgr.getNthVReg32(0);
   sparc_reg elemptr_reg = theRegSetMgr.getNthVReg32(1);

   // binptr_reg contains hash value; we need to mod it by numbins as usual:
   em.emitImm32(hash_table_mask_for_mod_numbins, vreg32_0);
   instr_t *i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_and, 
				false, // no cc
				binptr_reg, // dest
				vreg32_0, binptr_reg);
   em.emit(i);

   // Right now, binptr_reg contains the bin.  Let's turn it into &bins[bin]
   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       false, // not extended
                       binptr_reg, // dest
                       binptr_reg, hashtable_bin_log_nbytes);
   em.emit(i);
   assert(hashtable_bin_nbytes == sizeof(kptr_t));
   
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_bins, vreg32_0);
   i = new sparc_instr(sparc_instr::add, vreg32_0, binptr_reg, binptr_reg);
   em.emit(i);
   
   // Now binptr_reg finally contains &bins[bin], as we want
   
   // Initialize elemptr_reg by dereferencing binptr_reg.  This could yield NULL.
   em.emitLoadKernelNative(binptr_reg, 0, elemptr_reg);
   
   const tempBufferEmitter::insnOffset loopstartoffset = em.currInsnOffset();
   
   // If elemptr_reg == NULL then goto notfound
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       elemptr_reg,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "hash_add_part1_notfound");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   // Load key; see if it matches key_reg; if not then goto next & retry loop
   em.emitLoadKernelNative(elemptr_reg, hash_table_elem_key_field_offset,
			   vreg32_0);
   i = new sparc_instr(sparc_instr::sub, vreg32_0, key_reg, vreg32_0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpr,
                       sparc_instr::regNotZero,
                       true, // annulled
                       false, // predict not taken
                       vreg32_0,
                       -(em.currInsnOffset() - loopstartoffset));
   em.emit(i);
   em.emitLoadKernelNative(elemptr_reg, hash_table_elem_next_field_offset,
			   elemptr_reg);
      // delay slot -- goto next element -- only execute if bpr is taken
   
   // The key matches.  Check the removed flag; if 0 then goto duplicate
   em.emitLoadKernelNative(elemptr_reg, hash_table_elem_removed_field_offset,
			   vreg32_0);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       false, // predict not taken
		       vreg32_0,
		       0 // displacement for now
		       );
   em.emitCondBranchToForwardLabel(i, "hash_add_part1_duplicate");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   // The key matches, and the removed flag was set.
   // Clear it, write the value, (don't need to change the key), (don't need
   // to re-bin since it was never un-binned when the removed flag was set originally.)
   em.emitStoreKernelNative(sparc_reg::g0, // value to write (!removed)
			    elemptr_reg, hash_table_elem_removed_field_offset);
   em.emitStoreKernelNative(value_reg, // value to write
			    elemptr_reg, hash_table_elem_data_field_offset);

   // return "success" (1)
   if (emittedASave) {
      i = new sparc_instr(sparc_instr::ret);
      em.emit(i);
      i = new sparc_instr(sparc_instr::restore, sparc_reg::g0, 0x1,
                          result_reg.regLocationAfterARestore());
      em.emit(i);
   }
   else {
      i = new sparc_instr(sparc_instr::retl);
      em.emit(i);
      i = new sparc_instr(sparc_instr::mov, 1, result_reg);
      em.emit(i);
   }

   // DUPLICATE:
   // return failure (0)
   em.emitLabel("hash_add_part1_duplicate");
   if (emittedASave) {
      i = new sparc_instr(sparc_instr::ret);
      em.emit(i);
      i = new sparc_instr(false, sparc_instr::restore, sparc_reg::g0, 
			  sparc_reg::g0,
                          result_reg.regLocationAfterARestore());

      em.emit(i);   
   }
   else {
      i = new sparc_instr(sparc_instr::retl);
      em.emit(i);
      i = new sparc_instr(sparc_instr::mov, 0, result_reg);
      em.emit(i);
   }

   // NOTFOUND: (fall through)
   em.emitLabel("hash_add_part1_notfound");
}

static unsigned addPart2NumVReg32sNeeded() {
   return 3;
}

static fnRegInfo emit_hashtable_add_part2(tempBufferEmitter &em,
                                          kptr_t hashTable_kernelAddr,
                                          const fnRegInfo &theFnRegInfo,
                                          bool emittedSaveRestore) {
   // returns fnRegInfo for the call to hash table pack
   sparc_reg key_reg = theFnRegInfo.getParamRegByName("key");
   sparc_reg value_reg = theFnRegInfo.getParamRegByName("value");
   sparc_reg binptr_reg = theFnRegInfo.getParamRegByName("binptr");
   sparc_reg result_reg = theFnRegInfo.getResultReg();
   
   const regSetManager theRegSetMgr(theFnRegInfo.getAvailRegsForFn(),
                                    false, // don't save; assert enough regs
                                    0, // num vreg64's needed
                                    4, // num vreg32's needed
                                    0, // num robust reg32's needed
                                    sparc_reg_set::emptyset,
                                       // we don't reserve result_reg even if needed,
                                       // since we're not going to use it until the
                                       // last moment before returning.
                                    em.getABI());
   
   sparc_reg vreg32_0 = theRegSetMgr.getNthVReg32(0);
   sparc_reg vreg32_1 = theRegSetMgr.getNthVReg32(1);
   sparc_reg elemptr_reg = theRegSetMgr.getNthVReg32(2);
   sparc_reg savelink_reg = theRegSetMgr.getNthVReg32(3);
      // even if pack() does its own save/restore, we still need to
      // make sure that we can return to whoever called *us*.  Toward
      // that end, we must not let our call to pack() overwrite that %o7.
      // So we can either do a save/restore here (not an option, too expensive)
      // or we can manually manage saving of %o7.

   // Check for overflow: load numelems, compare to hash_table_max_num_elems
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_numelems, 
		  vreg32_0);
   em.emitLoadKernelNative(vreg32_0, 0, vreg32_1);

   em.emitImm32(hash_table_max_num_elems, vreg32_0);

   instr_t *i = new sparc_instr(sparc_instr::sub, vreg32_0, vreg32_1, 
				vreg32_0);
   em.emit(i); // maxelems - numelems

   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regGrZero,
		       false, // not annulled
		       true, // predict taken
		       vreg32_0,
		       0 // displacement, for now
		       );
   em.emitCondBranchToForwardLabel(i, "no_need_to_pack");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   // call pack -- no arguments needed.  save/restore %o7 manually, so we'll be able
   // to return to whoever called us!
   i = new sparc_instr(sparc_instr::mov, sparc_reg::o7, savelink_reg);
   em.emit(i);
   em.emitSymAddr("hashTablePackAddr", vreg32_0);
   i = new sparc_instr(sparc_instr::callandlink, vreg32_0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   i = new sparc_instr(sparc_instr::mov, savelink_reg, sparc_reg::o7);
   em.emit(i);

   const fnRegInfo
      fnRegInfoForHashTablePack(false, // not inlined
                                fnRegInfo::regsAvailAtCallTo,
                                theRegSetMgr.getAdjustedParentSet() - savelink_reg,
                                   // savelink_reg is the only written reg in part 2
                                   // that needs saving across the call to pack.
                                fnRegInfo::blankParamInfo,
                                   // no params to this fn
                                sparc_reg::g0
                                   // no return value from this fn
                                );
   
   // after the call to pack, did it free up enough space?
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_numelems, 
		  vreg32_0);
   em.emitLoadKernelNative(vreg32_0, 0, vreg32_1);

   em.emitImm32(hash_table_max_num_elems, vreg32_0);
   i = new sparc_instr(sparc_instr::sub, vreg32_0, vreg32_1, vreg32_0);
   em.emit(i); // maxelems - numelems
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regGrZero,
		       false, // not annulled
		       true, // predict taken
		       vreg32_0, 0);
   em.emitCondBranchToForwardLabel(i, "no_need_to_pack");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   // overflow -- return 0
   if (emittedSaveRestore) {
      i = new sparc_instr(sparc_instr::ret);
      em.emit(i);
      i = new sparc_instr(false, sparc_instr::restore, sparc_reg::g0, 
			  sparc_reg::g0,
                          result_reg.regLocationAfterARestore());
      em.emit(i);
   }
   else {
      i = new sparc_instr(sparc_instr::retl);
      em.emit(i);
      i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, result_reg);
      em.emit(i);
   }
   
   em.emitLabel("no_need_to_pack");
   
   // append to all_elems.
   // NOTE: vreg32_1 contains current num elems
   // set elemptr_reg to be a pointer to the new element

   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       false, // not extended
                       elemptr_reg, // dest (numelems * 16)
                       vreg32_1, // current num elems
                       hash_table_elem_log_nbytes);
   em.emit(i);
      // shift left to multiply by hash_table_elem_nbytes

   // set elemptr_reg now
   em.emitImmAddr(hashTable_kernelAddr, vreg32_0);
   i = new sparc_instr(sparc_instr::add, vreg32_0, elemptr_reg, elemptr_reg);
   em.emit(i);

   // write the key field:
   em.emitStoreKernelNative(key_reg,
			    elemptr_reg, hash_table_elem_key_field_offset);

   // write the data field:
   em.emitStoreKernelNative(value_reg,
			    elemptr_reg, hash_table_elem_data_field_offset);

   // write the removed field:
   em.emitStoreKernelNative(sparc_reg::g0, // zero
			    elemptr_reg, hash_table_elem_removed_field_offset);

   // ***binptr_reg already contains the bin***, from addPart1 (verify this!)

   // vreg32_0 <-- old value at the bin (a ptr into all_elems)
   em.emitLoadKernelNative(binptr_reg, 0, vreg32_0);

   // elemptr_reg->next = vreg32_0
   // write the "next" field (set it to the old value at the bin)
   em.emitStoreKernelNative(vreg32_0, // src value
			    elemptr_reg, hash_table_elem_next_field_offset);

   // update the bin itself: bins[bin] = e
   // (*binptr_reg) = e
   em.emitStoreKernelNative(elemptr_reg, // src value
			    binptr_reg, 0);

   // ht->num_elems++
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_numelems, 
		  vreg32_0);
   em.emitLoadKernelNative(vreg32_0, 0, vreg32_1);
   i = new sparc_instr(sparc_instr::add, vreg32_1, 1, vreg32_1);
   em.emit(i);
   em.emitStoreKernelNative(vreg32_1, vreg32_0, 0);

   // return 1 (success)
   if (emittedSaveRestore) {
      i = new sparc_instr(sparc_instr::ret);
      em.emit(i);
      i = new sparc_instr(sparc_instr::restore, sparc_reg::g0, 0x1,
                          result_reg.regLocationAfterARestore());
      em.emit(i);
   }
   else {
      i = new sparc_instr(sparc_instr::retl);
      em.emit(i);
      i = new sparc_instr(sparc_instr::mov, 1, result_reg);
      em.emit(i);
         // delay slot -- set return value to the constant 0x1
   }

   return fnRegInfoForHashTablePack;
}

fnRegInfo emit_hashtable_add(tempBufferEmitter &em,
                             const fnRegInfo &theFnRegInfo,
                             kptr_t hashTable_kernelAddr) {
   // returns information needed to emit hash table pack.

   // We don't include pack in the following calculations, meaning that pack will
   // just have to emit a save if needed.  That's fine with us, since it isn't really
   // called that often.
   const unsigned numVReg32s = 1 +
      max(addPart1NumVReg32sNeeded(), addPart2NumVReg32sNeeded());
   
   const regSetManager theRegSetMgr(theFnRegInfo.getAvailRegsForFn(),
                                    true, // save if needed
                                    0, // vreg64's
                                    numVReg32s,
                                    0, // robust reg32's
                                    sparc_reg_set::emptyset,
                                       // what about the result reg?
                                    em.getABI());
   
   sparc_reg key_reg(theFnRegInfo.getParamRegByName("key")); // for now
   sparc_reg value_reg(theFnRegInfo.getParamRegByName("value")); // for now
   sparc_reg result_reg = theFnRegInfo.getResultReg();
   if (theRegSetMgr.needsSaveRestore()) {
      // assert that input params are in %o regs, so we can find 'em after the save
      // same for result.
      assert((sparc_reg_set::allOs & key_reg) == key_reg);
      assert((sparc_reg_set::allOs & value_reg) == value_reg);
      assert((sparc_reg_set::allOs & result_reg) == result_reg);

      key_reg = key_reg.regLocationAfterASave();
      value_reg = value_reg.regLocationAfterASave();
      result_reg = result_reg.regLocationAfterASave();

      instr_t *i = new sparc_instr(sparc_instr::save,
				   -em.getABI().getMinFrameAlignedNumBytes());
      em.emit(i);
   }
   else
      assert(!theRegSetMgr.getAdjustedParentSet().exists(sparc_reg::o7));

   sparc_reg binptr_reg = theRegSetMgr.getNthVReg32(0);
   assert(key_reg != binptr_reg);
   assert(value_reg != binptr_reg);

   // Emit part 1 (inlined): search for element with matching key.  If found and
   // removed flag is set, then unset the flag, update value, and return success.
   // If found and removed flag is clear, then assert fail (duplicate).
   // Else, fall through.
   emit_hashtable_add_part1(em,
                            hashTable_kernelAddr,
                            fnRegInfo(true, // inlined
                                      fnRegInfo::regsAvailAtCallTo,
                                      theRegSetMgr.getAdjustedParentSet(),
                                      // don't need to subtract anything beyond the
                                      // params, since nothing else has been defined
                                      fnRegInfo::blankParamInfo +
                                      make_pair(pdstring("key"), key_reg) +
                                      make_pair(pdstring("value"), value_reg) +
                                      make_pair(pdstring("binptr"), binptr_reg),
                                      result_reg),
                            theRegSetMgr.needsSaveRestore()
                            );

   // Emit part2 (inlined)
   const fnRegInfo theFnRegInfoForHashTablePack =
      emit_hashtable_add_part2(em,
                               hashTable_kernelAddr,
                               fnRegInfo(true, // inlined
                                         fnRegInfo::RegsAvailAtCallTo(),
                                         theRegSetMgr.getAdjustedParentSet(),
                                         // don't need to subtract anything beyond the
                                         // params
                                         fnRegInfo::blankParamInfo +
                                         make_pair(pdstring("key"), key_reg) +
                                         make_pair(pdstring("value"), value_reg) +
                                         make_pair(pdstring("binptr"), binptr_reg),
                                         result_reg),
                               theRegSetMgr.needsSaveRestore());

   return theFnRegInfoForHashTablePack;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static void
emit_hash_table_keyandbin2NonRemovedElemPtr_inlined(tempBufferEmitter &em,
                                                    kptr_t hashTable_kernelAddr,
                                                    const fnRegInfo &theFnRegInfo) {
   // This routines takes in a key and bin and returns a pointer to the non-removed
   // element in the hash table with matching key, or NULL if not found.  (Also, NULL
   // if found but removed flag is set.)
   
   sparc_reg key_reg = theFnRegInfo.getParamRegByName("key");
   sparc_reg bin_reg = theFnRegInfo.getParamRegByName("bin");
   assert(key_reg != bin_reg);
   sparc_reg result_reg = theFnRegInfo.getResultReg();

   const regSetManager theRegSetMgr(theFnRegInfo.getAvailRegsForFn(),
                                    false, // no save -- assert enough regs
                                    0, // num vreg64's needed
                                    3, // num vreg32's needed
                                    0, // num robust reg32's needed
                                    sparc_reg_set::emptyset,
                                    em.getABI());

   sparc_reg elemptr_reg = theRegSetMgr.getNthVReg32(0);
   sparc_reg vreg0 = theRegSetMgr.getNthVReg32(1);
   sparc_reg vreg1 = theRegSetMgr.getNthVReg32(2);

   // assert that the bin is in range [0...hash_table_num_bins-1]
   em.emitImm32(hash_table_num_bins-1, vreg0);
   instr_t *i = new sparc_instr(sparc_instr::sub, true, false, // cc, no carry
                                vreg0, // dest
                                vreg0, bin_reg);
   em.emit(i);
      // (hash_table_num_bins-1) minus the bin...result should not be negative
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNegative,
                       false, // not extended
                       0x34);
   em.emit(i);

   // set vreg0 to the address of the bin
   em.emitImmAddr(hashTable_kernelAddr + hash_table_offset_to_bins, vreg0);
   i = new sparc_instr(sparc_instr::shift, sparc_instr::leftLogical,
                       false, // not extended
                       vreg1, // dest
                       bin_reg, hashtable_bin_log_nbytes);
   em.emit(i);
   assert(hashtable_bin_nbytes == sizeof(kptr_t));

   i = new sparc_instr(sparc_instr::add, vreg0, vreg1, vreg0);
   em.emit(i);

   // load the bin into elemptr_reg
   em.emitLoadKernelNative(vreg0, 0, elemptr_reg);

   // start of loop:
   const tempBufferEmitter::insnOffset loopstartoffset = em.currInsnOffset();

   // if result==NULL then break out of the loop
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       true, // annulled
		       false, // predict not taken
		       elemptr_reg, 0);
   em.emitCondBranchToForwardLabel(i, "emit_hash_table_keyandbin2elem_fallthru");
   i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, result_reg);
   em.emit(i);
      // delay slot -- set result to NULL (only if taken)
   
   // if result->the_key == key then break out of the loop (goto "match")
   em.emitLoadKernelNative(elemptr_reg, hash_table_elem_key_field_offset,
			   vreg0);
   i = new sparc_instr(sparc_instr::sub, vreg0, key_reg, vreg0);
   em.emit(i);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
		       false, // not annulled
		       true, // predict taken
		       vreg0, 0);
   em.emitCondBranchToForwardLabel(i, "emit_hash_table_keyandbin2elem_key_match");
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);
   
   // e = e->next; retry loop
   i = new sparc_instr(sparc_instr::bpcc, sparc_instr::iccAlways,
                       false, // not annulled
                       true, true,
                       // predict taken, xcc
                       -(em.currInsnOffset() - loopstartoffset));
   em.emit(i);
   em.emitLoadKernelNative(elemptr_reg, hash_table_elem_next_field_offset,
			   elemptr_reg);
      // delay slot: e = e->next

   em.emitLabel("emit_hash_table_keyandbin2elem_key_match");

   // the keys match, so we're *almost* ready to assume success.  The only thing
   // standing in our way is the removed field.  If set then goto fail.
   em.emitLoadKernelNative(elemptr_reg, hash_table_elem_removed_field_offset,
			   vreg0);
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regNotZero,
		       true, // annulled
		       false,
		       vreg0, 0);
   em.emitCondBranchToForwardLabel(i, "emit_hash_table_keyandbin2elem_fallthru");
   i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, result_reg);
   em.emit(i);
      // delay slot (only if taken): set result to NULL
   
   // the keys match, and the removed field is clear.  Success!
   i = new sparc_instr(sparc_instr::mov, elemptr_reg, result_reg);
   em.emit(i);

   em.emitLabel("emit_hash_table_keyandbin2elem_fallthru");
}

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------

void emit_hashtable_remove(tempBufferEmitter &em,
                           kptr_t hashTableKernelAddr,
                           const fnRegInfo &theFnRegInfo // avail regs, params, result
                           ) {
   sparc_reg key_reg = theFnRegInfo.getParamRegByName("key");
   sparc_reg result_reg = theFnRegInfo.getResultReg();

   const regSetManager theRegSetMgr(theFnRegInfo.getAvailRegsForFn(),
                                    false, // don't save; assert enough regs exist
                                    0, // num vreg64's needed
                                    2, // num vreg32's needed
                                    0, // num robust reg32's needed
                                    sparc_reg_set::emptyset,
                                    em.getABI());
   sparc_reg vreg32_0 = theRegSetMgr.getNthVReg32(0);
   sparc_reg vreg32_1 = theRegSetMgr.getNthVReg32(1);
   
   // [Inlined] call to the hash function, with key_reg as the argument.
   emit_hashfunc(em, true, // inlined emit
                 fnRegInfo(true, // inlined
                           fnRegInfo::regsAvailAtCallTo,
                           theFnRegInfo.getAvailRegsForFn(),
                              // params'll get removed for us
                           fnRegInfo::blankParamInfo +
                           make_pair(pdstring("key"), key_reg) +
                           make_pair(pdstring("result"), vreg32_0), // put result here
                           sparc_reg::g0));

   // The result, and'd with hash_table_mask_for_mod_numbins, gives the bin
   em.emitImm32(hash_table_mask_for_mod_numbins, vreg32_1);
   instr_t *i = new sparc_instr(sparc_instr::logic, sparc_instr::logic_and, false, // no cc
                       vreg32_0, // dest (bin)
                       vreg32_0, vreg32_1);
   em.emit(i);
   
   // call keyandbin2elem (args: key_reg, vreg32_0) [inlined]
   emit_hash_table_keyandbin2NonRemovedElemPtr_inlined
      (em, hashTableKernelAddr,
       fnRegInfo(true, // inlined
                 fnRegInfo::regsAvailAtCallTo,
                 theFnRegInfo.getAvailRegsForFn(),
                    // params'll get removed for us
                 fnRegInfo::blankParamInfo +
                 make_pair(pdstring("key"), key_reg) +
                 make_pair(pdstring("bin"), vreg32_0),
                 vreg32_0 // result reg
                 ));

   // if result (vreg32_0) is NULL then failure
   i = new sparc_instr(sparc_instr::bpr, sparc_instr::regZero,
                       true, // annulled
                       false, // predict not taken
                       vreg32_0, 0);
   em.emitCondBranchToForwardLabel(i, "hash_remove_finish");
   i = new sparc_instr(sparc_instr::mov, sparc_reg::g0, result_reg);
   em.emit(i);
      // delay slot (only if taken): set result to NULL

   // if removed field is set then assert fail (keyandbin2elem shouldn't have returned
   // such a thing)
   em.emitLoadKernelNative(vreg32_0, hash_table_elem_removed_field_offset,
			   vreg32_1);
   i = new sparc_instr(sparc_instr::tst, vreg32_1);
   em.emit(i);
   i = new sparc_instr(sparc_instr::trap, sparc_instr::iccNotEq,
                       false, // not extended
                       0x34);
   em.emit(i);

   // set removed field to true
   i = new sparc_instr(sparc_instr::mov, 0x1, vreg32_1);
   em.emit(i);
   em.emitStoreKernelNative(vreg32_1, // src value
			    vreg32_0, hash_table_elem_removed_field_offset);

   em.emitLoadKernelNative(vreg32_0, hash_table_elem_data_field_offset,
			   result_reg);
      // return value: e->the_data

   em.emitLabel("hash_remove_finish");

   // result has already been set
   i = new sparc_instr(sparc_instr::retl);
   em.emit(i);
   i = new sparc_instr(sparc_instr::nop);
   em.emit(i);

   i = new sparc_instr(sparc_instr::trap, 0x34);
   em.emit(i);
}
