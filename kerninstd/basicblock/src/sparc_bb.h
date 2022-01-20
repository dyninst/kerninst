#ifndef _SPARC_BB_H_
#define _SPARC_BB_H_

#include <inttypes.h> // uint32_t

#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"

#include "sparc_instr.h"
#include "fnCode.h"
#include "sparcTraits.h"

// fwd decls to avoid recursive #includes:
class basicblock_t;
class sparc_reg_set;
class monotone_bitwise_dataflow_fn;

class sparc_bb {
public:
   typedef fnCode fnCode_t;
   typedef uint16_t bbid_t; //same as in basicblock.h

   static pdvector<bool> backwardsSlice(kptr_t succ_bb_startAddr,
					regset_t &regsToSlice,
					bool includeControlDependencies,
					const fnCode_t &theFnCode,
					unsigned numInsnBytesToSkipAtEnd,
					unsigned bbNumInsnBytes,
					kptr_t bbStartAddr,
					kptr_t bbEndAddrPlus1) {
     // numInsnBytesToSkipAtEnd: the number of instruction bytes out of this basic
     // block, counting from the bottom of the basic block, that should be ignored
     // when doing the slice.  Of course 0 means skip nothing and consider the
     // whole basic block when slicing.
     assert(numInsnBytesToSkipAtEnd <= bbNumInsnBytes);

     sparc_reg_set &regs2slice = (sparc_reg_set&)regsToSlice;

     // In the future, we may allow slicing by memory locations, not just regs.

     // succ_bb is needed to determine whether the delay slot insn of 'this'
     // (if any) should be included.

     // If this bb ends in a DCTI with a delay slot insn that gets annulled if succ_bb
     // is next executed, then the delay slot insn should be removed from the slice (by
     // changing 'numInsnBytesToSkipAtEnd').
     const kptr_t lastInsnAddrInBlock = bbEndAddrPlus1 - 4;
     // getEndAddr() returns end of last insn...add 1 to get start of next insn...
     // subtract 4 to get back to beginning of last insn.

     // Since a basic block never crosses a chunk boundary, we can get a chunk now
     // and use its quick version of get1Insn() rather than function's get1Insn().
     fnCode_t::const_iterator chunk_iter = theFnCode.findChunk(bbStartAddr, false);
     assert(chunk_iter != theFnCode.end());
     const fnCode_t::codeChunk &theCodeChunk = *chunk_iter;
   
     instr_t *last_insn = theCodeChunk.get1Insn(lastInsnAddrInBlock);

     const unsigned bbnumInsns =
       theCodeChunk.calcNumInsnsWithinBasicBlock(bbStartAddr,
						 bbEndAddrPlus1);

     if (succ_bb_startAddr != -1U && bbnumInsns >= 2 &&
	 numInsnBytesToSkipAtEnd == 0) {
        instr_t *second_to_last_insn = theCodeChunk.get1Insn(lastInsnAddrInBlock - 4);
      
       sparc_instr::sparc_cfi cfi;
       second_to_last_insn->getControlFlowInfo(&cfi);

       if (cfi.delaySlot != instr_t::dsAlways &&
	   cfi.delaySlot != instr_t::dsNone && 
	   (cfi.fields.isBranch && cfi.fields.isConditional)) {
	 // There's a delay slot, and a chance that it's skipped
	 bool skipIt = cfi.delaySlot == instr_t::dsNever; // so far
	 if (!skipIt) {
	   assert(cfi.delaySlot == instr_t::dsWhenTaken);
	   kptr_t branchInsnAddr = bbStartAddr + 4*(bbnumInsns-2);
	   long offset_bytes = succ_bb_startAddr - branchInsnAddr;

	   assert(cfi.destination == sparc_instr::controlFlowInfo::pcRelative);
	   bool branchWasTaken = (offset_bytes==cfi.offset_nbytes);
	   if (!branchWasTaken)
	     // if it wasn't taken, then it should be a fall-through
	     assert(succ_bb_startAddr == branchInsnAddr +
		    cfi.delaySlot == instr_t::dsNone ? 4 : 8);

	   // cfi.delaySlot == dsWhenTaken.  Hence, the branch insn is annulled.
	   // Hence, the delay slot is annulled if NOT taken.
	   skipIt = !branchWasTaken;
	 }

	 if (skipIt)
	   numInsnBytesToSkipAtEnd += last_insn->getNumBytes();
       }
     }

     // initialize the result: no instructions are in the slice
     pdvector<bool> result(bbnumInsns, false);

     // add the PC register to regsToSlice if bb ends in a *conditional* branch (unless
     // it falls outside of the range of insns to slice thanks to numInsnBytesToSkipAtEnd)
     if (includeControlDependencies && numInsnBytesToSkipAtEnd == 0)
       factorInControlDependencyInRegsToSlice(regsToSlice, theCodeChunk, 
					      bbStartAddr, bbEndAddrPlus1);

     unsigned lcv = bbnumInsns - 1 - (numInsnBytesToSkipAtEnd / 4);

     unsigned byteoffset = bbNumInsnBytes - 4 - numInsnBytesToSkipAtEnd;

     while (true) {
       if (regsToSlice.isempty())
	 return result;

       assert(byteoffset < bbNumInsnBytes);
       instr_t *instr = theCodeChunk.get1Insn(bbStartAddr + byteoffset);

       sparc_instr::sparc_ru ru;
       instr->getRegisterUsage(&ru);

       *((sparc_reg_set*)ru.definitelyUsedBeforeWritten) -= 
	 sparc_reg_set::regsThatIgnoreWrites;
       *((sparc_reg_set*)ru.maybeUsedBeforeWritten) -= 
	 sparc_reg_set::regsThatIgnoreWrites;

       const sparc_reg_set regsToSlice_definitelyWritten = 
	 *((sparc_reg_set*)ru.definitelyWritten) & regs2slice;
       const sparc_reg_set regsToSlice_maybeWritten = 
	 *((sparc_reg_set*)ru.maybeWritten) & regs2slice;

       if (!regsToSlice_definitelyWritten.isempty()) {
	 // The insn contributes to the value of some reg(s) we're slicing; thus, the
	 // insn is part of the slice.
	 result[lcv] = true;

	 // The insn kills the set regsToSlice_definitelyWritten (probably just a
	 // single entry in the set, by the way).  Since they're dead, no previous insn
         // can affect them insofar as the slice concerns.  Our register interest now
	 // shifts to registers _used_ by this insn.

	 regs2slice -= regsToSlice_definitelyWritten; // dead

	 regs2slice += *((sparc_reg_set*)ru.definitelyUsedBeforeWritten); 
	   // g0 has been removed -- good
	 regs2slice += *((sparc_reg_set*)ru.maybeUsedBeforeWritten); 
	   // g0 has been removed -- good
       }
       if (!regsToSlice_maybeWritten.isempty()) {
	 // This instr *might* contribute to the value of some regs we care about.
	 // Thus, add it to the slice.
	 result[lcv] = true;

	 // The regs written to cannot be declared dead.
	 ;

	 // Live regs which contribute to the slice: care about 'em:
	 regs2slice += *((sparc_reg_set*)ru.definitelyUsedBeforeWritten); 
	   // g0 has been removed -- good
	 regs2slice += *((sparc_reg_set*)ru.maybeUsedBeforeWritten); 
	   // g0 has been removed -- good
       }

       if (lcv == 0 && byteoffset == 0)
         break;
       else if (lcv == 0)
         assert(false); // should be both or neither
       else if (byteoffset == 0)
         assert(false); // should be both or neither

       byteoffset -= instr->getNumBytes();
       --lcv;
     }

     return result;
   }

   static void
   factorInControlDependencyInRegsToSlice(regset_t &regsToSlice,
                                          const fnCode_t::codeChunk &theCodeChunk,
					  kptr_t bbStartAddr,
					  kptr_t bbEndAddrPlus1) {
     // This routine adds the PC register to regsToSlice if the bb ends with a
     // *conditional* branch.

     const unsigned bb_num_insns =
       theCodeChunk.calcNumInsnsWithinBasicBlock(bbStartAddr,
						 bbEndAddrPlus1);
     if (bb_num_insns == 0)
       return;

   // the following doesn't leave room for an architecture whose conditional branches
   // only "sometimes" have a delay slot (depending on some bit in the instruction,
   // presumably).  I don't know of any (the annul bit of sparc notwithstanding:
   // whether set or not, a truly *conditional* branch [i.e. not always or never]
   // always has delay slot...whether it gets executed or not is another question
   // of course, but of no concern here)
   
     unsigned ndxOfPossibleBranchInsn = bb_num_insns - 1;
     if (ndxOfPossibleBranchInsn-- == 0)
       return;

     kptr_t addr = bbStartAddr;
     while (ndxOfPossibleBranchInsn--) {
       const sparc_instr *i = (const sparc_instr *)theCodeChunk.get1Insn(addr);
       addr += i->getNumBytes();
     }
   
     instr_t *possible_branch_insn = theCodeChunk.get1Insn(addr);

     sparc_instr::sparc_cfi cfi;
     possible_branch_insn->getControlFlowInfo(&cfi);

     if (!cfi.fields.controlFlowInstruction)
       return;

     if ((cfi.sparc_fields.isBicc || cfi.sparc_fields.isBPcc) &&
	 cfi.condition != sparc_instr::iccAlways &&
	 cfi.condition != sparc_instr::iccNever) {
       (sparc_reg_set&)regsToSlice += sparc_reg::reg_pc;
     }
     else if ((cfi.sparc_fields.isFBfcc || cfi.sparc_fields.isFBPfcc) &&
	      cfi.condition != sparc_instr::fccAlways &&
	      cfi.condition != sparc_instr::fccNever) {
       (sparc_reg_set&)regsToSlice += sparc_reg::reg_pc;
     }
     else if (cfi.sparc_fields.isBPr) {
       (sparc_reg_set&)regsToSlice += sparc_reg::reg_pc;
     }
   }

   // Sun's CC says templated methods can't be static.  I disagree.
   template <class calcEffectOfCall>
   static monotone_bitwise_dataflow_fn*
   doLiveRegAnalysis(kptr_t addrWhereToStart, // such as just past the end of the bb
                     kptr_t addrWhereToStop, // such as the startAddr of the bb
                     const fnCode_t &theFnCode,
                     const calcEffectOfCall &callOracle,
                     unsigned bbNumInsnBytes,
		     kptr_t bbStartAddr,
		     kptr_t bbEndAddrPlus1,
		     unsigned bbNumSuccs,
		     bbid_t bbSuccOf0) {
     // a REVERSE data flow analysis problem.  We work backwards among the instructions
     // in the basic block, checking which registers it uses and writes to.
     // The return value is a bitwise data flow function for this basic block, which
     // can be applied to a bitwise data flow vector describing live registers at
     // the *bottom* of this bb to obtain live registers at the *top* of this bb.
     //
     // Handling calls:
     //    When we see a call, we 'follow' the callee by invoking a method of 'callOracle'
     //    for object 'callback'; it returns a monotone_bitwise_dataflow_fn which we'll
     //    use as the effects of the call.
     //
     // Handling interprocedural branches/jumps (as opposed to calls):
     //    When the destination address is known, these are handled just like calls.
     //
     // Algorithm:
     // Since this is a backwards data flow problem, we must work temporally backwards,
     // handling the effects of the basic block.  To make the steps clear, let's write
     // out (in usual temporal direction) the effects of a typical instruction:
     // 1) read some src registers
     // 2) write a dest register
     //
     // Some instructions are DCTI's (delayed control transfer instructions, such as
     // call, branch, and jump on SPARC), so include the delay slot insn, then the callee:
     // 3) read some src regs    [in delay slot insn, if appropriate]
     // 4) write a dest register [in delay slot insn, if appropriate]
     // ...and process the destination of the DCTI (which is a branch, call, bpr, etc.)
     // 5) process effects of the [branch, call, bpr, ...]

   // To process an instruction in reverse temporal order, perform steps 1-5 in reverse
   // order.
   //
   // So, our algorithm:
   //
   // a) If curr insn is delay slot of a call or interprocedural branch/jump, process
   //    the call's destination.
   // b) Process writes of curr insn (works even if curr insn is in a delay slot)
   // c) Process reads of curr insn ("  "  "  ")
   // d) Go to previous insn and repeat until there are no more

   // How to process saves and restores?  Well, remember that we're going temporally
   // backwards...thus a save should be treated by folding up the window that we've
   // been using and moving back to the one in use before (temporally) the save.
   // Correspondingly, a restore should open up a new window, corresponding to the one
   // in use (temporally) before the restore.
   // So on restore, we push onto a stack of pre-save window contents, so that when
   // we later enocounter a save, we pop off of the stack, and use values there instead
   // of pass.

   // And yet a final wrinkle: some DCTI's have a delay slot insn whose execution is
   // conditional (sometimes annulled but not always).  Since such delay slot insns
   // are 'maybe-executed', we must implicitly treat them as merged with a NOP...thus
   // turning all definitely (used/written) registers into maybe (used/written).
   // (IMPORTANT NOTE: as you might imagine, it's basically impossible to simply make
   // this definitely-->maybe transition when the annulled delay insn is a save or
   // restore.  Fortunately, we can assert that this will never happen, thanks to a
   // recent change (12/30/1999) that will put such a delay insn in its own 1-insn basic
   // block, reachable only from the if-taken successor.  For less monumental insns in
   // the delay slot, the definnitely-->maybe transition may still happen.)

     monotone_bitwise_dataflow_fn *result = const_cast<monotone_bitwise_dataflow_fn *>(callOracle.getIdentityDataflowFn());

     assert(bbNumInsnBytes > 0); // This should always hold for a basic block

     assert(addrWhereToStop >= bbStartAddr);
     assert(addrWhereToStart >= addrWhereToStop);
     assert(addrWhereToStart <= bbEndAddrPlus1);

     assert(sparc_instr::instructionHasValidAlignment(addrWhereToStart));
     assert(sparc_instr::instructionHasValidAlignment(addrWhereToStop));

   // Note that if addrWhereToStop==addrWhereToStart, we want to return all-pass.
   // Make sure no assert fails happen in such a corner case.

     const fnCode_t::const_iterator bbcode_iter = theFnCode.findChunk(bbStartAddr,
								      false);
     assert(bbcode_iter != theFnCode.end());
     const fnCode_t::codeChunk &bbCode = *bbcode_iter;
     // accessing bbCode will be a bit quicker than accessing fnCode.  On the downside,
     // we can't use it unless we're certain that the address lies within this 
     // basic block.

     assert(theFnCode.enclosesRange(addrWhereToStop, addrWhereToStart));
     assert(bbCode.enclosesRange(addrWhereToStop, addrWhereToStart));

     for (kptr_t insnaddr = addrWhereToStart-4; insnaddr >= addrWhereToStop;
	  insnaddr -= 4) {
       // get the instruction
       instr_t *insn = bbCode.get1Insn(insnaddr);
      
       // analyze the instruction's register usage
       sparc_instr::sparc_ru ru;
       insn->getRegisterUsage(&ru);
      
      // If this insn is the delay slot of a call, jmp, or interprocedural branch,
      // then now is the time to include the effects of the callee/jumpee/branchee.
      // "destAddr" will track the callee/jumpee/branchee's address
      // BUG FIX NOTE: We used to qualify this by saying "...*and* if the previous
      // instruction is within our analysis range [addrWhereToStart, addrWhereToStop)".
      // But that led to bugs when asking for reg analysis before a delay slot of
      // (say) a call instruction.  In fact, I cannot now think why such a qualification
      // was ever the correct thing to do.  We now change the qualification to
      // "and if the prev insn is within the range of the function, so that read1Insn()
      // won't crash".
      // NEWS FLASH 11/2000: Including the effects of a *conditional* interproc
      // branch is flat wrong, unless we properly conditionalize the effects of the
      // branch destination.  Actually, it's best for other
      // reasons to not include the effects of conditional interproc branches at all
      // in a basic block.  Instead, it's better to include the effects by following
      // edges!  (Try to reg analyze solaris 7/UltraSparc's unix/blkleft to see what
      // I mean).

       kptr_t destAddr=0;
       // destAddr == 0 --> not applicable.
       // destAddr == legitimate address --> use that address
       // destAddr == (kptr_t)-1 --> some unanalyzable destaddr
      
       const kptr_t prev_insn_addr = insnaddr - 4;
       if (bbCode.enclosesAddr(prev_insn_addr) || theFnCode.enclosesAddr(prev_insn_addr)) {
         // the "||" above is for performance reasons; if it's going to be true,
         // it'll probably happen from the first clause (bbCode.enclosesAddr()), which
         // is quicker than fnCode.enclosesAddr.  In fact, the latter clause is only
         // needed because prev_insn_addr may be in a different basic block.
         try {
	   // Check prev insn to see if it's a control flow insn.  Note that the prev
	   // insn may be data; hence "try" despite already having a nice manual
	   // check for containsAddr().

	   sparc_instr::sparc_cfi prev_cfi;
	   instr_t *previnsn = theFnCode.get1Insn(prev_insn_addr);
	   // bbCode.get1Insn() will *usually* work but I guess that's not
	   // good enough.

	   // may throw BadAddr() exception; hence the try block
	   previnsn->getControlFlowInfo(&prev_cfi);

	   if (prev_cfi.fields.controlFlowInstruction) {
	     if (prev_cfi.fields.isCall && !prev_cfi.fields.isJmpLink)
	       // true call instruction
	       destAddr = prev_insn_addr + prev_cfi.offset_nbytes;
	     else if (prev_cfi.fields.isRet || prev_cfi.sparc_fields.isRetl ||
		      prev_cfi.sparc_fields.isV9Return || 
		      prev_cfi.sparc_fields.isDoneOrRetry)
	       ; // keep not applicable
	     else if (prev_cfi.fields.isJmpLink) {
	       if (prev_cfi.fields.isCall)
		 // unanalyzable destination, but definitely a call that'll return
		 destAddr = (kptr_t)-1;
	       else {
		 // Either a nice jump table (keep not applicable) or an optimized
		 // tail call with an unanalyzable destination.  To determine one
		 // verses the other case, we check the successors: if just a single
		 // successor which is ExitBB, then assume it's a tail call to
		 // unanalyzable destination.
		 if (bbSuccOf0 == basicblock_t::xExitBBId) {
		   assert(bbNumSuccs == 1); // xExitBBId as only successor
		   destAddr = (kptr_t)-1;
		 }
		 else
		   ; // assume it's a nice jump table.  Keep not applicable.
	       }
             }
	     else if (prev_cfi.sparc_fields.isBicc || 
		      prev_cfi.sparc_fields.isBPcc ||
		      prev_cfi.sparc_fields.isBPr ||
		      prev_cfi.sparc_fields.isFBfcc || 
		      prev_cfi.sparc_fields.isFBPfcc) {
	       // Brand new 12/2000: don't do interprocedural branches here;
	       // they're included along the edges, as they should be!
	       // (Now handled in interProcRegAnalysis.h)
	     }
	     else
	       assert(false);
            
	     // This has nothing to do with finding the callee; we do it here because
	     // it's convenient: if this insn is a conditionally-executed delay slot,
	     // then definitely (reads/writes) must be changed into maybe
	     // (reads/writes).
	     // HOWEVER, don't do any of this if the current instruction is a save
	     // or restore instruction (since certain accomodations for this case were
	     // already made during parsing; to wit: putting the save/restore insn in
	     // its own basic block).
	     if (prev_cfi.delaySlot == instr_t::dsWhenTaken) {
	       if (!ru.sr.isSave && !ru.sr.isRestore) {
		 *((sparc_reg_set*)ru.maybeUsedBeforeWritten) += 
		   *((sparc_reg_set*)ru.definitelyUsedBeforeWritten);
		 *((sparc_reg_set*)ru.maybeWritten) += 
		   *((sparc_reg_set*)ru.definitelyWritten);
		 ru.definitelyUsedBeforeWritten->clear();
		 ru.definitelyWritten->clear();
	       }
	     }
	   }
         }
         catch (fnCode_t::BadAddr) {
	   // if previous instruction was not in this function, then do nothing(?)
         }
       }
      
       // One more chance to find an interprocedural branch: if the interproc branch
       // had no delay slot (ba,a) then checking the previous insn missed it; we need
       // to check the *current* insn in such cases.
       // NEW 12/2000: as stated above, we longer do this crap here; interproc branches
       // are handled along edges, as they always should have been.  So go see
       // interProcRegAnalysis.h for that stuff now.

       // OK -- if there's a call or interproc jump to handle, we've
       // found it by now.
       // NEW: 12/2000: note that "interprocedural branch" is not included in
       // the above listing; that's on purpose.  Inteproc jump, you're next on
       // the list to be removed from this file.  Calls, you get to stay forever.

       if (destAddr != 0) {
         // Regrettably, I don't think the dataflow fn can be made a const
         // reference any more, because one rare case of whenDestAddrIsKnown()
         // for one oracle needs to return a calculated value, instead of always
         // returning a pre-calculated, already-stored value.
         const monotone_bitwise_dataflow_fn *effectOfCall =
	   (destAddr == (kptr_t)-1) ?
	   monotone_bitwise_dataflow_fn::getDataflowFn(*(callOracle.whenDestAddrIsUnknown())):
	   monotone_bitwise_dataflow_fn::getDataflowFn(*(callOracle.whenDestAddrIsKnown(destAddr)));
	 
	 //quickly save a pointer to memory we are otherwise leaking...
         monotone_bitwise_dataflow_fn *temp = result;
	 result = effectOfCall->compose(*result);
	 delete temp;
         delete effectOfCall;
	 // compose: first 'result', then apply 'effectOfCall'
       }
      
       // OK, enough talk about following calls/interproc jumps (if appropriate).
       // Time to handle *US*, the instruction's effects.
       // Reverse data-flow problem, so process in this order: (a) reg writes,
       // (b) handle effect of save or restore, if appropriate, (c) reg reads.
      
       // Apply register writes.  Note that if this is a save or restore instruction,
       // the register write was done in the 'new' window, which is the one we have
       // been using (since we're doing a _reverse_ data flow problem).
       //
       // ** When we see a register write, apply STOP to the data flow fn for that reg.
       // For a maybe-written register, we must be conservative and make no change.

       result->stopSet(*ru.definitelyWritten);
       // maybe written not included (we must be conservative and err on the side
       // of saying that a register is live)

       if (ru.sr.isSave)
         // We've been working with the window opened up by the save.  Back up to the
  	 // state of things _before_ the save:  assign %o's to the %i's we have been
         // using...no change to %g's...discard %l's and %i's that we have been
         // using...and replace them with the %l's and %i registers in use by
         // instructions above (temporally before) the save instr...to obtain this, we
         // refer to the stack of pre-save register windows and pop a frame off.
         // (A restore operation will have pushed an entry onto the pre-save stack.)
         // If we don't have access to a pre-save frame, we might despair.  But
         // consider: since this is a reverse data flow problem, we are about to work
         // with the frame before the save!  Thus, why not just initialize the %l and
         // %i registers to PASS, and continue in the reverse direction?  (Because
         // pre-save also means post-restore...and we can't assume that no registers
 	 // are changed after a restore.  Luckily, we should have post-restore
 	 // information (since this is a bw proble), so we should have the pre-save
 	 // information.

         result->processSaveInBackwardsDFProblem();
         // assign %o's to %i's we've been using; set %l's and %i's to (pop
         // off pre-save/post-restore window stack).

       else if (ru.sr.isRestore || ru.sr.isV9Return)
	 // We have been working with the window popped by the restore.  Back up to
	 // the state of things _before_ the restore.  To make things easier to
 	 // understand, note that before the restore == after the save.
	 // So: let %i's get the %o's we've been working with...no change to %g's...
         // and (the neat part) %l's and %o's get STOP (yea!).
         result->processRestoreInBackwardsDFProblem();
         // push %i's and %l's onto pre-save window stack.
         // assign %i's to %o's we've been using; set %l's and %o's to STOP

       // reg reads:
       // Next in our reverse data flow problem, apply registers reads
       // Anything that might read a register --> START that bit.
       result->startSet(*ru.maybeUsedBeforeWritten);
       result->startSet(*ru.definitelyUsedBeforeWritten);
     }

     return result;
   }
};

#endif /* SPARC_BB_H_ */
