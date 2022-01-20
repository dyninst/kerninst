#ifndef _POWER_BB_H_
#define _POWER_BB_H_

#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"
#include <inttypes.h> // uint32_t
#include "power_instr.h"
#include "fnCode.h"
#include "powerTraits.h"
#include "power_bitwise_dataflow_fn.h"
// fwd decls to avoid recursive #includes:
class basicblock_t;
class power_reg_set;
class monotone_bitwise_dataflow_fn;

class power_bb {
public:
   typedef fnCode fnCode_t;
   typedef uint16_t bbid_t; //same as in basicblock.h

   static void
      factorInControlDependencyInRegsToSlice(regset_t  &  /* regsToSlice */ ,
                                             const fnCode_t::codeChunk  & /* theCodeChunk */,
                                             kptr_t  /* bbStartAddr  */,
                                             kptr_t /* bbEndAddrPlus1*/) {
      // This routine adds the PC register to regsToSlice if the bb ends with a
      // *conditional* branch.
      //Applicable to Power??
      assert(false && "not implemented until a need for such functionality appears");
   }

      static pdvector<bool> backwardsSlice(kptr_t /* succ_bb_startAddr*/, // used for ds on sparc
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

   // In the future, we may allow slicing by memory locations, not just regs.

     fnCode_t::const_iterator chunk_iter = theFnCode.findChunk(bbStartAddr, false);
     assert(chunk_iter != theFnCode.end());
     const fnCode_t::codeChunk &theCodeChunk = *chunk_iter;

     const unsigned bbNumInsns = 
        theCodeChunk.calcNumInsnsWithinBasicBlock(bbStartAddr,
                                                  bbEndAddrPlus1);

     // initialize the result: no instructions are in the slice
     pdvector<bool> result(bbNumInsns, false);
     
     
     //   ****** Not applicable to Power?? -Igor ****************
     // add the PC register to regsToSlice if bb ends in a *conditional* branch (unless
     // it falls outside of the range of insns to slice thanks to numInsnBytesToSkipAtEnd)
     if (includeControlDependencies && numInsnBytesToSkipAtEnd == 0)
        factorInControlDependencyInRegsToSlice(regsToSlice, theCodeChunk, 
                                               bbStartAddr, bbEndAddrPlus1);
    


     unsigned lcv = bbNumInsns - 1 - (numInsnBytesToSkipAtEnd / 4);
     
     unsigned byteoffset = bbNumInsnBytes - 4 - numInsnBytesToSkipAtEnd;
     
     while (true) {
        if (regsToSlice.isempty())
           return result;
        
        assert(byteoffset < bbNumInsnBytes);
        const power_instr *instr = 
           (const power_instr *)theCodeChunk.get1Insn(bbStartAddr + byteoffset);
        
        power_instr::registerUsage ru;
        instr->getRegisterUsage(&ru);
        
        *(power_reg_set *)ru.definitelyUsedBeforeWritten -=  
           power_reg_set::regsThatIgnoreWrites;
        *(power_reg_set *)ru.maybeUsedBeforeWritten -= 
           power_reg_set::regsThatIgnoreWrites;

        const power_reg_set regsToSlice_definitelyWritten = 
           *(power_reg_set *)ru.definitelyWritten &  (power_reg_set &)regsToSlice;
        const power_reg_set regsToSlice_maybeWritten = 
           *(power_reg_set *)ru.maybeWritten & (power_reg_set &)regsToSlice;

           
        power_reg_set &regsToSlice2 = (power_reg_set &)regsToSlice;
        //allow for easy use of power_reg_set operators
        
        if (!regsToSlice_definitelyWritten.isempty()) {
           // The insn contributes to the value of some reg(s) we're slicing; thus, the
           // insn is part of the slice.
           result[lcv] = true;

           // The insn kills the set regsToSlice_definitelyWritten 
           //(probably just a single entry in the set, by the way).  Since 
           //they're dead, no previous insn can affect them insofar as the
           //slice concerns.  Our register interest now shifts to registers 
           //_used_ by this insn.
           
           //Note that we want to add in all the registers that have been
           //used here, not just the ones we're interested in, since
           //those other registers might contain the data from the
           //registers we are initially interested in. -Igor
        
           regsToSlice2 -= regsToSlice_definitelyWritten; // dead
           
           regsToSlice2 += *(power_reg_set *)ru.definitelyUsedBeforeWritten; // g0 has been removed -- good
           regsToSlice2 += *(power_reg_set *)ru.maybeUsedBeforeWritten; // g0 has been removed -- good
        }
        if (!regsToSlice_maybeWritten.isempty()) {
           // This instr *might* contribute to the value of some regs we care about.
           // Thus, add it to the slice.
           result[lcv] = true;
           
           // The regs written to cannot be declared dead.
           ;
           
           // Live regs which contribute to the slice: care about 'em:
           regsToSlice2 += *(power_reg_set *)ru.definitelyUsedBeforeWritten; // g0 has been removed -- good
           regsToSlice2 += *(power_reg_set *)ru.maybeUsedBeforeWritten; // g0 has been removed -- good
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

   // Sun's CC says templated methods can't be static.  I disagree.
   template <class calcEffectOfCall>
      static monotone_bitwise_dataflow_fn *
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

     monotone_bitwise_dataflow_fn *result = monotone_bitwise_dataflow_fn::
        getDataflowFn(*(callOracle.getIdentityDataflowFn()));

     assert(bbNumInsnBytes > 0); // This should always hold for a basic block

     assert(addrWhereToStop >= bbStartAddr);
     assert(addrWhereToStart >= addrWhereToStop);
     assert(addrWhereToStart <= bbEndAddrPlus1);

     assert(power_instr::instructionHasValidAlignment(addrWhereToStart));
     assert(power_instr::instructionHasValidAlignment(addrWhereToStop));

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
       const power_instr *insn = (power_instr *) bbCode.get1Insn(insnaddr);
      
       // analyze the instruction's register usage
       power_instr::registerUsage ru;
       insn->getRegisterUsage(&ru);
    
       // If this insn is a call, jmp, or interprocedural branch,
      // then now is the time to include the effects of the callee/jumpee/branchee.
      // "destAddr" will track the callee/jumpee/branchee's address
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

       power_instr::power_cfi cfi;
     
       insn->getControlFlowInfo(&cfi);
       
       if (cfi.fields.controlFlowInstruction && cfi.power_fields.link) {
          if  ( cfi.destination != 
                instr_t::controlFlowInfo::regRelative) {
             // call or jump to fixed address
             if (cfi.destination == instr_t::controlFlowInfo::pcRelative)
                destAddr = insnaddr + cfi.offset_nbytes;
             else //destination == absolute
                destAddr = cfi.offset_nbytes;
          }
          else if (cfi.fields.isRet)
             ; // keep not applicable
          else if (cfi.fields.isConditional)
             ; //conditional call/return, keep not applicable
	       else {
             // Either a nice jump table (keep not applicable) or an optimized
             // tail call with an unanalyzable destination.  To determine one
             // verses the other case, we check the successors: if just a 
             //single successor which is ExitBB, then assume it's a tail call 
             //to unanalyzable destination.
             if (bbSuccOf0 == basicblock_t::xExitBBId) {
                assert(bbNumSuccs == 1); // xExitBBId as only successor
                destAddr = (kptr_t)-1;
             }
             else
                ; // assume it's a nice jump table.  Keep not applicable.
	       }
       
       }  

       if (destAddr != 0) {
         // Regrettably, I don't think the dataflow fn can be made a const
         // reference any more, because one rare case of whenDestAddrIsKnown()
         // for one oracle needs to return a calculated value, instead of always
         // returning a pre-calculated, already-stored value.
         monotone_bitwise_dataflow_fn *effectOfCall =
            (destAddr == (kptr_t)-1) ?
            monotone_bitwise_dataflow_fn::getDataflowFn(*(callOracle.whenDestAddrIsUnknown())) :
            monotone_bitwise_dataflow_fn::getDataflowFn(*(callOracle.whenDestAddrIsKnown(destAddr)));
        
	//quickly save a pointer to memory we are otherwise leaking...
	 monotone_bitwise_dataflow_fn *temp = result;
	 result = effectOfCall->compose(*result);
         // compose: first 'result', then apply 'effectOfCall'
         
         //get rid of memory we are not going to use anymore
         delete temp;
	 delete effectOfCall;
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

       result->stopSet(*(ru.definitelyWritten));
       // maybe written not included (we must be conservative and err on the side
       // of saying that a register is live)

       // reg reads:
       // Next in our reverse data flow problem, apply registers reads
       // Anything that might read a register --> START that bit.
       result->startSet(*(ru.maybeUsedBeforeWritten));
       result->startSet(*(ru.definitelyUsedBeforeWritten));
   
     }
     return result;
   }
};

#endif /* POWER_BB_H_ */
