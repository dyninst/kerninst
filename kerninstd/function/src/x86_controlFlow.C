// x86_controlFlow.C

#include "x86_controlFlow.h"
#include "x86_kmem.h"
#include "funkshun.h"
#include "out_streams.h"
#include "parseOracle.h"
#include "simplePath.h"
#include "simplePathSlice.h"

extern pdvector<kptr_t> functionsThatReturnForTheCaller;

const bool bbEndsOnANormalCall = false;
// false --> the old way.  Violates the strict definition of a basic block,
// in favor of this definition: once entered, all insns will be executed.
// This alternate definition definitely has its uses: for example, it's the 
// right granularity for basic block code coverage -- under the reasonable 
// assumption that all calls in the middle of the basic block will return. 
// And it's good enough for register analysis. And it reduces the number of 
// basic blocks.
// true --> a call insn will be the last things in a basic block. This
// may satisfy purists uncomfortable with kerninst straying from
// the strict definition of a basic block.

bool x86_controlFlow::
handleCallInstr(bbid_t bb_id,
		instr_t *callInstr,
		kptr_t &instrAddr,
		   // If returning true, modify this
		x86_instr::x86_cfi *cfi,
		const simplePath &pathFromEntryBB,
		kptr_t stumbleOntoAddr,
		   // next known bb startaddr, or (kptr_t)-1
		const kmem *km,
		parseOracle *theParseOracle)
{
   // called by handleControlFlowInstr when it detects a call.
   // Returns true iff the basic block parsing should be continued;
   // false if bb has ended.

   const function_t *fn = theParseOracle->getFn();
   const unsigned instrNumBytes = callInstr->getNumBytes();

   // quick assertion checks
   assert(cfi->fields.controlFlowInstruction);
   assert(cfi->delaySlot == instr_t::dsNone);

   if(cfi->destination == instr_t::controlFlowInfo::pcRelative) {
      const kptr_t destAddr = instrAddr + instrNumBytes + cfi->offset_nbytes;

      // Is it a call to some function which will return for us?
      if (std::binary_search(functionsThatReturnForTheCaller.begin(),
                             functionsThatReturnForTheCaller.end(),
                             destAddr)) {
         theParseOracle->processCallToRoutineReturningOnCallersBehalf(bb_id, 
					     callInstr, instrAddr, destAddr,
                                             false, //delaySlotInSameBlock
                                             (instr_t*)NULL //delaySlotInsn
                                                                      );
         delete cfi;	 
	 return false; // bb is done; call won't return
      }

      if (fn->containsAddr(destAddr)) {
         // intra-procedural call
         const bool recursion = (destAddr == fn->getEntryAddr());
         if(recursion) {
//            cout << "Recursion in fn " 
//                 << addr2hex(fn->getEntryAddr())
//                 << " "
//                 << fn->getPrimaryName().c_str()
//                 << endl;
            theParseOracle->processCallRecursion(bb_id, destAddr, callInstr,
                                                 instrAddr,
                                                 false, //delaySlotInSameBlock 
                                                 (instr_t*)NULL, //delaySlotInsn
                                                 pathFromEntryBB);
            delete cfi;
            return false; // bb is done
         }
         else {
            // Freaky: call to some place other than the start of function.
            theParseOracle->noteHiddenIntraProceduralFn(destAddr);
            // no "return" here; pretend it was a normal interproc call
            theParseOracle->getMutableFn()->bumpUpBlockSizeBy1Insn(bb_id, callInstr);
         }
      }
      else {
         // inter-procedural call
         bool tailCall = false;
         /* MJB TODO - check for tail call optimizations, what do they
                       look like on x86
         */
         if(tailCall) {
            /* MJB TODO - need to call some parseOracle method here, perhaps
               processTrueCallRestore is appropriate, but inaptly named
            */
	    delete cfi;
            return false; // bb is finished
         }
         else {
            theParseOracle->processNonTailOptimizedTrueCall(bb_id, callInstr,
                                                            instrAddr,
                                                            destAddr,
                                                            false,
                                                            (instr_t*)NULL);
         }
      }
   }
   else if(cfi->destination == instr_t::controlFlowInfo::memIndirect) {
      theParseOracle->getMutableFn()->bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   }
   else if(cfi->destination == instr_t::controlFlowInfo::regRelative) {
      theParseOracle->getMutableFn()->bumpUpBlockSizeBy1Insn(bb_id, callInstr);
   }
   else
      assert(!"x86_controlFlow::handleCallInstr - unexpected destination type\n");
   delete cfi;
   if (bbEndsOnANormalCall) {
      return false; // bb shouldn't be continued
   } else {
      // Since returning true, adjust instrAddr as appropriate:
      instrAddr += instrNumBytes;
      return true; // bb should be continued
   }
}

bool x86_controlFlow::
handleConditionalBranchInstr(unsigned bb_id,
			     x86_instr *instr,
			     kptr_t &instrAddr,
			     x86_instr::x86_cfi *cfi,
			     const simplePath &pathFromEntryBB,
			     kptr_t stumbleOntoAddr,
			     const kmem *km,
			     parseOracle *theParseOracle)
{
   const kptr_t fallThroughAddr = instrAddr + instr->getNumBytes();
   assert(cfi->destination == x86_instr::controlFlowInfo::pcRelative);
   const kptr_t destAddr = fallThroughAddr + cfi->offset_nbytes;

   theParseOracle->processCondBranch(bb_id, 
				     false, // branchAlways
				     false, // branchNever
				     instr, cfi, instrAddr,
				     false, // hasDelaySlot
				     false, // delaySlotInSameBlock
				     NULL, // delaySlotInsn*
				     fallThroughAddr,
				     destAddr, pathFromEntryBB);

   return false; // don't continue with bb, we're finished parsing it
}   

static kptr_t getJmpTableBaseAddr(x86_instr *instr,
				  kptr_t &instrAddr,
				  x86_instr::x86_cfi *cfi,
				  const simplePath &pathFromEntryBB,
				  const kmem *km,
				  const function_t *fn)
{
   // MJB TODO - need to do a backwards slice on the register
   //            specifed in order to find the jump table base addr
   return (kptr_t)0;
}

static bool handleJmpAsSimpleJumpTable32(unsigned bb_id, 
					 x86_instr *instr, 
					 kptr_t &jumpInstrAddr, 
					 x86_instr::x86_cfi *cfi,
					 const simplePath &pathFromEntryBB, 
					 kptr_t stumbleOntoAddr,
					 const kmem *km, 
					 parseOracle *theParseOracle, 
					 const function_t *fn,
					 kptr_t jumpTableStartAddr)
{
   regset_t *dummy_set = regset_t::getRegSet(regset_t::empty);
   simplePathSlice dummy_slice(*fn, pathFromEntryBB, *dummy_set, false, 0);
   theParseOracle->prepareSimpleJumpTable32(bb_id, instr, jumpInstrAddr,
					    false, NULL,
					    jumpTableStartAddr,
					    dummy_slice);
   delete dummy_set;

   // How to handle a jump table: the tricky part is that we don't know where
   // the jump table ends.  

   // So we process the first jump table entry (as the first successor to 
   // this bb) recursively.  Then, in general, if the next would-be jump 
   // table entry is the start of some basic block, then we stop; otherwise 
   // we recursively process it as the 2d successor to this bb.

   // It all continues until the next would-be jump table entry falls on top of
   // an existing basic block or the next known fn.

   bool allJumpsForwardSoFar = true;
   bool atLeastOneJumpForward = false;

   if (verbose_fnParse)
      cout << "Jump table (simple-32) begins at addr "
           << addr2hex(jumpTableStartAddr) << endl;

   unsigned jumpTableNdx = 0;
   while (true) {
      const kptr_t nextJumpTableEntryAddr = jumpTableStartAddr + 4*jumpTableNdx;

      bbid_t stumble_bb_id = fn->searchForBB(nextJumpTableEntryAddr);
      if (stumble_bb_id != (bbid_t)-1) {
	 // Stumbled onto existing bb of this fn.
	 assert(fn->getBasicBlockFromId(stumble_bb_id)->getStartAddr() ==
                nextJumpTableEntryAddr);
	 break;
      }

      // Now dereference "nextJumpTableEntryAddr", obtaining the actual data 
      // for one jump table entry:
      kptr_t nextJumpTableEntry = km->getdbwd(nextJumpTableEntryAddr);

      // If all jump table entries jump backwards, then it's quite likely 
      // that we won't be able to find the end of the jump table by stumbling 
      // onto an existing bb. When this occurs, we detect the end of jmp 
      // table by a jmp tab entry that appears to be garbage.

      // Is the jump table entry garbage (bad address)? 
      // Reminder: during parsing, chunks are if anything too large (they'll 
      // get trimmed when done parsing), so if the following test is false, 
      // then the jump table entry is certainly garbage.
      if (!fn->containsAddr(nextJumpTableEntry)) {
	 // If all jumps have been forward so far, then this shouldn't happen
	 if (allJumpsForwardSoFar) {
	    // All jmps fwd, so we would have hoped to stumble onto an 
	    // existing bb before having this happen.  In any event, time to 
	    // end the jmp table.
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
	    assert(false); // too strict?
	 }
	 else if (atLeastOneJumpForward) {
	    // At least 1 jmp has been fwd, so we would have hoped to stumble 
	    // onto an existing bb before having this happen.  In any event, 
	    // time to end the jmp table.
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
	    assert(false); // too strict?
	 }
	 else {
	    // Okay, all jumps have been backwards. That explains why we 
	    // haven't stumbled onto an existing bb. Time to end the jmp table.
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
      
      assert(fn->containsAddr(nextJumpTableEntry));

      if (nextJumpTableEntry < jumpInstrAddr)
	 allJumpsForwardSoFar = false;
      else
	 atLeastOneJumpForward = true;

      theParseOracle->process1SimpleJumpTable32Entry(bb_id, // uniquely id's the jmp table
						     nextJumpTableEntry,
						     pathFromEntryBB);
      
      jumpTableNdx++;
   } // while (true)

   // Done processing the jump table
   theParseOracle->completeSimpleJumpTable32(bb_id, jumpTableStartAddr);

   return false; // done with bb
}


static bool handleJmpTableJmpInstr(unsigned bb_id,
				   x86_instr *instr,
				   kptr_t &instrAddr,
				   x86_instr::x86_cfi *cfi,
				   const simplePath &pathFromEntryBB,
				   kptr_t stumbleOntoAddr,
				   const kmem *km,
				   parseOracle *theParseOracle,
				   const function_t *fn)
{
   // need to handle reg indirect and mem indirect
   // both types of jmp have opcode FF /4

   kptr_t jmpTableBaseAddr;
   if(cfi->destination == instr_t::controlFlowInfo::regRelative) {
      // register indirect example:
      // - 'FF E0' == 'jmp *eax'
      // - E0 -> (mod='11' : regOnly) (reg='100') (rm='000' : eax)
      // - therefore we need to do a backwards slice on the register
      //   specifed in order to find the jump table base addr
      jmpTableBaseAddr = getJmpTableBaseAddr(instr, instrAddr, cfi,
					     pathFromEntryBB, km, fn);
   }
   else if(cfi->destination == instr_t::controlFlowInfo::memIndirect) {
      if(cfi->memaddr.getType() == instr_t::address::scaledSingleRegPlusOffset)
         // memory indirect example: 
         // - 'FF 24 85 XXXXXXXX' == 'jmp *XXXXXXXX(,eax,4)'
         // - 24 -> (mod='00') (reg='100') (rm='100' : hasSIB) 
         // - 85 -> (scale='10' : 4) (index='000' : eax) (base='101' : hasDisp32)
         // - displacement specified in instruction is jmp table base addr
         jmpTableBaseAddr = cfi->memaddr.getOffset();
      else
         // memory indirect using offset from register, need backwards slice
         jmpTableBaseAddr = getJmpTableBaseAddr(instr, instrAddr, cfi,
                                                pathFromEntryBB, km, fn);
   }
   else
     assert(!"x86_controlFlow - unhandled jump-table jmp instr\n");

   if(jmpTableBaseAddr)
      return handleJmpAsSimpleJumpTable32(bb_id, instr, instrAddr, cfi,
					  pathFromEntryBB, stumbleOntoAddr,
					  km, theParseOracle, fn,
					  jmpTableBaseAddr);
   else {
      // MJB TODO - following is a kludge until we can do the backwards 
      //            slice for regIndirect jmp
      theParseOracle->processInterproceduralJmpToFixedAddr(bb_id, instr,
							   false, NULL, 
							   stumbleOntoAddr,
							   pathFromEntryBB);
      return false;
   }
}

bool x86_controlFlow::
handleJmpInstr(unsigned bb_id,
	       x86_instr *instr,
	       kptr_t &instrAddr,
	       x86_instr::x86_cfi *cfi,
	       const simplePath &pathFromEntryBB,
	       kptr_t stumbleOntoAddr,
	       const kmem *km,
	       parseOracle *theParseOracle)
{
   const function_t *fn = theParseOracle->getFn();

   // Check jump type
   if(cfi->destination != x86_instr::x86_cfi::pcRelative) {
      // What non pcrelative types of jmp's do we have on x86?
      // answer: absolute & indirect through reg/mem
      // so types we could have are absolute, memIndirect, & regRelative
      if(cfi->destination == x86_instr::x86_cfi::regRelative ||
         cfi->destination == x86_instr::x86_cfi::memIndirect)
	 return handleJmpTableJmpInstr(bb_id, instr, instrAddr, cfi, 
				       pathFromEntryBB, stumbleOntoAddr,
				       km, theParseOracle, fn);
      else {
	 if(cfi->destination == instr_t::controlFlowInfo::absolute) {
	   if(cfi->farSegment == 0x0010) { // then seg is kernel code seg
	      // absolute jump to offset in kernel code seg, adjust
	      // offset_nbytes for processing as normal interprocedural jump 
	     cfi->offset_nbytes = cfi->offset_nbytes - (instrAddr + instr->getNumBytes());
	   }
	   else {
	      fprintf(stderr, "Found absolute jump to non-kernel segment @ 0x%08lx to seg(0x%04x):offset(%08lx)\n", instrAddr, cfi->farSegment, cfi->offset_nbytes);
              theParseOracle->processDoneOrRetry(bb_id, instr, instrAddr); 
                 // for lack of a better method to call, does what we want
              return false;
	   }
	 }
	 else {
	    assert(!"x86_controlFlow::handleJmpInstr: jmp type unsupported");
	    return false;
	 }
      }
   }

   const kptr_t destAddr = instrAddr + instr->getNumBytes() + cfi->offset_nbytes;
   if(!fn->containsAddr(destAddr)) {
      // interprocedural jmp
      
      /* KLUDGE: on x86/Linux, the kernel's schedule function contains
         a push/jmp sequence that is used to call '__switch_to' and have
         the thread/task continue execution at the pushed address (which is
         the saved EIP of the thread/task being switched in, and normally
         is equal to the address of the instruction following the jmp). 
         Thus, we don't want to identify this particular interprocedural 
         jump as an exit, especially since the jmp instruction is also the
         point where we insert the context-switch-in handling code.
      */
      if(fn->matchesName(pdstring("schedule"))) {
         delete cfi;
         theParseOracle->getMutableFn()->bumpUpBlockSizeBy1Insn(bb_id, instr);
         // Since returning true, adjust instrAddr as appropriate:
         instrAddr += instr->getNumBytes();
         return true; // keep parsing basic block
      }

      theParseOracle->processInterproceduralJmpToFixedAddr(bb_id,
                                                           (instr_t*)instr,
                                                           false,
                                                           (instr_t*)NULL,
                                                           destAddr,
                                                           pathFromEntryBB);
							  
   }
   else {
      // intraprocedural jmp
      regset_t *dummy_set = regset_t::getRegSet(regset_t::empty);
      simplePathSlice dummy_slice(*fn, pathFromEntryBB, *dummy_set, false, 0);
      theParseOracle->processIntraproceduralJmpToFixedAddr(bb_id,
                                                           (instr_t*)instr,
                                                           instrAddr,
                                                           false,
                                                           (instr_t*)NULL,
                                                           destAddr,
                                                           dummy_slice,
                                                           pathFromEntryBB);
      delete dummy_set;

   }
   delete cfi;
   return false; // done with this basic block
}

bool 
x86_controlFlow::handleRetInstr(unsigned bb_id, 
				x86_instr *instr,
				kptr_t &instrAddr, 
				parseOracle *theParseOracle) {
   theParseOracle->processReturn(bb_id, instr, instrAddr, false, NULL);
   return false; // bb is finished; don't fall through
}

bool 
x86_controlFlow::handleIntInstr(unsigned bb_id, 
				x86_instr *instr,
				kptr_t &instrAddr, 
				parseOracle *theParseOracle) {
   theParseOracle->processDoneOrRetry(bb_id, instr, instrAddr); 
      // for lack of a better method to call, does basically what we want
   return false; // bb is finished; don't fall through
}

bool
x86_controlFlow::handleControlFlowInstr(
                            bbid_t bb_id,
			    x86_instr *instr,
			    kptr_t &instrAddr,
			      // iff returning true, we modify this
			    x86_instr::x86_cfi *cfi,
			    const simplePath &pathFromEntryBB,
			      // doesn't yet include curr bb
			    kptr_t stumbleOntoAddr,
			    const kmem *km,
			    parseOracle *theParseOracle)
{
   if(cfi->fields.isCall)
      return handleCallInstr(bb_id, instr, instrAddr, cfi, 
			     pathFromEntryBB, stumbleOntoAddr,
			     km, theParseOracle);

   else if (cfi->fields.isBranch && cfi->fields.isConditional)
      return handleConditionalBranchInstr(bb_id,
					  instr,
					  instrAddr,
					  cfi,
					  pathFromEntryBB,
					  stumbleOntoAddr,
					  km,
					  theParseOracle);

   else if (cfi->fields.isBranch && cfi->fields.isAlways)
      return handleJmpInstr(bb_id,
			    instr,
			    instrAddr,
			    cfi,
			    pathFromEntryBB,
			    stumbleOntoAddr,
			    km,
			    theParseOracle);

   else if (cfi->fields.isRet) {
      delete cfi;
      return handleRetInstr(bb_id, instr, instrAddr, theParseOracle);
   }
   else if (cfi->x86_fields.isInt || cfi->x86_fields.isUd2) {
      delete cfi;
      return handleIntInstr(bb_id, instr, instrAddr, theParseOracle);
   } 
   // we shouldn't get here, right???
   fprintf(stderr, "ERROR - unhandled control flow instr @%08lx", instrAddr);
   fprintf(stderr, " - instr bytes: ");
   const char *bytes = instr->getRawBytes();
   for(int j=0; j<instr->getNumBytes(); j++)
      fprintf(stderr, "%2x ", (unsigned char)bytes[j]);
   fprintf(stderr, "\n");
   assert(!"unhandled control flow case\n");
   return false;
}


