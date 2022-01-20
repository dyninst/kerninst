// sparc_controlFlow.C

#ifdef _KERNINSTD_

#include "sparc_controlFlow.h"
#include "sparc_kmem.h"
#include "simplePathSlice.h"
#include "funkshun.h"
#include "sparcTraits.h"
#include "sparc_misc.h" // readFromKernelInto()
#include <algorithm> // STL's binary_search()
#include "util/h/popc.h" // ari_popc()
#include "simplePath.h"
#include "out_streams.h"
#include "parseOracle.h"

// See funkshun.h for open issues and outright bugs in the control flow graph
// parsing.  Put simply, we need to better handle interprocedural branches
// (put them on edges), and we need to do a better job handling delay slot
// instructions that are also the destination of some other branch.

const bool bbEndsOnANormalCall = false;
// false --> the old way.  Violates the strict definition of a basic block,
// in favor of this definition: once entered, all insns will be executed.
// This alternate definition definitely has its uses: for example, it's the right
// granularity for basic block code coverage -- under the reasonable assumption
// that all calls in the middle of the basic block will return.  (Note that a
// call;restore will always be at the end of a basic block, so don't worry.)
// And it's good enough for register analysis.
// And it reduces the number of basic blocks.  I'd like to find out by exactly
// how much, but for right now, setting the flag to true crashes when parsing
// prioctl on Solaris7/UltraSPARC/32-bit at least.  At the moment I can't take the
// time to figure out exactly why this is, though I suspect that it has something
// to do with parsing too much of an offset-32 jump table.
//
// true --> a call insn (& its delay slot) will be the last things in a basic
// block.  This may satisfy purists uncomfortable with kerninst straying from
// the strict definition of a basic block.  Unfortunately, at the moment, I can't
// set it to true -- parsing crashes on prioctl (see above).  XXX fix this!!!

static bool handleCallInstr(bbid_t bb_id,
                            instr_t *callInstr,
                            kptr_t &instrAddr,
                            // NEW: A reference.  If returning true, modify this
                            // vrble s.t. it's the addr of the insn where parsing
                            // should continue (usually add 8...the insn plus its
                            // delay slot, right?)
                            sparc_instr::controlFlowInfo *cfi,
                            const simplePath & /*pathFromEntryBB*/,
                            kptr_t stumbleOntoAddr,
                               // next known bb startaddr, or (kptr_t)-1
                            const kmem *km,
                            parseOracle *theParseOracle) {
   // called by handleControlFlowInstr when it detects a call.
   // Returns true iff the basic block parsing should be continued;
   // false if bb has ended.
   // 
   // Note: calls come in two flavors: 'call' and 'jmpl <address>, %o7'
   // Note: some calls are tail-call optimized:
   //       1) call C; restore rs1, rs2, rd [from a stack frame]
   //       2) jmpl [addr], %o7; restore [from a stack frame]
   //       3) mov %o7, tmp; call C; mov tmp, %o7 [from no stack frame]
   //       There are other tail calls, but we don't recognize them in this routine
   //       since they involve jmp, not call/jmpl-to-o7.  They are:
   //       4) jmp [addr] where [addr] is a constant address (sethi/bset)
   //       5) jmp [addr] where [addr] is more general (virtual fn call)
   //       Note that there is a 6th potential case:
   //       6) mov %o7, tmp; jmpl [addr], %o7; mov tmp, %o7
   //       but I doubt that this occurs since it's dumb; any compiler can emit
   //       jmp [addr] instead.  Nevertheless we should at least check for it & warn.
   //
   // We usually include the delay slot as part of this basic block...unless it already
   // belongs to another already-parsed basic block, naturally.  We detect this by
   // checking stumbleOntoAddr.

   const function_t *fn = theParseOracle->getFn();

   // quick assertion checks
   assert(cfi->fields.controlFlowInstruction);
   assert(cfi->delaySlot == instr_t::dsAlways);

   instr_t *delaySlotInsn = km->getinsn(instrAddr+4);
   assert(delaySlotInsn->getDelaySlotInfo() == instr_t::dsNone);
      // possible, but would be messy to handle
   
   // Delay slot is in this block if: (1) it isn't part of
   // another already parsed basic block, and (2) is w/in bounds of this function
   const bool delaySlotInAnotherBlockSameFunction =
      stumbleOntoAddr != (kptr_t)-1 && instrAddr+4 == stumbleOntoAddr;
   const bool delaySlotInAnotherFunction =
      !delaySlotInAnotherBlockSameFunction &&
      !theParseOracle->getFn()->containsAddr(instrAddr+4);
   const bool delaySlotInSameBlock =
      !delaySlotInAnotherBlockSameFunction && !delaySlotInAnotherFunction;

   if (delaySlotInAnotherBlockSameFunction)
      // Any delay slot instruction beginning a basic block must also be the
      // only instruction of that basic block.  Necessary.
      theParseOracle->ensureBlockStartInsnIsAlsoBlockOnlyInsn(stumbleOntoAddr,
							      delaySlotInsn);
   
   if (!cfi->fields.isJmpLink) {
      const kptr_t destAddr = instrAddr + cfi->offset_nbytes;
      // cfi->offset_nbytes was undefined if cfi->fields.isJmpLink

      // Is it a call to .stret4 or .stret8, which will return for us?
      // (To do in the future: by changing this routine to an interprocedural
      // depth first search, we can automatically detect when a callee, like .stret4,
      // will return on our behalf, by examining live register stuff)
      if (std::binary_search(functionsThatReturnForTheCaller.begin(),
                             functionsThatReturnForTheCaller.end(),
                             destAddr)) {
         theParseOracle->processCallToRoutineReturningOnCallersBehalf(bb_id, 
					 callInstr, instrAddr, destAddr,
                                         delaySlotInSameBlock, delaySlotInsn);
         delete cfi;
	 return false; // bb is done; call won't return
      }

      if (fn->containsAddr(destAddr)) {
         // intra-procedural call!  Perhaps recursion; perhaps just obtaining pc.

         const bool justObtainingPC = (destAddr == instrAddr + 8);

         const bool recursion = (destAddr == fn->getEntryAddr());
         
         // If just obtaining PC, then we treat as an unconditional branch.
         // If recursion, then we treat like a conditional branch (i.e. there will
         // be both taken [recursion] and not taken [fall through; end of recursion]
         // cases).

         if (justObtainingPC) {
	    //Save the value of getNumBytes before we pass instructions
	    //to parseOracle methods which take ownership, and may destroy
	    //the objects before returning.
	    unsigned callInstrNumBytes =  callInstr->getNumBytes();
	    unsigned delayInstrNumBytes = delaySlotInsn->getNumBytes();
            theParseOracle->processCallToObtainPC(bb_id, callInstr,
                                                 instrAddr,
                                                 delaySlotInSameBlock,
                                                 delaySlotInsn);

            // Since returning true, adjust instrAddr as appropriate:
            instrAddr += callInstrNumBytes;
            if (delaySlotInSameBlock)
               instrAddr += delayInstrNumBytes;
            
            delete cfi;
	    return true; // keep parsing this basic block.
         }

         if (destAddr != fn->getEntryAddr()) {
            // Freaky: call to some place other than the start of the function.
            // unix/bcopy does this, by making a call to ".alignit", which is a
            // section of code that, indeed, does some work and then does a retl.
            // So what *really* should happen is recognizing .alignit as a
            // separate function, that just so happens to only be called from
            // bcopy.  So (alternative #1): recognize these types of calls, and
            // mark them as new functions to parse.
            // (alternative #2): try to handle it ad-hoc.  But we can't handle it
            // like a regular procedure call (no out edges to the callee, and 1
            // edge for fall-through), because we'd then never analyze the callee.
            // A possible solution is to make 1 succ edge to the callee, and 1 for
            // fall-through.  But the callee will have a retl and be wrongly
            // identified as an exit point for this function, when it's really
            // just an exit point of the "hidden" fn .alignit.
            //
            // So the best solution is to identify a true, new function (.alignit) and
            // parse it.  (alternative #1 listed above)

            theParseOracle->noteHiddenIntraProceduralFn(destAddr);
            // No "return" here; from here on, pretend it was a normal interproc call
         }
         else if (recursion) {
#if 0
//              cout << "Recursion in fn " 
//                   << addr2hex(fn->getEntryAddr())
//                   << " "
//                   << fn->getPrimaryName().c_str()
//                   << endl;

            theParseOracle->processCallRecursion(bb_id, destAddr, callInstr,
                                                instrAddr,
                                                delaySlotInSameBlock, delaySlotInsn,
                                                pathFromEntryBB);
	    delete cfi;
	    return false;
               // done with this basic block (fall-thru case was recursively parsed)
#endif
         }
         else
            assert(false && "unknown case -- intraprocedural call");
      } // intra-procedural call
   } // "true" call instruction

   // An interprocedural call:

   // Check for tail-call optimization, in any of these forms:
   // 1) call; restore
   // 2) jmpl <addr>, %o7; restore
   // 3) call; mov <reg>, %o7
   // 4) jmpl <addr>, %o7; mov <reg>, %o7
   // Note that there could be other forms, but I don't know of any others.
   // Nevertheless, we try to be clever in our analysis by saying that
   // cases (3) and (4) are fulfilled not only for "mov <reg>, %o7" but for
   // any instruction that writes to o7.

   // Do we have a tail-call-optimization sequence?

   sparc_reg regWrittenByRestore = sparc_reg::g0;
   if (((sparc_instr*)delaySlotInsn)->isRestoreInstr(regWrittenByRestore)) { // rd gets filled in if true
      const bool nonTrivialRestore = (regWrittenByRestore != sparc_reg::g0);
      unsigned num; // filled in
      const bool dangerRestore = (regWrittenByRestore.is_oReg(num) && num > 5 ||
                                  regWrittenByRestore.is_lReg(num) ||
                                  regWrittenByRestore.is_iReg(num));
         
      if (!cfi->fields.isJmpLink) {
         const kptr_t destAddr = instrAddr + cfi->offset_nbytes;

         theParseOracle->processTrueCallRestore(bb_id, callInstr,
                                               instrAddr,
                                               destAddr,
                                               nonTrivialRestore,
                                               dangerRestore,
                                               &regWrittenByRestore,
                                               delaySlotInSameBlock,
                                               delaySlotInsn);
	 delete cfi;
	 return false; // bb is finished
      }
      else {
         theParseOracle->processJmplCallRestore(bb_id, callInstr,
						instrAddr,
						nonTrivialRestore,
						dangerRestore,
						&regWrittenByRestore,
						delaySlotInSameBlock,
						delaySlotInsn);
	 delete cfi;
         return false; // bb is finished
      }
      assert(0);
   }
   else {
      // Delay slot insn isn't a restore.  Perhaps it's some other instruction
      // that writes to %o7, and thus can still be considered an optimized
      // tail call.
      sparc_instr::sparc_ru ru;
      delaySlotInsn->getRegisterUsage(&ru);
      if (*((sparc_reg_set*)ru.definitelyWritten) == sparc_reg::o7) {
         if (!cfi->fields.isJmpLink) {
            const kptr_t destAddr = instrAddr + cfi->offset_nbytes;

            theParseOracle->processTrueCallThatWritesToO7InDelay(bb_id, callInstr,
                                                                instrAddr,
                                                                destAddr,
                                                                delaySlotInSameBlock,
                                                                delaySlotInsn);
            delete cfi;
	    return false; // bb is finished
         }
         else {
            theParseOracle->processJmplThatWritesToO7InDelay(bb_id, callInstr,
                                                            instrAddr,
                                                            delaySlotInSameBlock,
                                                            delaySlotInsn);
	    delete cfi;
	    return false; // bb is finished
         }
         assert(0);
      }
      else {
         // delay slot isn't a restore, nor does it write to %o7.  So what we
         // have is an interprocedural call (either true call or jmpl to %o7) followed
         // by a "plain" delay slot instruction.
         if (!cfi->fields.isJmpLink) {
            const kptr_t destAddr = instrAddr + cfi->offset_nbytes;
            
	    //save value of numBytes.
	    unsigned callInstrNumBytes =  callInstr->getNumBytes();
	    unsigned delayInstrNumBytes = delaySlotInsn->getNumBytes();

            theParseOracle->processNonTailOptimizedTrueCall(bb_id, callInstr,
                                                           instrAddr,
                                                           destAddr,
                                                           delaySlotInSameBlock,
                                                           delaySlotInsn);

            // Since returning true, adjust instrAddr as appropriate:
            instrAddr += callInstrNumBytes;
            if (delaySlotInSameBlock)
               instrAddr += delayInstrNumBytes;
            delete cfi;
            if (bbEndsOnANormalCall) {
               // new as of 11/2000, will satisfy purists because the old way violated
               // the definition of a basic block.
	       return false; // bb shouldn't be continued
	    } else {
               // this is the old way.  Perhaps violated the definition of a basic
               // block, but still works well enough for current uses: live reg
               // analysis, identification of basic blocks for purposes of
               // code coverage, etc.  (If code enters a basic block, it'll execute
               // all of its instruction.)
               return true; // bb should be continued
	   }
         }
         else {
	    //save value of numBytes.
	    unsigned callInstrNumBytes =  callInstr->getNumBytes();
	    unsigned delayInstrNumBytes = delaySlotInsn->getNumBytes();
            
	    theParseOracle->processNonTailOptimizedJmplCall(bb_id, callInstr,
	    
                                                           instrAddr,
                                                           delaySlotInSameBlock,
                                                           delaySlotInsn);

            // Since returning true, adjust instrAddr as appropriate:
            instrAddr += callInstrNumBytes;
            if (delaySlotInSameBlock)
               instrAddr += delayInstrNumBytes;
            
	    delete cfi; 
            if (bbEndsOnANormalCall) {
               return false; // the new way (11/2000); see above
	    } else {
               return true; // bb should be continued.  The old way; see above.
	    }
         }
      }
      assert(0);
   }
   
   assert(0);
   return true; // placate compiler viz. return type
}

static bool
handleRetOrRetlInstr(bbid_t bb_id, // no basicblock & since refs are too volatile
                     instr_t *insn, kptr_t instrAddr,
                     sparc_instr::sparc_cfi *cfi,
                     kptr_t stumbleOntoAddr, // next known bb or (kptr_t)-1
                     const kmem *km,
                     parseOracle *theParseOracle) {
   // these special cases of jmpl end the basic block.  To make sure that there's
   // just one node that exits the bb, we set left to ExitBB and right to NULL.
   // And we'll return false.  But don't forget to include
   // the delay slot insn (there is one, since jmpl does not have an annul bit

   instr_t *delaySlotInsn = km->getinsn(instrAddr+4);
   assert(delaySlotInsn->getDelaySlotInfo() == instr_t::dsNone);
      // possible, but would be messy to handle
   
   // Delay slot is in this block if: (1) it isn't part of
   // another already parsed basic block, and (2) is w/in bounds of this function
   const bool delaySlotInAnotherBlockSameFunction =
      stumbleOntoAddr != (kptr_t)-1 && instrAddr+4 == stumbleOntoAddr;
   const bool delaySlotInAnotherFunction =
      !delaySlotInAnotherBlockSameFunction &&
      !theParseOracle->getFn()->containsAddr(instrAddr+4);
   const bool delaySlotInSameBlock =
      !delaySlotInAnotherBlockSameFunction && !delaySlotInAnotherFunction;

   if (!delaySlotInSameBlock)
      // Any delay slot instruction beginning a basic block must also be the
      // only instruction of that basic block.  Necessary.
      theParseOracle->ensureBlockStartInsnIsAlsoBlockOnlyInsn(instrAddr+4, 
							      delaySlotInsn);
   
   theParseOracle->processReturn(bb_id, insn, instrAddr,
				 delaySlotInSameBlock, delaySlotInsn);

   delete cfi;
   return false; // false --> bb is finished; don't fall through
}

static bool handleJmplAsJmpToFixedAddr(bbid_t bb_id,
                                       sparc_instr *instr,
                                       kptr_t jmpInsnAddr,
                                       bool delaySlotInSameBlock,
                                       sparc_instr *delaySlotInsn,
                                       const simplePathSlice_t &theSlice,
                                       sparc_reg Rjump,
                                       const simplePath &pathFromEntryBB,
                                          // doesn't yet include curr bb
                                       sparc_instr::sparc_cfi *cfi,
                                       parseOracle *theParseOracle) {
   // The link register must be thrown away; i.e., jmpl [address], %g0
   sparc_instr::sparc_ru ru;
   instr->getRegisterUsage(&ru);

   if (ru.definitelyWritten->count() > 1) // there's always PC, so it's always at least 1
      return false;

   // If a jump to a fixed address, then the slice should contain two instructions,
   // sethi followed by bset.
   // *** NOTE: That's not correct in a 64-bit environment!!!!!!!!!!!!1 ***
   // In both insns, the dest addr should be Rjump.

   // TODO: call some generic routine that returns true if a slice sets a register
   // to a constant.  Make it work for 32, 44, or 64 bit addresses.

   uint32_t the_addr = 0;

   theSlice.beginBackwardsIteration();
   if (theSlice.isEndOfIteration())
      return false; // empty slice

   instr_t *last_insn = theSlice.getCurrIterationInsn();

   theSlice.advanceIteration();
   if (theSlice.isEndOfIteration()) {
       // Just 1 insn in the slice. The following sequence occurs in
       // practice: "sethi rd; jmp rd + offs", lets check for it
       if (!((sparc_instr*)last_insn)->isSetHiToDestReg(Rjump, the_addr)) {
	   return false;
       }
       the_addr += cfi->offset_nbytes; // jmp %r + offset
   }
   else {
       instr_t *first_insn = theSlice.getCurrIterationInsn();

       theSlice.advanceIteration();
   
       if (!theSlice.isEndOfIteration())
	   return false; // more than 2 insns in slice

       // Okay, now examine "first_insn" and "last_insn" for sethi/or

       if (!((sparc_instr*)first_insn)->isSetHiToDestReg(Rjump, the_addr))
	   return false;

       sparc_reg Rtemp = sparc_reg::g0;
       if (!((sparc_instr*)last_insn)->isBSetLo(Rjump, the_addr) &&
	   !((sparc_instr*)last_insn)->isAddImmToDestReg(Rjump, Rtemp, the_addr)) {
	   return false;
       }
   }
   // Okay, we've found what we're looking for.

   if (theParseOracle->getFn()->containsAddr(the_addr)) {
      // Intra-procedural jump.  So treat it like a jump table entry, or a normal
      // unconditional branch.
      theParseOracle->processIntraproceduralJmpToFixedAddr(bb_id, instr,
                                                          jmpInsnAddr,
                                                          delaySlotInSameBlock,
                                                          delaySlotInsn,
                                                          the_addr,
                                                          theSlice,
                                                          pathFromEntryBB);
      delete cfi;
      return true; // true --> we have handled this jump
   }
   else {
      // Okay, it's interprocedural.  You can say it's an optimized tail call like
      // call/restore or jmp/restore, only from a routine without a stack frame.
      theParseOracle->processInterproceduralJmpToFixedAddr(bb_id, instr,
                                                          delaySlotInSameBlock,
                                                          delaySlotInsn,
                                                          the_addr,
                                                          pathFromEntryBB);
       delete cfi;
       return true; // true --> we have handled this jump
   }
}

// Determine whether the sequence of instructions in sliceR loads 
// the 32-bit address of a jump table into register R
// Returns the loaded address if so, 0 - otherwise 
static uint32_t isLoadJumpTableAddr32(const sparc_reg &R,
				      const simplePathSlice_t &sliceR)
{
   sliceR.beginBackwardsIteration();
   if (!sliceR.isEndOfIteration()) {
      instr_t *last_insn = sliceR.getCurrIterationInsn();
      sparc_reg srcReg = sparc_reg::g0;
      uint32_t jumpTableAddr = 0;

      if (((sparc_instr*)last_insn)->isORImmToDestReg(R, srcReg, jumpTableAddr) ||
          ((sparc_instr*)last_insn)->isAddImmToDestReg(R, srcReg, jumpTableAddr)) {
	 sliceR.advanceIteration();
	 if (!sliceR.isEndOfIteration()) {
	    instr_t *second_to_last_insn = sliceR.getCurrIterationInsn();
	    if (((sparc_instr*)second_to_last_insn)->isSetHiToDestReg(srcReg, jumpTableAddr)) {
               return jumpTableAddr;
            }
	 }
      }
   }
   
   return 0; // Can't recognize the sequence
}

// Test whether the given slice ends with the sethi/bset combination
// producing the value in the register R. The function looks at the 
// slice starting from the current position of the iterator
// Returns true if the sequence matches, generates the number in "value"
static bool isSethiBsetToDestReg(const sparc_reg &R,
				 const simplePathSlice_t &iter,
				 uint32_t &value)
{
    value = 0;
    sparc_reg srcReg = sparc_reg::g0;

    if (iter.isEndOfIteration()) {
	return false;
    }
    instr_t *current = iter.getCurrIterationInsn();
    // If current is an OR, fill lower 10 bits of jumpTableAddr
    // Set srcReg1 to the source register
    if (!((sparc_instr*)current)->isORImmToDestReg(R, srcReg, value) || srcReg != R) {
	return false;
    }

    iter.advanceIteration();
    if (iter.isEndOfIteration()) {
	return false;
    }
    instr_t *current2 = iter.getCurrIterationInsn();
    // If current is a SETHI, fill middle 22 bits of jumpTableAddr 
    if (!((sparc_instr*)current2)->isSetHiToDestReg(R, value)) {
	return false;
    }
    return true;
}

// Test whether two given slices emit 32-bit values into the corresponding
// registers and combine them into a 64-bit value
// Return the generated value
static uint64_t isGenAddr64(const sparc_reg &highReg, 
			    const sparc_reg &lowReg,
			    const simplePathSlice_t &sliceHigh, 
			    const simplePathSlice_t &sliceLow)
{
    sparc_reg srcReg = sparc_reg::g0;
    uint32_t loword, hiword, numBits;
    
    if (sliceHigh.isEndOfIteration()) {
	return 0;
    }
    instr_t *current = sliceHigh.getCurrIterationInsn();
    if (!((sparc_instr*)current)->isShiftLeftLogicalToDestReg(highReg, srcReg, numBits) ||
	srcReg != highReg || numBits != 32) {
	return 0;
    }
    sliceHigh.advanceIteration();
    // The following uses the current position of the iterator
    if (!isSethiBsetToDestReg(highReg, sliceHigh, hiword)) {
	return 0;
    }

    // The following uses the current position of the iterator
    if (!isSethiBsetToDestReg(lowReg, sliceLow, loword)) {
	return 0;
    }
    return (((uint64_t)hiword << 32) | (uint64_t)loword);
}

static bool is64BitKernel()
{
    return (sizeof(kptr_t) == sizeof(uint64_t));
}

// Determine whether the sequence of instructions in sliceR loads 
// the 64-bit address of a jump table into register R
// Starts looking at the code from the current position of the slice iter
// Returns the loaded address if so, 0 - otherwise 
static uint64_t isLoadJumpTableAddr64(const sparc_reg &R,
				      const simplePathSlice_t &sliceR)
{
    sparc_reg lowReg = sparc_reg::g0, highReg = sparc_reg::g0;
    uint64_t addr;

    // This code sequence occurs in the 64-bit kernel only
    if (!is64BitKernel()) {
	return 0;
    }

    sliceR.beginBackwardsIteration();
    if (sliceR.isEndOfIteration()) {
	return 0;
    }
    instr_t *current = sliceR.getCurrIterationInsn();

    // fill lowReg and  highReg
    if (!((sparc_instr*)current)->isOR2RegToDestReg(R, lowReg, highReg) ||
	(lowReg != R && highReg != R)) {
	return 0;
    }

    sparc_reg_set sliceHighReg(highReg);
    simplePathSlice_t sliceHigh(sliceR, 1, false, sliceHighReg);
    sliceHigh.beginBackwardsIteration();

    sparc_reg_set sliceLowReg(lowReg);
    simplePathSlice_t sliceLow(sliceR, 1, false, sliceLowReg);
    sliceLow.beginBackwardsIteration();

    if ((addr = isGenAddr64(highReg, lowReg, sliceHigh, sliceLow)) == 0) {
	// Try transposed
	addr = isGenAddr64(lowReg, highReg, sliceLow, sliceHigh);
    }
    return addr;
}

// Determines which of the registers in "jmp R1 + R2" actually contains
// the jump table address. The address is returned in "jumpTableAddr"
static void
findWhichRegLoadsJumpTableAddr(const sparc_reg &R1,
			       const sparc_reg &R2,
			       const simplePathSlice_t &sliceR1,
			       const simplePathSlice_t &sliceR2,
			       bool &sliceR1LoadsJumpTableAddr,
			       bool &sliceR2LoadsJumpTableAddr,
			       kptr_t &jumpTableAddr) {
   sliceR1LoadsJumpTableAddr = sliceR2LoadsJumpTableAddr = false;
   
   if ((jumpTableAddr = isLoadJumpTableAddr32(R1, sliceR1)) != 0) {
       sliceR1LoadsJumpTableAddr = true;
   } 
   else if ((jumpTableAddr = isLoadJumpTableAddr32(R2, sliceR2)) != 0) {
       sliceR2LoadsJumpTableAddr = true;
   }
   else if ((jumpTableAddr = isLoadJumpTableAddr64(R1, sliceR1)) != 0) {
       sliceR1LoadsJumpTableAddr = true;
   } 
   else if ((jumpTableAddr = isLoadJumpTableAddr64(R2, sliceR2)) != 0) {
       sliceR2LoadsJumpTableAddr = true;
   }
}

static bool handleJmplAsSimpleJumpTable32(bbid_t bb_id,
                                          sparc_instr *instr,
                                          bool delaySlotInThisBlock,
                                          sparc_instr *delaySlotInsn,
                                          kptr_t jumpInsnAddr,
                                          const simplePathSlice_t &theSlice,
                                          sparc_reg Rjump,
                                          const simplePath &pathFromEntryBB,
                                             // doesn't yet include curr bb
                                          const kmem *km,
                                          parseOracle *theParseOracle) {
   typedef function_t::bbid_t bbid_t;

   // "Simple" jump table:
   // The jump table data contains 32-bit absolute addresses.
   // Code will ld one of these entries and jmp there.  The load address is the
   // 2-register kind; one reg holds the start of the jump table data (sethi & bset/add)
   // and the other reg is the load offset (the index)
   // Once we find the start of the jump table contents, we read it to determine
   // various jump locations.
   // Note that the various jump locations may be before or after the jump table
   // itself.
   // The toughest thing about this kind of jump table is knowing when it ends;
   // I know of no oracle for telling us this.

//    cout << "Welcome to handleJmplAsSimpleJumpTable: slice of the jump stmt is:" << endl;
//    cout << theSlice << endl;

   theSlice.beginBackwardsIteration();
   if (theSlice.isEndOfIteration())
      return false; // bw slice of jmp stmt empty

   // If a simple jump table, the last insn in the slice should be ld [R1 + R2], Rjump.
   // Check for that now.
   instr_t *lastSliceInstr = theSlice.getCurrIterationInsn();
   if (!lastSliceInstr->isLoad())
      return false; // last insn in slice isn't a load

   sparc_instr::sparc_ru ru;
   lastSliceInstr->getRegisterUsage(&ru);

   // Check for a write to Rjump:
   if (ru.definitelyWritten->count() == 0 ||
       ((sparc_reg_set*)ru.definitelyWritten)->expand1() != Rjump)
      return false; // we expected the last insn in slice to load to Rjump

   // Now get R1 and R2, the regs used in the load
   // (Note: we could optimize away the vector by using sparc_reg_set iterators)
   *((sparc_reg_set*)ru.definitelyUsedBeforeWritten) -= sparc_reg::g0;

   if (ru.definitelyUsedBeforeWritten->count() != 2)
      return false; // last insn in slice loads to Rjump but doesn't use 2 regs

   sparc_reg_set::const_iterator regiter = ru.definitelyUsedBeforeWritten->begin();
   
   // Okay, now we've got the R1 and R2 in the ld [R1 + R2], Rjump.
   // Time to take two more backwards slices...this time, on R1 and R2 (not Rjump).
   // We'll start with false for control dependencies, to ensure that we find
   // the one which loads the jump table address.  For the other one, we'll redo
   // the slice with control dependencies on.
   const sparc_reg R1 = (sparc_reg&) *regiter++;
   sparc_reg_set regsToSliceR1(R1);
   simplePathSlice_t sliceR1(theSlice, 1, false, regsToSliceR1);

   const sparc_reg R2 = (sparc_reg&) *regiter++;
   sparc_reg_set regsToSliceR2(R2);
   simplePathSlice_t sliceR2(theSlice, 1, false, regsToSliceR2);

   assert(regiter == ru.definitelyUsedBeforeWritten->end());

//    cout << "sliceR1 printout (no ctl dep)" << endl;
//    cout << sliceR1 << endl;

//    cout << "sliceR2 printout (no ctl dep)" << endl;
//    cout << sliceR2 << endl;

   // One of the two slices (or both) should be complete (regsToSlice now empty).
   if (!regsToSliceR1.isempty() && !regsToSliceR2.isempty())
      return false; // at least one slice should be completed

   bool sliceR1LoadsJumpTableAddr=false;
   bool sliceR2LoadsJumpTableAddr=false;
   kptr_t jumpTableStartAddr = 0;
   findWhichRegLoadsJumpTableAddr(R1, R2, sliceR1, sliceR2,
				  sliceR1LoadsJumpTableAddr,
				  sliceR2LoadsJumpTableAddr,
				  jumpTableStartAddr);
   
   if (!sliceR1LoadsJumpTableAddr && !sliceR2LoadsJumpTableAddr)
      return false; // neither slice loads jmp table addr

   if (sliceR1LoadsJumpTableAddr && sliceR2LoadsJumpTableAddr)
      return false; // both slices load jmp table addr (!)

   const sparc_reg byteOffsetReg = sliceR1LoadsJumpTableAddr ? R2 : R1;

   // We comment out the following check; jump table data no longer needs to be
   // contained within the function's code.  Consider afs/SRXAFSCB_GetXState() on
   // solaris7/32 bit, where the jump table data is immediately *after* the function.
//     // During parsing, chunks are if anything too large (will be trimmed when done
//     // parsing), so the following check isn't too restrictive:
//     if (!theParseOracle->getFn()->containsAddr(jumpTableStartAddr)) {
//        cout << "At insnAddr " << addr2hex(jumpInsnAddr) << endl;
//        cout << "[SimpleJumpTable32] jmp table start addr "
//             << addr2hex(jumpTableStartAddr)
//             << " is not contained within its parent fn, as had been expected;"
//             << "throwing exception" << endl;
//        throw function::parsefailed();
//     }

   // We used to assert that the jump table data began immediately after the jmp/delay
   // combination, but that hasn't been safe to assert for a long time, so we got
   // rid of that check:
   // assert(jumpTableStartAddr == jumpInsnAddr + 8); (_IO_vfscanf doesn't obey this)

   // One slice (let's call it the jump table slice) just loads the addr of the jmp tab.
   // The other slice (call it the byte-offset slice) loads the bytes offset.
   simplePathSlice_t &bytesOffsetSlice = sliceR1LoadsJumpTableAddr ? sliceR2 : sliceR1;
   simplePathSlice_t &setJumpTableStartAddrSlice = sliceR1LoadsJumpTableAddr ? sliceR1 : sliceR2;

   // The last insn in the byte-offset slice should be "sll Rindex, 2, Rbyteoffset".
   bytesOffsetSlice.beginBackwardsIteration();
   if (bytesOffsetSlice.isEndOfIteration())
      return false;

   instr_t *last_insn_index_slice = bytesOffsetSlice.getCurrIterationInsn();
   sparc_reg indexReg = sparc_reg::g0; // will get filled in
   unsigned  shiftamt = 0; // will get filled in
   if (!((sparc_instr*)last_insn_index_slice)->isShiftLeftLogicalToDestReg(byteOffsetReg,
							   indexReg, // filled in
							   shiftamt // filled in
							   )) {
      if (verbose_fnParse)
         cout << "sorry, index slice doesn't end in a sll statement" << endl;
      return false;
   }
   if (shiftamt != 2) {
      if (verbose_fnParse)
         cout << "sorry, index slice ends in an sll stmt but the shiftamt isn't 2"
              << endl;
      return false;
   }

   // Now we need indexSlice, a sub-slice of byte-offset slice, but with ctl
   // dependencies ON.  Thus, we first need to re-take the byte-offset slice.
   sparc_reg_set byteOffsetSliceRegs(byteOffsetReg);
   simplePathSlice_t byteOffsetSliceCtlDep(theSlice, 1, true, byteOffsetSliceRegs);

//    cout << "(Zeroth revision) -- byte offset slice (w/ ctl dependencies):" << endl;
//    cout << byteOffsetSliceCtlDep << endl;
   byteOffsetSliceCtlDep.beginBackwardsIteration();
   if (byteOffsetSliceCtlDep.isEndOfIteration())
      assert(false);
   instr_t *last_insn = byteOffsetSliceCtlDep.getCurrIterationInsn();
   sparc_reg indexRegCtlDep = sparc_reg::g0; // filled in
   unsigned shiftamtCtlDep = 0; // filled in
   if (!((sparc_instr*)last_insn)->isShiftLeftLogicalToDestReg(byteOffsetReg,
					       indexRegCtlDep, shiftamtCtlDep))
      assert(false);
   assert(shiftamt == 2);
   // The register used in the sll statement (indexRegCtlDep) is the index register
   // we are looking for.
   assert(indexRegCtlDep == indexReg);
   // Now take a sub-slice of byte-offset slice, to isolate indexRegCtlDep
   sparc_reg_set indexSliceRegs(indexRegCtlDep);
   simplePathSlice_t indexRegSlice(byteOffsetSliceCtlDep, 1, true, indexSliceRegs);

//    cout << "(First revision) -- indexRegSlice (w/ ctl dependencies) is:" << endl;
//    cout << indexRegSlice << endl;

   theParseOracle->prepareSimpleJumpTable32(bb_id, instr, jumpInsnAddr,
					    delaySlotInThisBlock, delaySlotInsn,
					    jumpTableStartAddr,
					    setJumpTableStartAddrSlice);

   // How to handle a jump table: the tricky part is that we don't know where
   // the jump table ends.  So we process the first jump table entry
   // (as the first successor to this bb) recursively.  Then, in general, if
   // the next would-be jump table entry is the start of some basic block, then
   // we stop; otherwise we recursively process it as the 2d successor to this bb.
   // It all continues...until the next would-be jump table entry falls on top of
   // an existing basic block or the next known fn.

   bool allJumpsForwardSoFar = true;
   bool atLeastOneJumpForward = false;

   if (verbose_fnParse)
      cout << "Jump table (simple-32) begins at addr "
           << addr2hex(jumpTableStartAddr) << endl;

   const function_t *fn = theParseOracle->getFn();

   unsigned jumpTableNdx = 0;
   while (true) {
      const kptr_t nextJumpTableEntryAddr = jumpTableStartAddr + 4*jumpTableNdx;

      // See above; we no longer assert that jump table data entries must lie within
      // the body of a function (afs/SRXAFSCB_GetXStats() certainly doesn't obey
      // this for solaris7/32-bit, for example)
//        if (!fn->containsAddr(nextJumpTableEntryAddr))
//  	 // Stumbled onto next function!
//  	 break;

      bbid_t stumble_bb_id = fn->searchForBB(nextJumpTableEntryAddr);
      if (stumble_bb_id != (bbid_t)-1) {
	 // Stumbled onto existing bb of this fn.
	 assert(fn->getBasicBlockFromId(stumble_bb_id)->getStartAddr() ==
                nextJumpTableEntryAddr);
	 break;
      }

      // Now dereference "nextJumpTableEntryAddr", obtaining the actual data for one
      // jump table entry:
      instr_t *nextEntry = km->getinsn(nextJumpTableEntryAddr);
      kptr_t nextJumpTableEntry = nextEntry->getRaw();
      delete nextEntry;

      // If all jump tab entry jumps backwards, then it's quite likely that we won't
      // be able to find the end of the jump table by stumbling onto an existing bb.
      // When this occurs, we detect the end of jmp table by a jmp tab entry that
      // appears to be garbage.

      // Is the jump table entry garbage (bad address)? 
      // Reminder: during parsing, chunks are if anything too large (they'll get
      // trimmed when done parsing), so if the following test is false, then the
      // jump table entry is certainly garbage.
      if (!fn->containsAddr(nextJumpTableEntry)) {
	 // If all jumps have been forward so far, then this shouldn't happen
	 if (allJumpsForwardSoFar) {
	    // All jmps fwd, so we would have hoped to stumble onto
	    // an existing bb before having this happen.  In any event, time to end
	    // the jmp table.
            if (verbose_fnParse) {
               cout << "Ended jmp table (ALL jmps fwd) when garbage entry found."
                    << endl << "Please verify.  fnStartAddr="
                    << addr2hex(fn->getEntryAddr())
                    << " jump table start addr=" << addr2hex(jumpTableStartAddr)
                    << endl;
               cout << "Bad entry @" << addr2hex(nextJumpTableEntryAddr)
                    << " containing the bad address of " << addr2hex(nextJumpTableEntry)
                    << endl;
            }
	    
	    break;
	    assert(false);
	 }
	 else if (atLeastOneJumpForward) {
	    // At least 1 jmp has been fwd, so we would have hoped to stumble onto
	    // an existing bb before having this happen.  In any event, time to end
	    // the jmp table.
            if (verbose_fnParse) {
               cout << "Ended jmp table (>=1 jmp fwd) when garbage entry found."
                    << endl << "Please verify.  startAddr="
                    << addr2hex(fn->getEntryAddr())
                    << " jump table start addr=" << addr2hex(jumpTableStartAddr) << endl;
               cout << "Bad entry @" << addr2hex(nextJumpTableEntryAddr)
                    << " containing the bad address of " << addr2hex(nextJumpTableEntry)
                    << endl;
            }
	    
	    break;
	    assert(false); // too strict
	 }
	 else {
	    // Okay, all jumps have been backwards.  That explains why we haven't
	    // stumbled onto an existing bb.  Time to end the jmp table.
            if (verbose_fnParse) {
               cout << "Ended jmp table (all jumps backwards) when garbage entry found."
                    << endl << "Please verify.  startAddr=" << addr2hex(fn->getEntryAddr())
                    << " jump table start addr=" << addr2hex(jumpTableStartAddr) << endl;
               cout << "Bad entry @" << addr2hex(nextJumpTableEntryAddr)
                    << " containing the bad address of " << addr2hex(nextJumpTableEntry)
                    << endl;
            }

	    break;
	 }
      }
      
      assert(nextJumpTableEntry % 4 == 0);
      assert(fn->containsAddr(nextJumpTableEntry));

      if (nextJumpTableEntry < (jumpInsnAddr + 8))
	 allJumpsForwardSoFar = false;
      else
	 atLeastOneJumpForward = true;

      theParseOracle->process1SimpleJumpTable32Entry(bb_id, // uniquely id's 
						           // the jmp table
                                                    nextJumpTableEntry,
                                                    pathFromEntryBB);
      
      jumpTableNdx++;
   } // while (true)

//     cout << fn->getPrimaryName().c_str()
//          << "SIMPLE-32; insnAddr=" << addr2hex(jumpInsnAddr)
//          << "; jmpTabData starts at " << addr2hex(jumpTableStartAddr) << endl;
   
   // Done processing the jump table
   theParseOracle->completeSimpleJumpTable32(bb_id, jumpTableStartAddr);
   
   return true; // true --> we successfully found a jump table
}

static bool handleJmplAsTaggedJumpTable(bbid_t bb_id,
                                        sparc_instr *instr,
                                        bool delaySlotInThisBlock,
                                        sparc_instr *delaySlotInsn,
                                        kptr_t jumpInsnAddr,
                                        const simplePathSlice_t &theSlice,
                                        sparc_reg Rjump,
                                        const simplePath &pathFromEntryBB,
                                           // doesn't yet include curr bb
                                        const kmem *km,
                                        parseOracle *theParseOracle) {
   typedef function_t::bbid_t bbid_t;
   
   // Tagged jump table:
   // tag1
   // address1
   // tag2
   // address2
   // tag3
   // address3
   // ...
   // ...
   //
   // The code looks like this:
   // ld [R1 + 0x4], Rjump
   // jmp Rjump
   //
   // We then take a backwards slice of R1.  It ends in
   // add T1, T2, R1
   //
   // We then take backwards slices of T1 and T2
   // One of those slices just sets the (32-bit) address of the jump table
   // 
   // As with simple jump tables, we stop when we see a bad address or stumble
   // onto an existing bb.  All addresses must fall within function bounds.

//    cout << "handleJmplAsTaggedJumpTable: welcome...theSlice is" << endl;
//    theSlice.print(cout, fnStartAddr, orig_insns);
  
   // Slice should end in:  ld [R1 + 0x4], %Rjump
   theSlice.beginBackwardsIteration();
   if (theSlice.isEndOfIteration())
      return false;
   instr_t *lastSliceInstr = theSlice.getCurrIterationInsn();
   sparc_reg rs1 = sparc_reg::g0; // filled in
   int offset = 0; // filled in
   if (!((sparc_instr*)lastSliceInstr)->isLoadRegOffsetToDestReg(Rjump, rs1, offset))
      return false;

   if (offset != 0x4)
      return false;
   
   // Sub-slice on "rs1"
   sparc_reg_set regsToSlice(rs1);
   simplePathSlice_t rs1Slice(theSlice, 1, true, regsToSlice);

//    cout << "complex jump table: working with this slice (should end in add)" << endl;
//    cout << rs1Slice << endl;

   // Slices on the 2 src regs used in the add statement
   rs1Slice.beginBackwardsIteration();
   if (rs1Slice.isEndOfIteration())
      return false;
   lastSliceInstr = rs1Slice.getCurrIterationInsn();
   sparc_reg addReg1 = sparc_reg::g0; // filled in
   sparc_reg addReg2 = sparc_reg::g0; // filled in
   if (!((sparc_instr*)lastSliceInstr)->isAdd2RegToDestReg(rs1, addReg1, addReg2))
      return false;
   
   sparc_reg_set regsToSlice1 = addReg1;
   simplePathSlice_t addReg1Slice(rs1Slice, 1, false, regsToSlice1);
//    cout << "add reg 1 slice: " << endl;
//    cout << addReg1Slice << endl;
   
   sparc_reg_set regsToSlice2 = addReg2;
   simplePathSlice_t addReg2Slice(rs1Slice, 1, false, regsToSlice2);
//    cout << "add reg 2 slice: " << endl;
//    cout << addReg2Slice << endl;
   
   // One of these two slices should be complete (regsToSlice empty) and
   // contain just two instructions: sethi; bset; (load addr of jmp table)
   
   if (!regsToSlice1.isempty() && !regsToSlice2.isempty())
      return false;
   
   bool slice1LoadsJumpTableAddr=false;
   bool slice2LoadsJumpTableAddr=false;
   kptr_t jumpTableStartAddr = 0;
   findWhichRegLoadsJumpTableAddr(addReg1, addReg2, addReg1Slice, addReg2Slice,
				  slice1LoadsJumpTableAddr,
				  slice2LoadsJumpTableAddr,
				  jumpTableStartAddr);

   if (!slice1LoadsJumpTableAddr && !slice2LoadsJumpTableAddr)
      return false; // neither
   if (slice1LoadsJumpTableAddr && slice2LoadsJumpTableAddr)
      return false; // both?!

   theParseOracle->prepareTaggedJumpTable32(bb_id, jumpTableStartAddr,
                                           instr,
                                           delaySlotInThisBlock, delaySlotInsn);
   
   // We assume that the jump table is organized like this:
   // tag, address, tag, address, ...

   bool allJumpsForwardSoFar = true;
   bool atLeastOneJumpForward = false;

   const function_t *fn = theParseOracle->getFn();

   unsigned jumpTableNdx = 0;
   while (true) {
      kptr_t nextTagAddr = jumpTableStartAddr + 8*jumpTableNdx;
      kptr_t nextJumpTableEntryAddr = nextTagAddr + 4;
      if (!fn->containsAddr(nextTagAddr))
         // probably stumbled onto the next function
         break;
      
      bbid_t stumble_bb_id = fn->searchForBB(nextTagAddr);
      if (stumble_bb_id != (bbid_t)-1) {
         // stumbled onto existing bb of this fn.
         const basicblock_t *stumble_bb = fn->getBasicBlockFromId(stumble_bb_id);
         assert(stumble_bb->getStartAddr() == nextTagAddr);
         break;
      }
      else
         assert((bbid_t)-1 == fn->searchForBB(nextJumpTableEntryAddr));
      
      instr_t *nextT =  km->getinsn(nextTagAddr);
      const kptr_t nextTag = nextT->getRaw();
      delete nextT;

      if (nextTag == UINT_MAX)
	 break;
      
      instr_t *nextJ =  km->getinsn(nextJumpTableEntryAddr);
      kptr_t nextJumpTableEntry = nextJ->getRaw();
      delete nextJ;
      
      // Is the jump table entry garbage (bad address)?
      if (!fn->containsAddr(nextJumpTableEntry)) {
	 // If all jumps have been forward so far, then this shouldn't happen
	 if (allJumpsForwardSoFar) {
	    // All jmps fwd, so we would have hoped to stumble onto
	    // an existing bb before having this happen.  In any event, time to end
	    // the jmp table.
            if (verbose_fnParse) {
               cout << "Ended jmp table (ALL jmps fwd) when garbage entry found."
                    << endl << "Please verify.  startAddr=" << addr2hex(fn->getEntryAddr())
                    << " jump table start addr=" << addr2hex(jumpTableStartAddr) << endl;
               cout << "Bad entry @" << addr2hex(nextJumpTableEntryAddr)
                    << " containing the bad address of " << addr2hex(nextJumpTableEntry)
                    << endl;
            }
	    
	    break;
	    assert(false);
	 }
	 
	 else if (atLeastOneJumpForward) {
	    // At least 1 jmp has been fwd, so we would have hoped to stumble onto
	    // an existing bb before having this happen.  In any event, time to end
	    // the jmp table.
            
            if (verbose_fnParse) {
               cout << "Ended jmp table (>=1 jmp fwd) when garbage entry found."
                    << endl << "Please verify.  startAddr=" << addr2hex(fn->getEntryAddr())
                    << " jump table start addr=" << addr2hex(jumpTableStartAddr) << endl;
               cout << "Bad entry @" << addr2hex(nextJumpTableEntryAddr)
                    << " containing the bad address of " << addr2hex(nextJumpTableEntry)
                    << endl;
            }
	    
	    break;
	    assert(false); // too strict
	 }
	 else {
	    // Okay, all jumps have been backwards.  That explains why we haven't
	    // stumbled onto an existing bb.  Time to end the jmp table.
            if (verbose_fnParse) {
               cout << "Ended jmp table (all jumps backwards) when garbage entry found."
                    << endl << "Please verify.  startAddr=" << addr2hex(fn->getEntryAddr())
                    << " jump table start addr=" << addr2hex(jumpTableStartAddr) << endl;
               cout << "Bad entry @" << addr2hex(nextJumpTableEntryAddr)
                    << " containing the bad address of " << addr2hex(nextJumpTableEntry)
                    << endl;
            }
	    
	    break;
	 }
      }
      
      assert(nextJumpTableEntry % 4 == 0);
      assert(fn->containsAddr(nextJumpTableEntry));

      // If all jump tab entry jumps backwards, then it's quite likely that we won't
      // be able to find the end of the jump table by stumbling onto an existing bb.
      // When this occurs, we detect the end of jmp table by a jmp tab entry that
      // appears to be garbage.
      if (nextJumpTableEntry < (jumpInsnAddr + 8))
	 allJumpsForwardSoFar = false;
      else
	 atLeastOneJumpForward = true;

      theParseOracle->process1TaggedJumpTable32Entry(bb_id,
                                                    nextTag, nextJumpTableEntry,
                                                    pathFromEntryBB);

      jumpTableNdx++;
   }

   theParseOracle->completeTaggedJumpTable32(bb_id, jumpTableStartAddr);
   
   return true; // true --> we have successfully found a jmp table
}

static bool
handleJmplAsNonLoadingBackwardsJumpTable(bbid_t bb_id,
                                         sparc_instr *instr,
                                         bool delaySlotInThisBlock,
                                         sparc_instr *delaySlotInsn,
                                         kptr_t /* insnAddr */,
                                         const simplePathSlice_t &theSlice,
                                         sparc_reg Rjump,
                                         const simplePath &pathFromEntryBB,
                                         parseOracle *theParseOracle)
{
   // In sparc solaris 2.6/2.7, two functions that this should handle are blkclr (has
   //    two occurrences) & hwblkclr
   // Returns true iff we have handled the jump table

   // Steps to take:
   // 1) last insn in slice should be "sub r1, r2, Rjump"
   // 2) get backwards slice on r1: it should be a sethi and bset/add of a constant
   //    (the address just **after** the **end** of jump table), which is the 
   //    start of some basic block *within* the function (not "above" it, as per usual
   //    with jump table entries)
   // 3) backwards slice on r2: it should end in srl r3, shift-const, r2
   //    Remember the const.
   // 4) backwards slice on r3: it should end in andn r4, andn-const, r3
   //    andn-const should be right justified 1's (2^x - 1, for some integer x)
   // 5) popc of (andn-const) minus shift-const should equal 2
   // 6) All we need now is the upper bounds.  Get backwards slice on r4.
   //    Expect: cmp r4, (andn-const)+1; blt <elsewhere>;
   //            cmp r4, (max); bge[,a] <elsewhere>
   // 7) max--; max &= ~(andn-const); max >>= shift right amount; max >>= 2
   //    Now, max is the *exact* number of jump table entries!

   // Step 1: Last insn in slice should be "sub r1, r2, Rjump"
   theSlice.beginBackwardsIteration();
   if (theSlice.isEndOfIteration())
      return false;

   instr_t *lastSliceInstr = theSlice.getCurrIterationInsn();

   sparc_reg r1(sparc_reg::g0); // filled in
   sparc_reg r2(sparc_reg::g0); // filled in
   if (!((sparc_instr*)lastSliceInstr)->isSub2RegToDestReg(Rjump, r1, r2))
      return false;

   // Step 2: Get backwards slice on r1: it should be a sethi; bset/add
   sparc_reg_set regsToSlice(r1);
   simplePathSlice_t rs1Slice(theSlice, 1, true, regsToSlice);
   rs1Slice.beginBackwardsIteration();
   if (rs1Slice.isEndOfIteration())
      return false;
   lastSliceInstr = rs1Slice.getCurrIterationInsn();

   uint32_t endOfJumpTable = 0;
      // TODO: make this 64-bit safe
   sparc_reg srcReg = sparc_reg::g0;
   
   if (!((sparc_instr*)lastSliceInstr)->isORImmToDestReg(r1, srcReg, endOfJumpTable) &&
       !((sparc_instr*)lastSliceInstr)->isAddImmToDestReg(r1, srcReg, endOfJumpTable))
      return false;
   
   rs1Slice.advanceIteration();
   if (rs1Slice.isEndOfIteration())
      return false;
   instr_t *secToLastInsn = rs1Slice.getCurrIterationInsn();
   if (!((sparc_instr*)secToLastInsn)->isSetHiToDestReg(srcReg, endOfJumpTable))
      return false;

   // Step 3: Get backwards slice on r2: it should end in srl r3, shift-const, r2
   sparc_reg_set r2_slice_regs(r2);
   simplePathSlice_t r2Slice(theSlice, 1, true, r2_slice_regs);
   r2Slice.beginBackwardsIteration();
   if (r2Slice.isEndOfIteration())
      return false;
   lastSliceInstr = r2Slice.getCurrIterationInsn();

   sparc_reg r3(sparc_reg::g0);
   unsigned shift_amt = UINT_MAX;
   if (!((sparc_instr*)lastSliceInstr)->isShiftRightLogicalToDestReg(r2, r3, shift_amt))
      return false;

   // Step 4: Backwards slice on R3: should end in andn r4, andn-const, r3
   sparc_reg_set r3_slice_regs(r3);
   simplePathSlice_t r3Slice(r2Slice, 1, true, r3_slice_regs);
   r3Slice.beginBackwardsIteration();
   if (r3Slice.isEndOfIteration())
      return false;
   lastSliceInstr = r3Slice.getCurrIterationInsn();

   sparc_reg r4(sparc_reg::g0);
   int andn_const = UINT_MAX;
   if (!((sparc_instr*)lastSliceInstr)->isAndnImmToDestReg(r3, r4, andn_const))
      return false;

   // andn-const should be 2^x-1 for some integer x.  To check this quicky: do a
   // popc, and compare (2^popc) - 1 to andn-const; they should be equal
   if ((1 << ari_popc(andn_const)) - 1 != andn_const)
      return false;
   
   // Step 5: popc(andn_const) minus shift_amt should equal 2
   if (ari_popc(andn_const) - shift_amt != 2)
      return false;

   // Step 6: backwards slice on %pc, looking for a conditional branch, preceded
   // by a compare instruction.

   unsigned min_val=UINT_MAX;
   unsigned max_val=0;

   sparc_reg_set pc_slice_regs(sparc_reg::reg_pc);
   simplePathSlice_t pcSlice(r3Slice, 1, true, pc_slice_regs);
   if (!analyzeSliceForRegisterBoundsViaBranches(pcSlice, r4, min_val, max_val))
      return false;

   // Step 7: The home stretch.  Play around with 'max_val' to obtain the number
   // of jump table entries.
   max_val &= ~andn_const; // these bits get cleared
   max_val >>= shift_amt; // and this is the shift amount to get byte offset
   max_val >>= 2; // from byte offset to insnNum offset.  The number of insns in the
                  // jump table

   // Convert min_val to a number of jump table entries
   min_val &= ~andn_const;
   min_val >>= shift_amt; // now we have byte offset
   min_val >>= 2; // from byte offset to insnNum offset.
   if (min_val != 1)
      return false;

//   if (verbose_fnParse)
//      cout << "Now processing non-loading-backwards-jumptable..." << endl;
//   const bbid_t justPastEndOfJumpTableBBId =
//      theParseOracle->getFn()->searchForBB(endOfJumpTable);
//   assert(theParseOracle->getFn()->getBasicBlockFromId(justPastEndOfJumpTableBBId).getStartAddr() == endOfJumpTable && "endOfJumpTable must be at the beginning of some basic block");

   theParseOracle->processNonLoadingBackwardsJumpTable
      (bb_id,
       endOfJumpTable-4*max_val, // start of jump table
       endOfJumpTable, // just *past* the end of the jump table, right?
       instr, delaySlotInThisBlock, delaySlotInsn,
       rs1Slice,
       pathFromEntryBB);
   
   return true; // we have successfully parsed a jump table of this type
}

static bool
handleJmpAsRetlUsingASavedLinkRegister(bbid_t bb_id,
                                       sparc_instr *instr,
                                       bool delaySlotInSameBlock,
                                       sparc_instr *dsInsn,
                                       kptr_t insnAddr,
                                       sparc_instr::controlFlowInfo *cfi,
                                       const simplePathSlice_t &theSlice,
                                       sparc_reg Rjump,
                                       parseOracle *theParseOracle)
{
   // Recognizes this kind of cleverly-optimized function:
   // mov %o7, <reg>   where <reg> won't be clobbered by any function calls
   //                  (so it must be a %i or %l reg)
   // ...
   // body of function, making procedure call(s), which thus trash %o7
   //    (good thing we saved %o7 earlier)
   // ...
   // jmp <reg>+8   // we should treat this as a retl!
   // <delay>

   // How to detect this case: backwards slice on Rjump will be a single
   // instruction: mov %o7, Rjump

   assert(cfi->fields.isJmpLink);
   assert(*((sparc_reg*)cfi->destreg1) == Rjump);
   if (cfi->destination != instr_t::controlFlowInfo::regRelative ||
       cfi->offset_nbytes != 8)
      // it's not of the form "jmpl [somereg+8], <linkreg>" so forget about it
      return false;

   if (!sparc_reg_set::allLs.exists(Rjump) && !sparc_reg_set::allIs.exists(Rjump))
      // it must (well that may be too strict a claim) be an %i or %l register
      // to even have the possibility of being non-volatile across calls.
      return false;
   
   // OK, we've got jmp Rjump + 8.  Time to check the slice

//     cout << "handleJmplAsRetlUsingASavedLinkRegister: here is the slice" << endl;
//     theSlice.printx(cout);

   theSlice.beginBackwardsIteration();
   if (theSlice.isEndOfIteration())
      return false;

   instr_t *lastSliceInstr = theSlice.getCurrIterationInsn();
   sparc_reg source_reg_of_move = sparc_reg::g0;
   if (!((sparc_instr*)lastSliceInstr)->isMove(Rjump, // must match
					       source_reg_of_move // filled in
					       ))
      return false;
   
   if (source_reg_of_move != sparc_reg::o7)
      // oh, never mind, since we're jumping to something other than a saved o7.
      return false;

   // it's jmp <reg> + 8 where <reg> was set as "mov %o7, <reg>".  That's good
   // enough for me.  Match!

   theParseOracle->processJmpBehavingAsRet(bb_id, instr, insnAddr,
                                           delaySlotInSameBlock,
                                           dsInsn);
   delete cfi; 
   return true; // successfully parsed
}

// Most calls through a function pointer get translated into indirect jumps
// Our goal is to distinguish these cases from the jump tables. In the
// following special case we seem to be able to do this. Consider the sequence:
// ld [%Oreg + offs], %r1 ; ld [%r1 + offs1], %r2; ... ; jmp %rN
// We guess! that it never occurs in the jump table code. Here is why:
// The code corresponds to a chain of pointer dereferences. The first
// pointer is passed as an argument to the function (%Oreg), which is 
// uncommon of jump tables, since they are not visible from the outside.
// The contents of %r1 is an absolute address, not an index (we do 
// ld [%r1 + offs], %r2). Again, this pointer is visible from the outside,
// so it does not belong to a jump table, ... Overall, it is just a guess.
static bool
handleJmpAsIndirectTailCall(bbid_t bb_id,
			    sparc_instr *instr,
			    kptr_t insnAddr,
			    bool delaySlotInSameBlock,
			    sparc_instr *dsInsn,
			    const simplePathSlice_t &theSlice,
			    parseOracle *theParseOracle)
{
    instr_t *firstInstr;
    sparc_reg rd = sparc_reg::g0;
    sparc_reg rs1 = sparc_reg::g0;
    int imm;

    theSlice.beginBackwardsIteration();
    if (theSlice.isEndOfIteration()) {
	return false;
    }
    while (!theSlice.isEndOfIteration()) {
	firstInstr = theSlice.getCurrIterationInsn();
	if (!((sparc_instr*)firstInstr)->isLoadRegOffset(rd, rs1, imm)) {
	    return false;
	}
	theSlice.advanceIteration();
    }
    // Check the source register of the first instruction
    if (!sparc_reg_set::allOs.exists(rs1)) {
	return false;
    }
    
    theParseOracle->processIndirectTailCall(bb_id, instr, insnAddr,
                                            delaySlotInSameBlock, dsInsn);
    dout << "Identified jmp at " << addr2hex(insnAddr) 
	 << " as an indirect tail call\n";
    return true; // for now
}

// Determines whether the code loads the end of the jump table in regTail 
// and the size of the jump table in regSize. Modifies *ptail and *psize
// to contain the actual values of the end address and the size.
static bool isTailAndSize(const sparc_reg &regTail,
			  const simplePathSlice_t &sliceTail,
			  const sparc_reg &regSize,
			  const simplePathSlice_t &sliceSize,
			  uint64_t *ptail,
			  uint32_t *psize)
{
    uint64_t tail;
    unsigned size = 0;

    if ((tail = isLoadJumpTableAddr64(regTail, sliceTail)) == 0) {
	return false;
    }
    *ptail = tail;

    // Verify that the first insn is a load
    sliceSize.beginBackwardsIteration();
    if (sliceSize.isEndOfIteration()) {
	return false;
    }

    instr_t *ldsw = sliceSize.getCurrIterationInsn();
    sparc_reg rs1 = sparc_reg::g0, rs2 = sparc_reg::g0;
    if (!((sparc_instr*)ldsw)->isLoadDualRegToDestReg(regSize, rs1, rs2)) {
	return false;
    }

    // Verify that the second insn is an addImm
    sliceSize.advanceIteration();
    if (sliceSize.isEndOfIteration()) {
	return false;
    }
    instr_t *add = sliceSize.getCurrIterationInsn();
    if (!((sparc_instr*)add)->isAddImmToDestReg(regSize, rs1, size) || (int)size > 0) {
	return false;
    }
    *psize = -size; // Size was negative !
    
    return true;
}

// Determines whether the code corresponds to a backward offset jump table.
// Offsets are negative 32-bit integers that are specified from the end of 
// the jump table
static bool handleBackwardOffset32JumpTable(bbid_t bb_id,
					    sparc_instr *jumpInsn,
					    bool delaySlotInThisBlock,
					    sparc_instr *delaySlotInsn,
					    kptr_t jumpInsnAddr,
					    const simplePathSlice_t &sliceR,
					    sparc_reg Rjump,
					    const simplePath &pathFromEntryBB,
					    // doesn't yet include curr bb
					    parseOracle *oracle) 
{
    sparc_reg srcReg1 = sparc_reg::g0, srcReg2 = sparc_reg::g0;
    const function_t *fn = oracle->getFn();

    // This kind of jump table exists in the 64-bit kernel only
    if (!is64BitKernel()) {
	return false;
    }
    // Let's make sure that the instruction before "jump rd" is 
    // add r1, r2, rd
    sliceR.beginBackwardsIteration();
    if (sliceR.isEndOfIteration()) {
	return false;
    }
    instr_t *add = sliceR.getCurrIterationInsn();
    if (!((sparc_instr*)add)->isAdd2RegToDestReg(Rjump, srcReg1, srcReg2)) {
	return false;
    }

    sparc_reg_set setR1(srcReg1);
    simplePathSlice_t sliceR1(sliceR, 1, false, setR1);

    sparc_reg_set setR2(srcReg2);
    simplePathSlice_t sliceR2(sliceR, 1, false, setR2);

    uint64_t tail;
    uint32_t size;

    if (!isTailAndSize(srcReg1, sliceR1, srcReg2, sliceR2, &tail, &size) &&
	!isTailAndSize(srcReg2, sliceR2, srcReg1, sliceR1, &tail, &size)) {
	return false;
    }

    kptr_t jumpTableStartAddr = tail - size;
    pdvector<uint32_t> jumpTableRawEntries;
    readFromKernelInto(jumpTableRawEntries, jumpTableStartAddr, tail);
    
    pdvector<uint32_t>::const_iterator iter = jumpTableRawEntries.begin();
    pdvector<uint32_t>::const_iterator finish = jumpTableRawEntries.end();
    pdvector<kptr_t> jumpTableBlockAddrs;
    for (; iter != finish; iter++) {
	int offset = *iter; // the byte offset (negative !), as we want.
	kptr_t addr = tail + offset;
	// reminder: entries in this jump table are byte offsets from
	// the end of the jump table!

	if (!fn->containsAddr(addr)) {
	    // garbage entry; the jump table is considered ended.
	    if (verbose_fnParse) {
		cout << "ended backward-offset-32 jump table due to "
		     << "garbage entry" << endl;
		cout << "please verify: startAddr = " 
		     << addr2hex(fn->getEntryAddr())
		     << " jmp tab start=" 
		     << addr2hex(jumpTableStartAddr) << endl;
	    }
	    // OK, we have stopped adding to jumpTableBlockAddrs[] a
	    // little earlier than expected.  We should trim 
	    // jumpTableRawEntries[] as well (even though, at present, that 
	    // variable isn't used below here except in an assert, but best 
	    // to be safe.)
	    assert(jumpTableBlockAddrs.size() > 0);
	    jumpTableRawEntries.shrink(jumpTableBlockAddrs.size());
	    break;
	}
	else {
	    jumpTableBlockAddrs += addr;
	}
    }
    assert(jumpTableBlockAddrs.size() > 0);
    assert(jumpTableBlockAddrs.size() == jumpTableRawEntries.size());

    oracle->processOffsetJumpTable32WithTentativeBounds(bb_id,
						       jumpInsn,
						       jumpInsnAddr,
						       delaySlotInThisBlock,
						       delaySlotInsn,
						       jumpTableStartAddr,
						       jumpTableBlockAddrs,
						       sliceR, // dummy !
						       pathFromEntryBB);

    return true;
}

class IsBranchDestAddrWithinSliceOracle {
 private:
   const simplePathSlice_t theSlice; // NOT a reference; make it a true copy
   
 public:
   IsBranchDestAddrWithinSliceOracle(const simplePathSlice_t &iSlice) :
      theSlice(iSlice) {
   }
   
   bool operator()(kptr_t addr) const {
      theSlice.beginBackwardsIteration();
      while (!theSlice.isEndOfIteration()) {
         if (addr == theSlice.getCurrIterationInsnAddr())
            return true;
         else
            theSlice.advanceIteration();
      }

      return false;
   }
};

static unsigned
tryToCalcMaxNumOffset32JumpTableEntries_part2(const function_t */*fn*/,
                                              kptr_t /*jumpInsnAddr*/,
                                              const simplePathSlice_t &REG1AndPCslice,
                                              sparc_reg REG1,
                                              const IsBranchDestAddrWithinSliceOracle &isBranchDestAddrWithinSlice
                                              ) {
   REG1AndPCslice.beginBackwardsIteration();
   if (REG1AndPCslice.isEndOfIteration())
      return UINT_MAX;
   instr_t *branchInsn = REG1AndPCslice.getCurrIterationInsn();

   sparc_instr::sparc_cfi cfi;
   branchInsn->getControlFlowInfo(&cfi);
   
   if (cfi.fields.controlFlowInstruction &&
       (cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc) &&
          // sorry, this routine doesn't understand the other kinds of
          // conditional branches
       (cfi.condition != sparc_instr::iccAlways &&
        cfi.condition != sparc_instr::iccNever)) {
      // A truly conditional integer branch instruction of type Bicc or BPcc
      const kptr_t branchDestAddr =
         REG1AndPCslice.getCurrIterationInsnAddr() + cfi.offset_nbytes;
      
      sparc_instr::IntCondCodes theConditionToStayOnCourse;
      if (isBranchDestAddrWithinSlice(branchDestAddr))
         theConditionToStayOnCourse = (sparc_instr::IntCondCodes)cfi.condition;
      else
         theConditionToStayOnCourse =
            sparc_instr::reversedIntCondCodes[cfi.condition];

      // This is being a little lazy...another sub-slice is called for...but at
      // the moment I'm going to assume that the compare instruction immediately
      // precedes the branch instruction, within REG1AndPCslice
      REG1AndPCslice.advanceIteration();
      if (REG1AndPCslice.isEndOfIteration())
         return UINT_MAX;
      
      instr_t *cmp_insn = REG1AndPCslice.getCurrIterationInsn();
      int cmp_val = INT_MAX;
      if (((sparc_instr*)cmp_insn)->isCmp(REG1, cmp_val)) {
         switch (theConditionToStayOnCourse) {
            case sparc_instr::iccLessOrEq:
            case sparc_instr::iccLessOrEqUnsigned:
               if (cmp_val > 0)
                  return cmp_val+1; // 0 to cmp_val, *inclusive* means cmp_val+1 entries
               else
                  return UINT_MAX;
               break;
            case sparc_instr::iccLess:
            case sparc_instr::iccLessUnsigned:
               if (cmp_val > 0)
                  return cmp_val; // 0 to cmp_val-1, *inclusive* means cmp_val entries
               else
                  return UINT_MAX;
               break;
            default:
               return UINT_MAX;
         }

         assert(false);
      }
      else
         return UINT_MAX;
   }
   else
      // sorry, couldn't determine the bounds
      return UINT_MAX;
}

static unsigned
tryToCalcMaxNumOffset32JumpTableEntries(const function_t *fn,
                                        const kptr_t jumpInsnAddr, // just for cout
                                        const simplePath &pathFromEntryBB,
                                           // doesn't include bb_id yet, tho'
                                        const bbid_t bbid,
                                        sparc_reg loadOffsetReg,
                                        const IsBranchDestAddrWithinSliceOracle &isBranchDestAddrWithinSlice,
                                        bool haveJumpAndNopInsnsBeenAddedToBlockYet
                                        ) {
   // How many insn bytes to skip at the end? Our final goal is to get
   // past the ld instruction that writes to loadOffsetReg. We do that in
   // two steps: create a slice on loadOffsetReg skipping 0 or 8
   // instructions to get to "ld" and then a subslice on the same register,
   // skipping 1 instruction to get past the "ld"

   sparc_reg_set tmpRegSet(loadOffsetReg);
   tmpRegSet += sparc_reg::reg_pc; // include ctl dependencies in the slice
   const simplePathSlice_t tmpSlice
                          (*fn,
                           simplePath(pathFromEntryBB, bbid),
                           tmpRegSet,
                           false,
                           haveJumpAndNopInsnsBeenAddedToBlockYet ? 8 : 0);

   // Skip the load insn at the end
   sparc_reg_set offsetRegSet(loadOffsetReg);
   offsetRegSet += sparc_reg::reg_pc; // include ctl dependencies in the slice
   const simplePathSlice_t loadOffsetRegSliceWithCtlDeps(tmpSlice, 1, false, 
							 offsetRegSet);
//     cout << "Found offset-32 jump table fn " << fn->getPrimaryName().c_str()
//          << "; jump insn addr="
//          << addr2hex(jumpInsnAddr) << endl;

   // The slice should end in:
   // sll REG1, 2, loadOffsetReg   (the regs may be the same; maybe not)
   loadOffsetRegSliceWithCtlDeps.beginBackwardsIteration();
   if (loadOffsetRegSliceWithCtlDeps.isEndOfIteration()) {
      assert(false); // too strong?
      return UINT_MAX;
   }

   instr_t *loadOffsetRegSliceLastInsn =
      loadOffsetRegSliceWithCtlDeps.getCurrIterationInsn();
   
   unsigned shiftAmt = 0;
   sparc_reg REG1 = sparc_reg::g0;
   if (!((sparc_instr*)loadOffsetRegSliceLastInsn)->isShiftLeftLogicalToDestReg(loadOffsetReg,
						       REG1, shiftAmt)) {
       // assert(false); // fas_onebyte_msg() can't be analyzed this way
       return UINT_MAX;
   }
   
   if (shiftAmt != 2) {
      assert(false);
      return UINT_MAX;
   }
   
   // For the next phase, get a sub-slice of REG1.
   sparc_reg_set REG1AndPCSet(REG1);
   REG1AndPCSet += sparc_reg::reg_pc;
   
   simplePathSlice_t REG1AndPCslice(loadOffsetRegSliceWithCtlDeps,
                                    1, // skip 1 insn at the end, the sll
                                    false, // no ctl deps (pc already in regs-to-slice)
                                    REG1AndPCSet);
   
   return tryToCalcMaxNumOffset32JumpTableEntries_part2(fn,
                                                        jumpInsnAddr, // just for cout
                                                        REG1AndPCslice,
                                                        REG1,
                                                        isBranchDestAddrWithinSlice);
}

static bool handleJmplAsOffsetJumpTable32(bbid_t bb_id,
                                          sparc_instr *jumpInsn,
                                          bool delaySlotInThisBlock,
                                          sparc_instr *delaySlotInsn,
                                          kptr_t jumpInsnAddr,
                                          const simplePathSlice_t &sliceR1,
                                          const simplePathSlice_t &sliceR2,
                                          sparc_reg R1jump,
                                          sparc_reg R2jump,
                                          const simplePath &pathFromEntryBB,
                                             // doesn't yet include curr bb
                                          const kmem */*km*/,
                                          parseOracle *theParseOracle,
                                          bool haveJmpAndNopInsnsBeenAddedToCFGYet) {
   // "Offset" jump table: the jump table, which starts at a fixed 32-bit address,
   // contains not absolute 32-bit addresses, but instead, 32-bit *offsets* (from
   // the start of the jump table).
   // Other than that curious wrinkle, handling this kind of jump table is similar
   // to "simple" jump tables (handleJmplAsSimpleJumpTable32).

   kptr_t jumpTableStartAddr=0;
   bool sliceR1LoadsJumpTableAddr=false;
   bool sliceR2LoadsJumpTableAddr=false;
   findWhichRegLoadsJumpTableAddr(R1jump, R2jump, sliceR1, sliceR2,
				  sliceR1LoadsJumpTableAddr,
				  sliceR2LoadsJumpTableAddr,
				  jumpTableStartAddr);

   if (!sliceR1LoadsJumpTableAddr && !sliceR2LoadsJumpTableAddr)
      return false; // neither -- not a jump table that we recognize
   if (sliceR1LoadsJumpTableAddr && sliceR2LoadsJumpTableAddr)
      return false; // both?!

   // OK, we've got the jump table address (call it %j)
   // Time to work on the other register (call it %other).
   // The last insn in a slice of %other should be ld [%j + %offset], %other,
   // where %offset is a register (perhaps the same as %other) that represents
   // an offset from the start of the jump table where the load takes place.
   const sparc_reg jumpTableReg = sliceR1LoadsJumpTableAddr ? R1jump : R2jump;
   const sparc_reg otherReg = sliceR1LoadsJumpTableAddr ? R2jump : R1jump;
   const simplePathSlice_t &otherSlice = sliceR1LoadsJumpTableAddr ? sliceR2 : sliceR1;
   otherSlice.beginBackwardsIteration();
   if (otherSlice.isEndOfIteration())
      return false;
   instr_t *otherSlice_lastinsn = otherSlice.getCurrIterationInsn();
   sparc_reg otherSlice_loadR1 = sparc_reg::g0;
   sparc_reg otherSlice_loadR2 = sparc_reg::g0;
   if (!((sparc_instr*)otherSlice_lastinsn)->isLoadDualRegToDestReg(otherReg,
						    otherSlice_loadR1,
						    otherSlice_loadR2))
      return false;
   // One of the 2 regs used in the load should be jumpTableReg...
   if (otherSlice_loadR1 != jumpTableReg && otherSlice_loadR2 != jumpTableReg)
      return false;
   if (otherSlice_loadR1 == jumpTableReg && otherSlice_loadR2 == jumpTableReg)
      return false;
   // ...and the other reg used in the load: let's call it loadOffsetReg
   const sparc_reg loadOffsetReg = (otherSlice_loadR1 == jumpTableReg ? otherSlice_loadR2 : otherSlice_loadR1);
   
   // Let's now work with loadOffsetReg.
   // Its slice should end in "sll %src, 2, %loadOffsetReg"
   sparc_reg_set loadOffsetRegsToSlice(loadOffsetReg);
   simplePathSlice_t loadOffsetReg_slice(otherSlice, 1, false, loadOffsetRegsToSlice);
   loadOffsetReg_slice.beginBackwardsIteration();
   if (loadOffsetReg_slice.isEndOfIteration())
      return false;
   instr_t *loadOffsetRegSlice_lastinsn = loadOffsetReg_slice.getCurrIterationInsn();
   unsigned shiftAmt = 0;
   sparc_reg ignoreReg = sparc_reg::g0;
   if (!((sparc_instr*)loadOffsetRegSlice_lastinsn)->isShiftLeftLogicalToDestReg(loadOffsetReg,
                                                                ignoreReg,
                                                                shiftAmt))
      return false;
   if (shiftAmt != 2)
      return false;

   // Okay, we're now pretty satisfied that we've seen an "offset-32" style jump table.
   // The tricky part is that we don't know where the jump table ends.
   // We parse jump table entries recursively.  If a next would-be jump table
   // entry is the start of some basic block, or the start of the next fn,
   // or goes backwards when all previously seen ones go forward, then we stop.
   // NOTE: in practice, it looks like the offset-32 jump tables are located
   // **before** the function being parsed.  If they all conform to this pattern,
   // then things are greatly simplified -- we can expect them to stumble onto the
   // start of us, the function currently being parsed.  For now, we'll assert that.

   const function_t *fn = theParseOracle->getFn();

//     cout << "Found offset-32 jump table within fn "
//          << addr2hex(fn->getEntryAddr()) << " " << fn->getPrimaryName().c_str()
//          << "; jump insn is at " << addr2hex(jumpInsnAddr)
//          << "; loadOffsetRegSlice is:" << endl;
//         loadOffsetReg_slice.printx(cout);
//     cout << "And the jump table start-addr slice is:" << endl;
//     (sliceR1LoadsJumpTableAddr ? sliceR1 : sliceR2).printx(cout);

   const IsBranchDestAddrWithinSliceOracle isBranchDestAddrWithinSlice(otherSlice);
   const unsigned max_num_jumptable_entries_ifknown =
      tryToCalcMaxNumOffset32JumpTableEntries(fn,
                                              jumpInsnAddr, // just for cout'ing
                                              pathFromEntryBB, bb_id,
                                              otherReg,
                                              isBranchDestAddrWithinSlice,
                                              haveJmpAndNopInsnsBeenAddedToCFGYet);

   if (max_num_jumptable_entries_ifknown == UINT_MAX) {
       dout << "Caution: unknown # of entries in jump table, jumpInsnAddr="
	    << addr2hex(jumpInsnAddr) << " fn "
	    << fn->getPrimaryName().c_str() << endl;
       return false;
   }

   fnCode::const_iterator chunk_iter =
      fn->getOrigCode().findChunk(jumpInsnAddr, false);
      // false --> doesn't have to be start of chunk

   if (jumpTableStartAddr >= chunk_iter->startAddr) {
      cout << "WARNING: looked like an offset-32 jump table, but it doesn't start **before**" << endl;
      cout << "the current code chunk, as we had expected" << endl;
      return false;
   }

   // We now want to pre-read the jump table's data contents into memory.
   // We could use kmem's getinsn(), but it's slow in the general case.

   pdvector<uint32_t> jumpTableRawEntries; // offsets from jumpInsnAddr
   readFromKernelInto(jumpTableRawEntries,
                      jumpTableStartAddr,
		      chunk_iter->startAddr);
   if (max_num_jumptable_entries_ifknown != UINT_MAX) {
      assert(max_num_jumptable_entries_ifknown <= jumpTableRawEntries.size());
      if (max_num_jumptable_entries_ifknown < jumpTableRawEntries.size())
         jumpTableRawEntries.shrink(max_num_jumptable_entries_ifknown);
//        cout << "Was able to determine that jump table (jumpInsnAddr=" 
//             << addr2hex(jumpInsnAddr)
//             << ") contained "
//             << jumpTableRawEntries.size() << " entries." << endl;
   }
   else {
      dout << "Caution: unknown # of entries in jump table, jumpInsnAddr="
           << addr2hex(jumpInsnAddr) << " fn "
           << fn->getPrimaryName().c_str() << endl;
   }

//     if (verbose_fnParse) {
//        cout << "At most, the jump table consists of the following data, starting at "
//             << addr2hex(jumpTableStartAddr) << endl;
//     }

   pdvector<uint32_t>::const_iterator iter = jumpTableRawEntries.begin();
   pdvector<uint32_t>::const_iterator finish = jumpTableRawEntries.end();
   pdvector<kptr_t> jumpTableBlockAddrs;
   for (; iter != finish; ++iter) {
      unsigned offset = *iter; // the byte offset, as we want.
      kptr_t addr = jumpTableStartAddr + offset;
         // reminder: entries in an offset32 jump table are byte offsets from
         // the start of the jump table (not from the jump insn)

      // Of course, we can't translate addr in bb_id, because that basic block
      // may not exist yet (it would if we're parsing codeObjects, but not if we're
      // creating a control flow graph).
      
      if (!instr_t::instructionHasValidAlignment(addr) || !fn->containsAddr(addr)) {
         // garbage entry; the jump table is considered ended.
         if (verbose_fnParse) {
            cout << "ended offset-32 jump table due to garbage entry" << endl;
            cout << "please verify: startAddr=" << addr2hex(fn->getEntryAddr())
                 << " jmp tab start=" << addr2hex(jumpTableStartAddr) << endl;
         }

         // OK, we have stopped adding to jumpTableBlockAddrs[] a little earlier
         // than expected.  We should trim jumpTableRawEntries[] as well (even though,
         // at present, that variable isn't used below here except in an assert, but
         // best to be safe.)
         assert(jumpTableBlockAddrs.size() > 0);
         jumpTableRawEntries.shrink(jumpTableBlockAddrs.size());
         break;
      }
      else
         jumpTableBlockAddrs += addr;
   }

   assert(jumpTableBlockAddrs.size() > 0);
   assert(jumpTableBlockAddrs.size() > 0);
   assert(jumpTableBlockAddrs.size() == jumpTableRawEntries.size());

   if (verbose_fnParse) {
      cout << "About to process offset32 jump table; startAddr="
           << addr2hex(jumpTableStartAddr) << " and entries are (at most):"
           << endl;
      for (pdvector<kptr_t>::const_iterator iter = jumpTableBlockAddrs.begin();
           iter != jumpTableBlockAddrs.end(); ++iter) {
         cout << addr2hex(*iter) << ' ';
      }
      cout << endl;
   }
   
   theParseOracle->processOffsetJumpTable32WithTentativeBounds(bb_id,
                                                              jumpInsn,
                                                              jumpInsnAddr,
                                                              delaySlotInThisBlock,
                                                              delaySlotInsn,
                                                              jumpTableStartAddr,
                                                              jumpTableBlockAddrs,
                                                                 // tentative!  May be
                                                                 // too many!!!
                                                              sliceR1LoadsJumpTableAddr ? sliceR1 : sliceR2,
                                                              pathFromEntryBB);
                                                              
   return true; // successfully found the jump table
}

static bool
handleJmplInstr(bbid_t bb_id,
                instr_t *instr, kptr_t instrAddr,
                sparc_instr::sparc_cfi *cfi,
		const simplePath &pathFromEntryBB, // doesn't include curr bb yet
                kptr_t stumbleOntoAddr, // next bb or (kptr_t)-1
                const kmem *km,
                parseOracle *theParseOracle) {
   typedef function_t::bbid_t bbid_t;
   
   // First we check for a form of tail-call optimization: jmp <address>; restore

   // Next we check for a jump to a fixed address: sethi; bset; jmpl; ds
   // The fixed addr  may be inter- or intra-procedural; we can handle either.

   // We need to check for a jump table:
   // sll <?>, 2, R1
   // sethi %hi(jump table addr), R2
   // or R2, %lo(jump table addr), R2
   // ld [R1 + R2], Rjump
   // jmpl Rjump
   // nop
   // <addr0> <in all probability, the jump table begins here -- 2 insns after the jmp>
   // <addr1>
   // <addr2>
   // ...
   // <n-1>   where n is the number of jump table entries
   //
   // Borrowed from the EEL '95 paper, we take backward slice on the "jmp Rjump" insn,
   // along some path from entry to this bb.  Note that the parameter "pathFromEntryBB"
   // already provides us with the path (sans this bb).
   // If this is a jump table, then the last insn in the slice is of the
   // form ld[R1+R2], Rjump.  If R1/R2 differ, then we do two more backwards slices, on
   // R1 and R2, respectively.  One of those slices should contain just two instrs
   // which load the address of the jump table (sethi; bset).  If all this occurs, then
   // assume that we have a jump table.  The start addr of the jump table is known.
   // Finding out where the jump table ends is tougher.  In fact, we don't even try to
   // determine this directly.  Instead, we fully processs jump table entries, one at a
   // time, until what would be the next jump table entry instead stumbles onto an
   // existing bb, at which time we decide that the jump table has ended.
   // Note: Borrowing from the EEL '95 paper, we compute the slice along "some" path
   // to this bb.  The problem with this approach is that we're assuming that *every*
   // path to this bb will load the same jump table offset.  Of course, *if* this is
   // truly a jump table (which we don't know in advance!) then the assumption is a safe
   // one.  In general, it's not safe.  A more general solution, imho, is to perform
   // a constant propagation (in reverse?) to determine if the target of the jump table
   // is truly a constant.
   //
   // WARNING: currently, we don't stop looking at jump table entries, even when we
   // reach the end of a function.  This leads to an assert failure in the vector
   // class.

   const function_t *fn = theParseOracle->getFn();
   
   instr_t *delaySlotInsn = km->getinsn(instrAddr+4);
   assert(delaySlotInsn->getDelaySlotInfo() == instr_t::dsNone);
      // possible, but would be messy to handle
   
   // Delay slot is in this block if: (1) it isn't part of
   // another already parsed basic block, and (2) is w/in bounds of this function
   const bool delaySlotInAnotherBlockSameFunction =
      stumbleOntoAddr != (kptr_t)-1 && instrAddr+4 == stumbleOntoAddr;
   const bool delaySlotInAnotherFunction =
      !delaySlotInAnotherBlockSameFunction &&
      !theParseOracle->getFn()->containsAddr(instrAddr+4);
   const bool delaySlotInSameBlock =
      !delaySlotInAnotherBlockSameFunction && !delaySlotInAnotherFunction;

   if (delaySlotInAnotherBlockSameFunction)
      // Any delay slot instruction beginning a basic block must also be the
      // only instruction of that basic block.  Necessary.
      theParseOracle->ensureBlockStartInsnIsAlsoBlockOnlyInsn(stumbleOntoAddr,
							      delaySlotInsn);
   
   if (delaySlotInsn->isRestore()) {
      // jmp <address>; restore is a tail call, much like call; restore (except
      // to a function pointer instead of to a fixed address).

      theParseOracle->processJmplRestoreOptimizedTailCall(bb_id, instr,
                                                        instrAddr,
                                                        delaySlotInSameBlock,
                                                        delaySlotInsn);
      delete cfi;      
      return false; // false --> done parsing this bb
   }

   // Make sure that the jmpl throws away the link (e.g. jmpl [address], %g0)
   sparc_instr::sparc_ru ru;
   instr->getRegisterUsage(&ru);
   *((sparc_reg_set*)ru.definitelyUsedBeforeWritten) -= sparc_reg::g0;

   const unsigned numRegsWritten = ru.definitelyWritten->count();
      // not countIntRegs()...we want to include %pc, right?

   if (numRegsWritten > 1) { // 1 --> will always write to %pc
      // doesn't write to %g0
      dout << "WARNING: handleJmplInstr -- jmpl doesn't throw away the link; can't parse function @" << addr2hex(fn->getEntryAddr()) << endl;
      delete instr;
      delete cfi;
      delete km;
      delete delaySlotInsn;
      throw function_t::parsefailed();
   }

   // Invariant: "jmpl" is actually "jmp"
   // At least 1 reg should be used in the jump:
   const unsigned numRegsUsed = ru.definitelyUsedBeforeWritten->count();
   if (numRegsUsed == 0) {
      dout << "WARNING: handleJmplInstr -- unanalyzable jump: no regs used (only g0)?" 
           << endl;
      delete instr;
      delete cfi;
      delete km;
      delete delaySlotInsn;
      throw function_t::parsefailed();
   }

   // We handle the case of (1) vs. (2) regs used in the jump insn differently.
   if (numRegsUsed == 1) {
      const sparc_reg Rjump = ((sparc_reg_set*)ru.definitelyUsedBeforeWritten)->expand1();

      const bool haveJumpAndDelayInsnsBeenAddedToBlockYet =
         theParseOracle->hasCFGBeenFullyParsedYet();

      // Obtain backwards slice on Rjump along the path "pathFromEntryBB".
      sparc_reg_set regsToSlice(Rjump);
      regsToSlice += sparc_reg::reg_pc;
      const simplePathSlice_t theSliceWithCtlDeps(*fn,
                                                  simplePath(pathFromEntryBB, bb_id),
                                                  regsToSlice,
                                                  false, // exclude control dependencies -- %pc is already in the set
                                                  haveJumpAndDelayInsnsBeenAddedToBlockYet ? 8 : 0);
         // want to ignore jmp & delay at the end, which is trivial if parsing
         // for the first time (not added to bb yet --> hence the 0)...but not
         // so trivial if CFG is already parsed (e.g. doing code object parsing)

      if (handleBackwardOffset32JumpTable(bb_id,
					  (sparc_instr*)instr,
					  delaySlotInSameBlock,
					  (sparc_instr*)delaySlotInsn,
					  instrAddr,
					  theSliceWithCtlDeps, Rjump,
					  pathFromEntryBB,
					  theParseOracle)) {
         delete cfi;
	 return false; // false --> done with bb
      }
      if (handleJmplAsJmpToFixedAddr(bb_id,
                                     (sparc_instr*)instr,
                                     instrAddr,
                                     delaySlotInSameBlock,
                                     (sparc_instr*)delaySlotInsn,
                                     theSliceWithCtlDeps, Rjump,
                                     pathFromEntryBB,
				     cfi,
                                     theParseOracle))
         return false; // false --> done with bb

      if (handleJmplAsSimpleJumpTable32(bb_id,
                                        (sparc_instr*)instr, 
					delaySlotInSameBlock, 
					(sparc_instr*)delaySlotInsn,
                                        instrAddr,
                                        theSliceWithCtlDeps, Rjump,
                                        pathFromEntryBB,
                                        km,
                                        theParseOracle)) {
         delete cfi;
         return false; // false --> done with bb
      }
      if (handleJmplAsTaggedJumpTable(bb_id,
                                      (sparc_instr*)instr, 
				      delaySlotInSameBlock, 
				      (sparc_instr*)delaySlotInsn,
                                      instrAddr, theSliceWithCtlDeps, Rjump,
                                      pathFromEntryBB,
                                      km,
                                      theParseOracle)) {
         delete cfi;
         return false; // false --> done with bb
      }

      if (handleJmplAsNonLoadingBackwardsJumpTable(bb_id,
                                                   (sparc_instr*)instr,
                                                   delaySlotInSameBlock, 
						   (sparc_instr*)delaySlotInsn,
                                                   instrAddr,
                                                   theSliceWithCtlDeps,
                                                   Rjump, pathFromEntryBB,
                                                   theParseOracle
                                                   )) {
         delete cfi;
	 return false; // false --> done with bb
     }
      regsToSlice = sparc_reg_set(Rjump); // needs to be reset since modified
      const simplePathSlice_t theSliceWithoutCtlDeps(*fn,
                                                     simplePath(pathFromEntryBB, bb_id),
                                                     regsToSlice,
                                                     false, // exclude ctrl dependencies
                                                     0);
         // skip 0 insns at end (jmp & delay haven't yet been added)

      // The following is interesting: jmp <reg> + 8 (plus any delay slot insn)
      // can have the effect of a return for any integer <reg>, provided that a
      // backwards slice on <reg> is simply one instruction: mov %o7, <reg>
      // Why would any routine do anything like this?  Simple (and in fact kperfmon
      // does the same trick): when the routine is going to make other procedure
      // calls (and thus trash %o7).  So in other words this routine is simply
      // saving the original %o7 in a non-volatile register (i.e. one that isn't
      // trashed by the call(s) this function makes).
      // Examples of kernel routines that use this trick: clk_thread,
      // current_thread, and intr_thread of sparc solaris 2.6/interrupt.s
      if (handleJmpAsRetlUsingASavedLinkRegister(bb_id,
                                                 (sparc_instr*)instr,
                                                 delaySlotInSameBlock, 
						 (sparc_instr*)delaySlotInsn,
                                                 instrAddr,
                                                 cfi, theSliceWithoutCtlDeps, 
						 Rjump, theParseOracle))
         return false; // done with bb
      
      if (handleJmpAsIndirectTailCall(bb_id, (sparc_instr*)instr, 
				      instrAddr, delaySlotInSameBlock, 
				      (sparc_instr*)delaySlotInsn,
				      theSliceWithoutCtlDeps,
				      theParseOracle)) {
          delete cfi;
	  return false;
      }
      if (verbose_fnParse) {
         cout << "unanalyzable jmpl using 1 register. ("
              << instr->disassemble(instrAddr)
              << "). Here's the slice:" << endl;
         theSliceWithCtlDeps.printx(cout);
      }
   }
   else if (numRegsUsed == 2) {
      sparc_reg_set::const_iterator regiter = ru.definitelyUsedBeforeWritten->begin();
      const sparc_reg R1jump = (sparc_reg&) *(regiter++);
      const sparc_reg R2jump = (sparc_reg&) *(regiter++);
      assert(regiter == ru.definitelyUsedBeforeWritten->end());

      // Obtain backwards slice on R1jump:
      const bool haveJumpAndNopInsnsBeenAddedToBlockYet =
         theParseOracle->hasCFGBeenFullyParsedYet();

      sparc_reg_set regsToSlice1(R1jump);
      simplePathSlice_t theSlice1(*fn,
                                  simplePath(pathFromEntryBB, bb_id),
                                  regsToSlice1, false,
                                  haveJumpAndNopInsnsBeenAddedToBlockYet ? 8 : 0);
         // the 8:0 is a little hairy; needed because jmp & delay insns haven't
         // yet been added, if we're parsing CFG, but have been added if e.g.
         // doing codeObjects.
         // false --> no ctl dependency in slice,
         // so e.g. a cond branch insn (& the insns before it that set cond codes)
         // won't be part of the slice based on that alone.
      
      // Obtain backwards slice on R2jump:
      sparc_reg_set regsToSlice2(R2jump);
      simplePathSlice_t theSlice2(*fn,
                                  simplePath(pathFromEntryBB, bb_id),
                                  regsToSlice2, false,
                                  haveJumpAndNopInsnsBeenAddedToBlockYet ? 8 : 0);
         // the 8:0 is a little hairy; needed because jmp & delay insns haven't
         // yet been added, if we're parsing CFG, but have been added if e.g.
         // doing codeObjects.
         // false --> no ctl dependency in slice,
         // so e.g. a cond branch insn (& the insns before it that set cond codes)
         // won't be part of the slice based on that alone.

      if (handleJmplAsOffsetJumpTable32(bb_id,
                                        (sparc_instr*)instr,
                                        delaySlotInSameBlock, 
					(sparc_instr*)delaySlotInsn,
                                        instrAddr, theSlice1, theSlice2,
                                        R1jump, R2jump, pathFromEntryBB,
                                        km, theParseOracle,
                                        haveJumpAndNopInsnsBeenAddedToBlockYet
                                        )) {
         delete cfi;					
         return false;
     }
   }

   dout << "WARNING: unanalyzable jmpl at " << addr2hex(instrAddr)
        << "; can't parse function at " << addr2hex(fn->getEntryAddr()) << endl;
   delete cfi;
   delete instr;
   delete delaySlotInsn;
   delete km;

   throw function_t::parsefailed();
   
   return false; // placate compiler
}

static bool
handleV9ReturnInstr(bbid_t bb_id,
                    // no basicblock & since refs too volatile
                    sparc_instr *insn, kptr_t instrAddr,
                    sparc_instr::sparc_cfi *cfi,
                    kptr_t stumbleOntoAddr, // next bb or (kptr_t)-1
                    const kmem *km,
                    parseOracle *theParseOracle) {
   // new with sparc v9 (don't confuse with the pseudo-instr ret or retl).
   // Does a restore (which doesn't concern us here) and a jump (no link).  There
   // is a delay slot insn, and it's always executed (like call or jmpl in that
   // respect).
   
   instr_t *delaySlotInsn = km->getinsn(instrAddr+4);
   assert(delaySlotInsn->getDelaySlotInfo() == instr_t::dsNone);
      // possible, but would be messy to handle
   
   // Delay slot is in this block if: (1) it isn't part of
   // another already parsed basic block, and (2) is w/in bounds of this function
   const bool delaySlotInAnotherBlockSameFunction =
      stumbleOntoAddr != (kptr_t)-1 && instrAddr+4 == stumbleOntoAddr;
   const bool delaySlotInAnotherFunction =
      !delaySlotInAnotherBlockSameFunction &&
      !theParseOracle->getFn()->containsAddr(instrAddr+4);
   const bool delaySlotInSameBlock =
      !delaySlotInAnotherBlockSameFunction && !delaySlotInAnotherFunction;

   if (delaySlotInAnotherBlockSameFunction)
      // Any delay slot instruction beginning a basic block must also be the
      // only instruction of that basic block.  Necessary.
      theParseOracle->ensureBlockStartInsnIsAlsoBlockOnlyInsn(stumbleOntoAddr,
							      delaySlotInsn);
   
   theParseOracle->processReturn(bb_id, insn, instrAddr,
				 delaySlotInSameBlock, delaySlotInsn);
   delete cfi;
   return false; // false --> caller shouldn't continue bb; we've completed it.
}

static bool
handleDoneOrRetryInstr(bbid_t bb_id,
                       // no basicblock & since refs are too volatile
                       sparc_instr *insn,
                       kptr_t insnAddr,
		       sparc_instr::sparc_cfi *cfi,
                       const kmem *,
                       parseOracle *theParseOracle) {
   // no delay slot insn; so no need to add to 'contents'
   theParseOracle->processDoneOrRetry(bb_id, insn, insnAddr);
   delete cfi;
   return false; // false --> bb is completed
}

static bool
handleConditionalBranchInstr(bbid_t bb_id,
                             bool branchAlways, bool branchNever,
                             kptr_t instrAddr,
                             sparc_instr *i,
                             sparc_instr::sparc_cfi *cfi,
			     const simplePath &pathFromEntryBB,
			        // doesn't include curr bb
                             kptr_t stumbleOntoAddr, // next bb or (kptr_t)-1
                             const kmem *km,
                             parseOracle *theParseOracle) {
   // Called in three places by handleControlFlowInstr (it's nice to share code)

   // quick assertion check
   assert(cfi->destination == instr_t::controlFlowInfo::pcRelative);

   // Handle the delay slot instruction, if any.  Here are the rules (recently
   // updated, 12/30/1999 --ari)
   // 1) If no delay slot instruction, or a delay slot instruction that is part
   //    of another basic block (stumbleOntoAddr), then never mind and don't do
   //    anything for that instruction
   // 2) else if a delay slot instruction that is conditionally executed (dsWhenTaken)
   //    [no such thing as dsWhenNotTaken in Sparc] then put the delay instruction
   //    in its own special 1-instruction basic block having this block as its
   //    precessor, and having the if-taken block as its successor.
   //    Optimization to avoid explosion in # of bbs: only do this in the case where the
   //    delay slot instruction is a save or restore instruction (where it's absolutely
   //    necessary, for correct register analysis, to avoid mergings blocks with
   //    differing numbers of saves & restores).
   // 3) else [a delay slot instruction that isn't conditionally executed, and isn't
   //    already in another basic block] add the delay slot instruction to this
   //    basic block.

   bool hasDelaySlotInsn = cfi->delaySlot != instr_t::dsNone;

   //assert(km);
   instr_t *delaySlotInsn = hasDelaySlotInsn ?
      km->getinsn(instrAddr+4) : new sparc_instr(sparc_instr::nop);
   //assert(delaySlotInsn);
   assert(delaySlotInsn->getDelaySlotInfo() == instr_t::dsNone);
      // possible, but would be messy to handle

   // Delay slot is in this block if: (1) ds exists, and (2) isn't part of
   // another already parsed basic block, and (3) is w/in bounds of this function
   // (3 is new; fixes a bug with, e.g., unix/got_result in Solaris7)
   const bool delaySlotInAnotherBlockSameFunction =
      hasDelaySlotInsn && stumbleOntoAddr != (kptr_t)-1 &&
      instrAddr+4 == stumbleOntoAddr;
   const bool delaySlotInAnotherFunction =
      hasDelaySlotInsn && !delaySlotInAnotherBlockSameFunction &&
      !theParseOracle->getFn()->containsAddr(instrAddr+4);
   const bool delaySlotInSameBlock =
      !delaySlotInAnotherBlockSameFunction && !delaySlotInAnotherFunction;
   
   if (delaySlotInAnotherBlockSameFunction)
      // Any delay slot instruction beginning a basic block must also be the
      // only instruction of that basic block.  Necessary.
      theParseOracle->ensureBlockStartInsnIsAlsoBlockOnlyInsn(stumbleOntoAddr,
                                                             delaySlotInsn);

   const kptr_t fallThroughAddr = instrAddr + (hasDelaySlotInsn ? 8 : 4);
      // The hasDelaySlotInsn check is needed because some sparc branches (namely
      // ba,a/bn and ba,a,px/bn,px) simply don't have a delay slot instruction.

   const kptr_t destAddr = instrAddr + cfi->offset_nbytes;

   theParseOracle->processCondBranch(bb_id, branchAlways, branchNever,
                                    i, cfi, instrAddr,
                                    hasDelaySlotInsn,
                                    delaySlotInSameBlock,
                                    delaySlotInsn,
                                    fallThroughAddr,
                                    destAddr,
                                    pathFromEntryBB);

   return false; // don't continue the basic block; we've finished parsing it.
}

bool sparc_controlFlow::handleControlFlowInstr(bbid_t bb_id,
					       sparc_instr *instr,
					       kptr_t &instrAddr,
					       // iff returning true, we modify this
					       sparc_instr::sparc_cfi *cfi,
					       const simplePath &pathFromEntryBB,
					       // doesn't yet include curr bb
					       kptr_t stumbleOntoAddr,
					       const kmem *km,
					       parseOracle *theParseOracle) {
  // Return true if the basic block should be continued (call instr -- though some
  // changes have been made as of 11/2000 that may make it return false anyway),
  // or false if the control flow instruction has ended the basic block (e.g. ret).

  // If (and only if) returning true, we adjust "instrAddr" so that it's the address
  // of the instruction at which parsing should continue.  It'll presumably be the
  // value on entry to this routine plus 4 (if no delay slot in this block) or plus
  // 8 (the more usual case).

  // First, handle calls to other functions; this includes
  // 'call' and 'jmpl <addr>, %o7'.  May be recursive, may just be
  // an attempt to read %pc into %o7, may be intraprocedural, may be to an
  // unknown function, may be an optimized tail call.
  if (cfi->fields.isCall) // normal call or jmpl xxx, %o7...
    return handleCallInstr(bb_id, instr, instrAddr, cfi, 
			   pathFromEntryBB, stumbleOntoAddr, 
			   km, theParseOracle);
   
  // Handle ret and retl (must be handled before handleJmplInstr(), below)
  if (cfi->fields.isRet || cfi->sparc_fields.isRetl)
    return handleRetOrRetlInstr(bb_id, instr, instrAddr, cfi,
				stumbleOntoAddr, km, theParseOracle);

  // Handle jump tables and (gulp) unanalyzable jumps   
  if (cfi->fields.isJmpLink)
    return handleJmplInstr(bb_id, instr, instrAddr, cfi, 
			   pathFromEntryBB, stumbleOntoAddr, km,
			   theParseOracle);

   
  if (cfi->sparc_fields.isBicc || cfi->sparc_fields.isBPcc) {
    // new with sparc v9, BPcc is much like bicc.
    return handleConditionalBranchInstr(bb_id,
					cfi->condition == sparc_instr::iccAlways,
					cfi->condition == sparc_instr::iccNever,
					instrAddr,
					instr,
					cfi,
					pathFromEntryBB,
					stumbleOntoAddr,
					km,
					theParseOracle);
  }

  if (cfi->sparc_fields.isBPr) {
    // BPr has a delay slot and an annul bit, but annulment takes place under VERY
    // different circumstances from bicc and BPcc; beware.
    // 1) If branch taken, delay slot always executed
    // 2) Else, annul bit is respected.
    assert(cfi->destination == instr_t::controlFlowInfo::pcRelative);
    return handleConditionalBranchInstr(bb_id,
					false, false, // BPr's are always conditional
					instrAddr,
					instr,
					cfi,
					pathFromEntryBB,
					stumbleOntoAddr,
					km,
					theParseOracle);
  }
   
  if (cfi->sparc_fields.isFBfcc || cfi->sparc_fields.isFBPfcc) {
    assert(cfi->destination == instr_t::controlFlowInfo::pcRelative);
    return handleConditionalBranchInstr(bb_id,
					cfi->condition == sparc_instr::fccAlways,
					cfi->condition == sparc_instr::fccNever,
					instrAddr,
					instr,
					cfi,
					pathFromEntryBB,
					stumbleOntoAddr,
					km,
					theParseOracle);
  }

  if (cfi->sparc_fields.isV9Return) {
    // new with sparc v9 (don't confuse with the pseudo-instr ret or retl).
    return handleV9ReturnInstr(bb_id, instr, 
			       instrAddr, cfi, stumbleOntoAddr,
			       km, theParseOracle);
  }

  if (cfi->sparc_fields.isDoneOrRetry) {
    // new with sparc v9.  Return from trap handler, using state saved in TSTATE.
    return handleDoneOrRetryInstr(bb_id, instr, instrAddr,
				  cfi, km, theParseOracle);
  }

  assert(false);
  return false; // placate compiler
}

#endif /* _KERNINSTD_ */
