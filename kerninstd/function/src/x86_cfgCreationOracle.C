// x86_cfgCreationOracle.C

#include "x86_cfgCreationOracle.h"
#include "simplePath.h"
#include "moduleMgr.h"
#include "kernelDriver.h"
#include "fillin.h"

extern moduleMgr *global_moduleMgr;
extern kernelDriver *global_kernelDriver;

static bool
handleConditionalBranchInstrTakenCase(bbid_t bb_id,
                                      kptr_t destAddr,
				      const simplePath &pathFromEntryBB,
				         // doesn't include this
                                      const kmem */*km*/,
                                      parseOracle *theParseOracle) 
{
   // Returns true iff we split ourselves, in which case the caller should
   // process the fall-through case, if any, on behalf of getsucc(0) instead 
   // of this.

   // If destAddr isn't in any bb, then recursively create it.
   // Else, if destAddr is the start of a bb, then use it.
   // Else, if it's in the middle of an existing bb, then split it (being 
   // careful when splitting ourselves).

   // Note: we can be assured that this branch is intraprocedural; whoever 
   // called this routine was certain of this before calling.  Therefore, 
   // the following asserts may be applied:
   function_t *fn = theParseOracle->getMutableFn();
   assert(fn->getOrigCode().enclosesAddr(destAddr));
   assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() == 0);

   bbid_t succ_bb_id = fn->searchForBB(destAddr);
   if (succ_bb_id == (bbid_t)-1) {
      // destAddr doesn't lie within an existing basic block.  Recurse.
      basicblock_t *newbb = basicblock_t::getBasicBlock(destAddr, bb_id);
      succ_bb_id = fn->addBlock_new(newbb);

      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id);
         // won't be a dup since it was just created

      fn->parse(succ_bb_id,
		simplePath(pathFromEntryBB, bb_id), // pathFromEntryBB + bb_id
		theParseOracle);

      return false;
   }

   if (fn->getBasicBlockFromId(succ_bb_id)->getStartAddr() == destAddr) {
      // destAddr is the beginning of an existing bb; use it.
      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id); 
         // won't be a dup since numSuccessors() was 0
      if (!fn->addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
	 assert(false);

      return false;
   }

   assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() == 0);

   // destAddr lies in the middle of an existing bb.  Split that basic block
   // in two.  Be careful when splitting ourselves, it's a tricky case
   bbid_t bb_to_split_id = succ_bb_id;
   const bool splitSelf = (bb_to_split_id == bb_id);
   succ_bb_id = fn->splitBasicBlock(bb_to_split_id, destAddr);

   // NOTE: we don't bother to update pathFromEntryBB since it's only needed 
   // if we're gonna recurse ... which doesn't happen here.

   // Finally, do what we came here to do:
   if (!splitSelf) {
      // If we didn't split ourselves, then we should have no successors
      assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() == 0);
      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id);
         // won't be a dup since numSuccessors() was 0
      if (!fn->addPredecessorToBlockByIdUnlessDup(succ_bb_id, bb_id))
	 assert(false);
   }
   else {
      // If we split ourselves, then we should now have 1 successor: the lower
      // half of the split block
      const basicblock_t *bb = fn->getBasicBlockFromId(bb_id);
      assert(bb->numSuccessors() == 1);

      const bbid_t succ0bb_id = bb->getsucc(0);
      const basicblock_t *succ0bb = fn->getBasicBlockFromId(succ0bb_id);
      
      assert(succ0bb->numPredecessors() == 1);
      assert(succ0bb->getpred(0) == bb_id);

      // Add a successor representing the taken branch. Since we have split 
      // ourselves, the branch should be added to the successors of getsucc(0);
      if (fn->addSuccessorToBlockByIdUnlessDup(succ0bb_id, succ0bb_id))
         if (!fn->addPredecessorToBlockByIdUnlessDup(succ0bb_id, succ0bb_id))
            assert(false);
   }

   // NOTE: If splitSelf, then we want to process the fall-through case on 
   // behalf of 'succ', instead of curr bb.
   return splitSelf;
}

static void
handleConditionalBranchInstrFallThroughCase(bbid_t bb_id,
                                            kptr_t fallThroughInsnAddr,
					    const simplePath &pathFromEntryBB,
					      // curr bb has not yet been added
                                            const kmem */*km*/,
                                            parseOracle *theParseOracle,
                                            bbParsingStats &theBBParsingStats)
{
   // If the dest block is nextKnownFnStartAddr, then do nothing (much).
   // Else, if the dest block lies at the start of an existing bb, then use it.
   // Else, if it lies in the middle on an existing bb, then assert fail (never
   // happens with the fall-through case).

   function_t *fn = theParseOracle->getMutableFn();
   assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() < 2);

   if (!fn->containsAddr(fallThroughInsnAddr)) {
      // The fall-through case stumbles onto another function, noted by adding
      // "ExitBBId" as a successor instead of parsing the fall-thru block.
      fn->addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

      fn->markBlockAsHavingInterProcFallThru(bb_id);

      theBBParsingStats.numCondBranchFallThrusThatDoInterProceduralStumble++;
      
      return;
   }

   bbid_t succ_id = fn->searchForBB(fallThroughInsnAddr);

   if (succ_id == (bbid_t)-1) {
      // fall-thru addr isn't within any existing basic block, so recursively
      // create it.
      basicblock_t *newbb = basicblock_t::getBasicBlock(fallThroughInsnAddr, bb_id);
      succ_id = fn->addBlock_new(newbb);

      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_id);
         // won't be a duplicate if it didn't exist anywhere until now

      fn->parse(succ_id,
		simplePath(pathFromEntryBB, bb_id), // pathFromEntryBB + bb_id
		theParseOracle);
   }
   else {
      const basicblock_t *fallThruBlock = fn->getBasicBlockFromId(succ_id);
   
      if (fallThruBlock->getStartAddr() == fallThroughInsnAddr) {
         // Dest block lies at the start of an existing bb; use it.
         // Don't allow duplicates in 'successors':
         if (fn->getBasicBlockFromId(bb_id)->numSuccessors() == 0 ||
             fn->getBasicBlockFromId(bb_id)->getsucc(0) != succ_id) { 
	    // disallow duplicates
            fn->addSuccessorToBlockByIdNoDup(bb_id, succ_id);
            if (!fn->addPredecessorToBlockByIdUnlessDup(succ_id, bb_id))
               assert(false);
         }
      }
      else {
         // dest block lies within an existing bb without being the start of 
	 // a bb; can't happen with the fall-through case.  
	 // Proof: Assume fall through address is in the middle of an existing
	 // basic block.  This means that there are insns before it in that 
	 // basic block, because one does not fall through without a branch 
	 // instruction preceding it.  But that's strange, since we've
         // just been working on the instructions before the fall-through 
	 // case; and they're in the current bb, which won't yet show up in 
	 // a searchForBB() yet.
         cout << "Sorry, fall thru to " << addr2hex(fallThroughInsnAddr)
              << " lies within, but not at the beginning of, a basic block "
              << " whose startAddr is " << addr2hex(fallThruBlock->getStartAddr())
              << endl;
         assert(false);
      }
   }

   assert(fn->getBasicBlockFromId(bb_id)->numSuccessors() > 0);
}

void x86_cfgCreationOracle::
processCallRecursion(bbid_t bb_id, kptr_t calleeAddr,
                     instr_t *callInstr,
                     kptr_t instrAddr,
                     bool /*delaySlotInSameBlock*/,
                     instr_t */*dsInsn*/,
                     const simplePath &pathFromEntryBB
                        // doesn't include this block yet
                     ) 
{
   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);

   // Recursively parse the destination of the call (will probably do 
   // nothing, since presumably, the destination is the start of the fn, 
   // an already-parsed block).
   const bool splitSelf =
      handleConditionalBranchInstrTakenCase(bb_id,
                                            calleeAddr,
                                            pathFromEntryBB,
                                               // doesn't include this block yet
                                            km,
                                            this);

   // Recursively parse the fall-thru of the call
   
   bbid_t fallThruOnBehalfOf = splitSelf ? 
      fn.getBasicBlockFromId(bb_id)->getsucc(0) : bb_id;
   
   handleConditionalBranchInstrFallThroughCase(fallThruOnBehalfOf,
                                               instrAddr + callInstr->getNumBytes(), 
                                                  // fallthru addr
                                               splitSelf ?
                                                  simplePath(pathFromEntryBB, bb_id) :
                                                     // pathFromEntryBB + bb_id
                                                  pathFromEntryBB,
                                               km,
                                               this,
                                               theBBParsingStats);

   theBBParsingStats.numCallRecursion++;
}


#if 0
// MJB NOTE: The following doesn't seem to work since only those functions
//           that have been parsed have their origCode filled in, so
//           trying to find the fn that holds the destAddr almost always
//           fails

static bool handlePossibleOutOfLineCode(bbid_t bb_id, kptr_t destAddr,
                                        const simplePath &pathFromEntryBB,
                                        parseOracle *theParseOracle)
{
   // We have identified an interprocedural conditional branch, and think
   // it may be to some out-of-line basic blocks. We'll try to parse
   // the destination and see if it ends up unconditionally branching 
   // back to this function

   const function_t *dest_fn = NULL;
   pdstring this_mod_name;
   const loadedModule *this_mod;

   // This sucks, there's got to be a better way. All we really need
   // is the current module name and we eliminate the search through
   // all modules; just have to search the functions of a single module
   moduleMgr::const_iterator mod_iter = global_moduleMgr->begin();
   moduleMgr::const_iterator mod_end_iter = global_moduleMgr->end();
   for(; mod_iter != mod_end_iter; mod_iter++) {
      this_mod = mod_iter.currval();
      this_mod_name = mod_iter.currkey();
      //      cerr << "MJB DEBUG: current mod is " << this_mod_name << endl;
      loadedModule::const_iterator fn_iter = this_mod->begin();
      loadedModule::const_iterator fn_end_iter = this_mod->end();
      for(; fn_iter != fn_end_iter; fn_iter++) {
         // cerr << "         : current fn is " 
         //   << addr2hex((*fn_iter)->getEntryAddr())
         //   << ", with chunks ";
         // const fnCode &fn_code = (*fn_iter)->getOrigCode();
//          fnCode::const_iterator fn_code_chk_iter = fn_code.begin();
//          fnCode::const_iterator fn_code_chk_end = fn_code.end();
//          while(fn_code_chk_iter != fn_code_chk_end) {
//             cerr << "(" << addr2hex(fn_code_chk_iter->startAddr) 
//                  << ", " <<  fn_code_chk_iter->numBytes() << ") ";
//             fn_code_chk_iter++;
//          }
//          cerr << endl;
         
         if((*fn_iter)->containsAddr(destAddr)) {
            dest_fn = *fn_iter;
            break;
         }
      }
      if(dest_fn != NULL)
         break;
   }

   if(dest_fn == NULL) {
      cout << "WARNING: could not find any function in any module " 
           << "that contains the address " << addr2hex(destAddr)
           << ", the destination of an interprocedural conditional branch\n";
      return false;
   }

   function_t *fn = theParseOracle->getMutableFn();
   bbid_t succ_bb_id = dest_fn->searchForBB(destAddr);
   if (succ_bb_id == (bbid_t)-1) {
      // destAddr doesn't lie within an existing basic block, which would
      // make sense if this is an out-of-line block for our function
      basicblock_t *newbb = basicblock_t::getBasicBlock(destAddr, bb_id);
      succ_bb_id = fn->addBlock_new(newbb);

      fn->addSuccessorToBlockByIdNoDup(bb_id, succ_bb_id);
         // won't be a dup since it was just created

      // Before parsing the new bb, we need to add it's code to this fn's
      // origCode. We'll assume (hopefully correctly) that the out-of-line
      // code doesn't jump back to some addr in dest_fn before destAddr,
      // so we'll grab the chunk out of dest_fn's origCode from destAddr
      // till the end of the chunk
      const fnCode &dest_fn_code = dest_fn->getOrigCode();
      fnCode::const_iterator the_chunk = dest_fn_code.findChunk(destAddr, 
                                                                false);
      assert(the_chunk != dest_fn_code.end());
      kptr_t chk_start_addr = the_chunk->startAddr;
      unsigned chk_num_bytes = the_chunk->numBytes();
      insnVec_t *filledin_insns = insnVec_t::getInsnVec();
      fillin_kernel(filledin_insns,
                    destAddr,
                    chk_start_addr + chk_num_bytes,
                    *global_kernelDriver);
      fnCode &fn_code = const_cast<fnCode&>(fn->getOrigCode());
      fn_code.addChunk(destAddr, filledin_insns, true);

      // Now parse the dest bb
      try {
         fn->parse(succ_bb_id, simplePath(pathFromEntryBB, bb_id),
                   theParseOracle);
      }
      // TODO: Should probably remove the chunk just added on exception
      catch (function_t::parsefailed) {
         return false;
      }
      catch (...) {
         cout << "caught an unexpected exception! (...) in x86_cfgCreationOracle.C//handlePossibleOutOfLineCode()\n";
         return false;
      }

      return true;
   }
   else {
      // destAddr is the start of some parsed basic block in dest_fn, so
      // this is truly an interprocedural branch
      return false;
   }
}
#endif

void x86_cfgCreationOracle::
processInterproceduralBranch(bbid_t bb_id, kptr_t destAddr,
                             instr_t */*i*/,
                             const controlFlowInfo_t &cfi,
                             bool branchAlways,
                             kptr_t instrAddr,
                             bool /*delaySlotInThisBlock*/,
                             instr_t */*delayInstr*/) 
{
   if(branchAlways) {
      // Add "interProcBranchExitBBId" as a successor to this basic block,
      // and add to interProcJumpsAndCalls[].
      fn.addSuccessorToBlockByIdNoDup(bb_id, 
                                      basicblock_t::interProcBranchExitBBId);
      interProcJumpsAndCalls[destAddr] = true; // key is what matters
      theBBParsingStats.numInterProcBranchAlways++;
      return;
   }

#if 0
   bool found_ool_code = handlePossibleOutOfLineCode(bb_id, destAddr,
                                                     pathFromEntryBB, this);

   if(found_ool_code) {
      theBBParsingStats.numIntraProcCondBranches++;
   }
   else {
      fn.addSuccessorToBlockByIdNoDup(bb_id, 
                                      basicblock_t::interProcBranchExitBBId);
      interProcJumpsAndCalls[destAddr] = true; // key is what matters
      theBBParsingStats.numInterProcCondBranches++;
   }
                                                        
#else
   // Conditional interprocedural branch - check for synchcode special case.

   // If we're lucky, the synch code is within the original function
   // bounds as determined from symbol info, and thus the jcc to the 
   // synch code is treated as an intraprocedural branch and gets
   // parsed. However, if we're in this function, then this was not the 
   // case.

   bool is_synchcode_branch = false;

   /* Here is what various synchronization methods look like on x86/Linux2.4:
      
      NOTE: the '[lock]' notation indicates that on an SMP kernel the lock
            prefix will be present, while on a UP kernel it will not 


      SPINLOCKS:
        
         <addr x>:          [lock] dec [spinlock_addr]
                            js <spin_code>
                            ...

         <addr spin_code>:  cmp [spinlock_addr], 0
                            nop
                            jle <spin_code>
                            jmp <x>

      SEMAPHORES:

                    <a>:   [lock] dec <sem addr>
                    <b>:   js <down_code>
                    <c>:   ...

            <down_code>:   call 'kernel/__down_failed'
                           jmp <c>


                    <x>:   [lock] inc <sem addr>
                    <y>:   jle <up_code>
                    <z>:   ...

              <up_code>:   call 'kernel/__up_wakeup'
                           jmp <z>

      OTHER VARIATIONS:

      It's possible that the instruction preceeding the jcc is a
      'cmp r/m32, 0' or 'test %reg, %reg' that immediately follows some
      atomic update of r/m32 or %reg.

      A '[lock] xadd' may be substituted for a semaphore increment op.

      Some forms of spinlocks seem to use 32-bit (as opposed to 8-bit)
      values, and do add/sub of 0x01000000 instead of inc/dec.

   */

   // Check for 'js', 'jle', 'jnz'
   if((cfi.condition == (unsigned)x86_instr::Sign) ||
      (cfi.condition == (unsigned)x86_instr::LessThanEqual) ||
      (cfi.condition == (unsigned)x86_instr::NotEqual)) {

      // Get previous instruction and check for known synch methods
      const fnCode &fn_code = fn.getOrigCode();
      fnCode::const_iterator chk_iter = fn_code.findChunk(instrAddr, false);
      fnCode::codeChunk::const_iterator insn_iter = chk_iter->begin();
      instr_t *this_insn = chk_iter->get1Insn(instrAddr);
      while((*insn_iter != this_insn) // NOTE: ptr comparison, not instr 
            && (insn_iter != chk_iter->end())) {
         insn_iter++;
      }
      if(insn_iter == chk_iter->end())
         assert(!"x86_cfgCreationOracle::processInterproceduralBranch() - didn't find current insn\n");
      instr_t *prev_insn = *(insn_iter-1);
      unsigned prev_numbytes = prev_insn->getNumBytes();
      kptr_t prev_insn_addr = instrAddr - prev_numbytes;
      const char *insn_bytes = ((x86_instr*)prev_insn)->getRawBytes();

      if((prev_numbytes == 4) &&
         (insn_bytes[0] == (char)0x83) &&
         ((insn_bytes[1] & (char)0x38) == (char)0x38) &&
         (insn_bytes[3] == (char)0x0) && 
         (cfi.condition == (unsigned)x86_instr::NotEqual))
         // cmp r/m32, 0; jnz
         is_synchcode_branch = true;

      else if((prev_numbytes == 2) && 
              (insn_bytes[0] == (char)0x85) && 
              ((insn_bytes[1] & (char)0xC0) == (char)0xC0) && 
              (cfi.condition == (unsigned)x86_instr::NotEqual))
         // test reg, reg; jnz
         is_synchcode_branch = true;

      else { 
         if(insn_bytes[0] == (char)0xF0) {
            // lock prefixed
            insn_bytes = &insn_bytes[1];
            prev_numbytes--;
         }
         switch(insn_bytes[0]) {
         case 0xFE: // inc/dec
         case 0xFF:
            is_synchcode_branch = true;
            break;
         case 0x81: 
            if(prev_numbytes == 6) {
               if((insn_bytes[1] & (char)0x38) == (char)0x28) {
                  const unsigned *arg = (const unsigned*)(&insn_bytes[2]);
                  if(*arg == (unsigned)0x01000000)
                     // sub r/m32, imm32(==0x01000000)
                     is_synchcode_branch = true;
               }
            }
            break;
         case 0x83:
            if(prev_numbytes == 3) {
               if((insn_bytes[1] & (char)0x28) == (char)0x28) {
                  if(insn_bytes[2] == (char)0x1)
                     // sub r/m32, imm8(==1)
                     is_synchcode_branch = true;
               }
            }
            break;
         case 0x0F:
            if(insn_bytes[1] == (char)0xC1)
               // xadd
               is_synchcode_branch = true;
            break;
         default:
            break;
         }
      }
   }

   if(! is_synchcode_branch) {
      fn.addSuccessorToBlockByIdNoDup(bb_id, 
                                      basicblock_t::interProcBranchExitBBId);
      interProcJumpsAndCalls[destAddr] = true; // key is what matters
      theBBParsingStats.numInterProcCondBranches++;
   }
   else
      theBBParsingStats.numIntraProcCondBranches++;
#endif
}
   
void x86_cfgCreationOracle::
processCondBranch(bbid_t bb_id,
		  bool /* branchAlways */, 
		  bool /* branchNever */,
		  instr_t *i,
		  controlFlowInfo_t *cfi,
		  kptr_t instrAddr,
		  bool /* hasDelaySlotInsn */,
		  bool /* delaySlotInThisBlock */,
		  instr_t * /* delaySlotInsn */,
		  kptr_t fallThroughAddr,
		  kptr_t destAddr,
		  const simplePath &pathFromEntryBB)
{
   fn.bumpUpBlockSizeBy1Insn(bb_id, i);
   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);

   bool definitelySplit = false;

   // Handle the taken case.  It may or may not be interprocedural.
   if(!fn.containsAddr(destAddr)) {
      // interprocedural branch
      this->processInterproceduralBranch(bb_id,
					 destAddr, i, *cfi,
					 false, // branchAlways
					 instrAddr,
					 false, // delaySlotInThisBlock
					 NULL); // delaySlotInsn*
   }
   else {
      // intraprocedural branch
      theBBParsingStats.numIntraProcCondBranches++;
      definitelySplit = handleConditionalBranchInstrTakenCase(bb_id,
							      destAddr,
							      pathFromEntryBB,
							      km,
							      this);
   }

   // Recurse on the fall-through case, if any.  If curr bb has been split 
   // anytime during processing of the taken case, then the fall-through case 
   // must be processed on behalf of bottom half
   
   bbid_t parentIdOfFallThruBlock = bb_id;
   if(definitelySplit) {
     // bottom half bbid will be the one that contains the branch
     parentIdOfFallThruBlock = fn.searchForBB(instrAddr);
   }
   
   assert(parentIdOfFallThruBlock != (bbid_t)-1);
         
   handleConditionalBranchInstrFallThroughCase(
          parentIdOfFallThruBlock,
	  fallThroughAddr,
	  parentIdOfFallThruBlock != bb_id ?
                 simplePath(pathFromEntryBB, bb_id) : pathFromEntryBB,
	  km,
	  this,
	  theBBParsingStats);
   
}

void x86_cfgCreationOracle::
processNonTailOptimizedTrueCall(bbid_t bb_id, 
                                instr_t *callInstr,
                                kptr_t /*instrAddr*/,
                                kptr_t destAddr,
                                bool /*delaySlotInThisBlock*/,
                                instr_t */*delayInstr*/) 
{
   fn.bumpUpBlockSizeBy1Insn(bb_id, callInstr);

   assert(instr_t::instructionHasValidAlignment(destAddr));
   interProcJumpsAndCalls[destAddr] = true;

   theBBParsingStats.numTrueCalls_notTail++;
}
   
void x86_cfgCreationOracle::
processNonTailOptimizedJmplCall(bbid_t /*bb_id*/, 
                                instr_t */*callInstr*/,
                                kptr_t /*instrAddr*/,
                                const bool /*delaySlotInSameBlock*/,
                                instr_t */*delaySlotInsn*/) 
{
   assert(!"x86_cfgCreationOracle::processNonTailOptimizedJmplCall() should never be called\n");
}

void x86_cfgCreationOracle::
processIndirectTailCall(bbid_t /*bb_id*/, 
                        instr_t */*jmpInsn*/,
                        kptr_t /*insnAddr*/,
                        bool /*delaySlotInSameBlock*/,
                        instr_t */*dsInsn*/) const 
{
   assert(!"x86_cfgCreationOracle::processIndirectTailCall() nyi\n");
}

void x86_cfgCreationOracle::
processIntraproceduralJmpToFixedAddr(bbid_t bb_id,
				     instr_t *instr,
				     kptr_t /*jmpInsnAddr*/,
				     bool /*delaySlotInSameBlock*/,
				     instr_t */*dsInsn*/,
				     kptr_t destAddr,
				     const simplePathSlice_t &/*jmpRegSlice*/,
				     const simplePath &pathFromEntryBB)
{
   fn.bumpUpBlockSizeBy1Insn(bb_id, instr);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
      
   (void)handleConditionalBranchInstrTakenCase(bb_id, destAddr,
                                               pathFromEntryBB,
                                               km,
                                               this);
      // void the result: we don't care if we split ourself (this basic block),
      // since we're not going to process any fall-through case.

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 1);

   theBBParsingStats.numJumpToRegFixedAddr_IntraProc++;
}

void x86_cfgCreationOracle::
processInterproceduralJmpToFixedAddr(bbid_t bb_id,
				     instr_t *instr,
				     bool /*delaySlotInSameBlock*/,
				     instr_t */*dsInsn*/,
				     kptr_t destAddr,
				     const simplePath &/*pathFromEntryBB*/)
{
   fn.bumpUpBlockSizeBy1Insn(bb_id, instr);

   assert(fn.getBasicBlockFromId(bb_id)->numSuccessors() == 0);
   fn.addSuccessorToBlockByIdNoDup(bb_id, basicblock_t::xExitBBId);

   interProcJumpsAndCalls[destAddr] = true; // value is a dummy; key is what matters

   theBBParsingStats.numJumpToRegFixedAddr_InterProc++;
}

// ---------- Non-Loading Backwards Jump Table: ----------
void x86_cfgCreationOracle::
processNonLoadingBackwardsJumpTable(bbid_t /*bb_id*/,
                                    kptr_t /*jumpTableStartAddr*/,
                                    kptr_t /*jumpTableEndAddr*/,
                                    instr_t */*jmpInsn*/,
                                    bool /*delaySlotInThisBlock*/,
                                    instr_t */*delaySlotInsn*/,
                                    const simplePathSlice_t &/*justPastEndOfJmpTableSlice*/,
                                    const simplePath &) 
{
   assert(!"x86_cfgCreationOracle::processNonLoadingBackwardsJumpTable() nyi\n");
}

// ---------- Offset Jump Table 32: ----------
void x86_cfgCreationOracle::
processOffsetJumpTable32WithTentativeBounds(bbid_t /*bb_id*/,
                                            instr_t */*instr*/,
                                            kptr_t /*jmpInsnAddr*/,
                                            bool /*delaySlotInThisBlock*/,
                                            instr_t */*delaySlotInsn*/,
                                            kptr_t /*jumpTableStartAddr*/,
                                            const pdvector<kptr_t> &,
                                               // jump table block addrs
                                               // tentative!! May be too many!!
                                            const simplePathSlice_t &/*theSlice*/,
                                            const simplePath &/*pathFromEntryBB*/) 
{
   assert(!"x86_cfgCreationOracle::processOffsetJumpTable32WithTentativeBounds() nyi\n");
}
