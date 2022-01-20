#ifndef _X86_BB_H_
#define _X86_BB_H_

#include <inttypes.h> // uint32_t

#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"

#include "x86_instr.h"
#include "x86_reg_set.h"
#include "fnCode.h"

// Some fwd decls to avoid recursive #include's:
class basicblock_t;
class monotone_bitwise_dataflow_fn;

class x86_bb {
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
      // numInsnBytesToSkipAtEnd: the number of instruction bytes out of this 
      // basic block, counting from the bottom of the basic block, that should 
      // be ignored when doing the slice.  Of course 0 means skip nothing and 
      // consider the whole basic block when slicing.
      assert(numInsnBytesToSkipAtEnd <= bbNumInsnBytes);

      x86_reg_set &regs2slice = (x86_reg_set&)regsToSlice;

      // Since a basic block never crosses a chunk boundary, we can get a 
      // chunk now and use its quick version of get1Insn() rather than 
      // function's get1Insn().
      fnCode_t::const_iterator chunk_iter = theFnCode.findChunk(bbStartAddr, false);
      assert(chunk_iter != theFnCode.end());
      const fnCode_t::codeChunk &theCodeChunk = *chunk_iter;
      
      const unsigned bbnumInsns =
	 theCodeChunk.calcNumInsnsWithinBasicBlock(bbStartAddr,
						   bbEndAddrPlus1);

      // initialize the result: no instructions are in the slice
      pdvector<bool> result(bbnumInsns, false);

      // add the PC register to regsToSlice if bb ends in a *conditional* 
      // branch (unless it falls outside of the range of insns to slice 
      // thanks to numInsnBytesToSkipAtEnd)
      if (includeControlDependencies && numInsnBytesToSkipAtEnd == 0)
	 factorInControlDependencyInRegsToSlice(regsToSlice, theCodeChunk, 
						bbStartAddr, bbEndAddrPlus1);

      assert(numInsnBytesToSkipAtEnd == 0);
         // we have no way to convert it to a number of insns at the moment

      int lcv = bbnumInsns - 1;

      while (true) {
	 if (regs2slice.isempty())
	    return result;

	 assert(lcv >= 0);
	 instr_t *instr = theCodeChunk.get1InsnByNdx(lcv);

	 instr_t::registerUsage ru;
	 instr->getRegisterUsage(&ru);

	 *((x86_reg_set*)ru.definitelyUsedBeforeWritten) -= 
	    x86_reg_set::regsThatIgnoreWrites;
	 *((x86_reg_set*)ru.maybeUsedBeforeWritten) -= 
	    x86_reg_set::regsThatIgnoreWrites;

	 const x86_reg_set regsToSlice_definitelyWritten = 
	    *((x86_reg_set*)ru.definitelyWritten) & regs2slice;
	 const x86_reg_set regsToSlice_maybeWritten = 
	    *((x86_reg_set*)ru.maybeWritten) & regs2slice;

	 if (!regsToSlice_definitelyWritten.isempty()) {
	    // The insn contributes to the value of some reg(s) we're slicing;
	    // thus, the insn is part of the slice.
	    result[lcv] = true;

	    // The insn kills the set regsToSlice_definitelyWritten (probably 
	    // just a single entry in the set, by the way).  Since they're 
	    // dead, no previous insn can affect them insofar as the slice 
	    // concerns.  Our register interest now shifts to registers 
	    // _used_ by this insn.

	    regs2slice -= regsToSlice_definitelyWritten; // dead

	    regs2slice += *((x86_reg_set*)ru.definitelyUsedBeforeWritten); 
	    regs2slice += *((x86_reg_set*)ru.maybeUsedBeforeWritten); 
	 }
	 if (!regsToSlice_maybeWritten.isempty()) {
	    // This instr *might* contribute to the value of some regs we care
	    // about. Thus, add it to the slice.
	    result[lcv] = true;

	    // only maybeWritten, can't declare regs dead as in above case

	    // Live regs which contribute to the slice: care about 'em:
	    regs2slice += *((x86_reg_set*)ru.definitelyUsedBeforeWritten); 
	    regs2slice += *((x86_reg_set*)ru.maybeUsedBeforeWritten); 
	 }

	 if (lcv == 0) break;
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

      unsigned ndxPossibleBranch = bb_num_insns - 1;
      instr_t *possible_branch = theCodeChunk.get1InsnByNdx(ndxPossibleBranch);

      x86_instr::x86_cfi cfi;
      possible_branch->getControlFlowInfo(&cfi);

      if (!cfi.fields.controlFlowInstruction)
	 return;
      else if(cfi.fields.isBranch && cfi.fields.isConditional)
	 (x86_reg_set&)regsToSlice += x86_reg::PC;
   }

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
      // a REVERSE data flow analysis problem.  We work backwards among the 
      // instructions in the basic block, checking which registers it uses and
      // writes to. The return value is a bitwise data flow function for this 
      // basic block, which can be applied to a bitwise data flow vector 
      // describing live registers at the *bottom* of this bb to obtain live 
      // registers at the *top* of this bb.
      //
      // Handling calls:
      //   When we see a call, we 'follow' the callee by invoking a method of
      //   callOracle for object 'callback'; it returns a mbdf 
      //   which we'll use as the effects of the call.
      //
      // Handling interprocedural branches/jumps (as opposed to calls):
      //   When the destination address is known, these are handled just like 
      //   calls.
      //
      // Algorithm:
      // Since this is a backwards data flow problem, we must work temporally 
      // backwards, handling the effects of the basic block. To make the steps
      // clear, let's write out (in usual temporal direction) the effects of a
      // typical instruction:
      //
      //   1) read some src registers
      //   2) write a dest register
      //
      // Some instructions are CTI's (control transfer instructions), so 
      // include the callee:
      //
      //   3) process effects of the [branch, call, bpr, ...]
      //
      // To process an instruction in reverse temporal order, perform steps 
      // 1-3 in reverse order.
      //
      // So,
      //   a) If curr insn is a call or interprocedural branch/jump, process
      //      the call's destination.
      //   b) Process writes of curr insn
      //   c) Process reads of curr insn
      //   d) Go to previous insn and repeat until there are no more

      monotone_bitwise_dataflow_fn *result = const_cast<monotone_bitwise_dataflow_fn *>(callOracle.getIdentityDataflowFn());

      assert(bbNumInsnBytes > 0); // This should always hold for a basic block
      
      assert(addrWhereToStop >= bbStartAddr);
      assert(addrWhereToStart >= addrWhereToStop);
      assert(addrWhereToStart <= bbEndAddrPlus1);
      
      // Note that if addrWhereToStop==addrWhereToStart, we want to return 
      // all-pass. Make sure no assert fails happen in such a corner case.
      if(addrWhereToStart == addrWhereToStop)
         return result;

      const fnCode_t::const_iterator bbcode_iter = theFnCode.findChunk(bbStartAddr, false);
      assert(bbcode_iter != theFnCode.end());
      const fnCode_t::codeChunk &bbCode = *bbcode_iter;

      // accessing bbCode will be a bit quicker than accessing fnCode. On the 
      // downside, we can't use it unless we're certain that the address lies 
      // within this basic block.

      assert(theFnCode.enclosesRange(addrWhereToStop, addrWhereToStart));
      assert(bbCode.enclosesRange(addrWhereToStop, addrWhereToStart));

      // get iterator for first & last insn
      unsigned num_insns = bbCode.calcNumInsnsWithinBasicBlock(addrWhereToStop,
                                                               addrWhereToStart);

      fnCode_t::codeChunk::const_iterator iter = bbCode.begin();
      fnCode_t::codeChunk::const_iterator end = bbCode.end();
      instr_t *stop_insn = bbCode.get1Insn(addrWhereToStop);
      int ndx = 0;
      int stop_ndx = 0;
      unsigned stop_found = 0;
      while(iter != end) {
         if(!stop_found) {
            if(*iter == stop_insn) {
               stop_ndx = ndx;
               stop_found = 1;
               if(num_insns == 1)
                  break;
            }
         }
         else if(ndx == (stop_ndx+(num_insns-1)))
            break;
         ndx++;
         iter++;
      }
      
      // at this point, iter points to starting insn
      if(iter == end) {
         cerr << "ERROR: x86_bb::doLiveRegisterAnalysis - did not find start insn in bbCode; ndx=" << ndx << ", stop_ndx=" << stop_ndx << ", start_ndx should have been " << stop_ndx+(num_insns-1) << endl;
         assert(0);
      }

      kptr_t insnaddr = addrWhereToStart;
      for(; ndx >= stop_ndx; ndx--) {
         // get the instruction
         assert(ndx >= stop_ndx);
         const instr_t *insn = *iter;
         assert(insn != NULL);
         unsigned insn_num_bytes = insn->getNumBytes();
         if(ndx == stop_ndx) {
            insnaddr = addrWhereToStop;
         }
         else {
            insnaddr -= (*iter)->getNumBytes();
            iter--;
            assert(*iter != NULL);
         }

         // analyze the instruction's register usage
         x86_instr::registerUsage ru;
         insn->getRegisterUsage(&ru);

         // First thing we need to check is if this is a CTI, so we can
         // process the destination first

         kptr_t destAddr=0;
            // destAddr == 0 --> not applicable.
            // destAddr == legitimate address --> use that address
            // destAddr == (kptr_t)-1 --> some unanalyzable destaddr

         x86_instr::x86_cfi cfi;
         insn->getControlFlowInfo(&cfi);

         if(cfi.fields.controlFlowInstruction) {
            // Only want to add effects for unconditional interprocedural
            // calls/jmps, since conditional ones are handled by our
            // interProcRegAnalysis
            if(cfi.fields.isCall || 
               (cfi.fields.isBranch && !cfi.fields.isConditional)) {
               if(cfi.destination == instr_t::controlFlowInfo::pcRelative)
                  destAddr = (insnaddr + insn_num_bytes) + cfi.offset_nbytes;
               else
                  destAddr = (kptr_t)-1;
            }
         }

         if (destAddr != 0) {
            // Regrettably, I don't think the dataflow fn can be made a const
            // reference any more, because one rare case of whenDestAddrIsKnown
            // for one oracle needs to return a calculated value, instead of 
            // always returning a pre-calculated, already-stored value.
            const monotone_bitwise_dataflow_fn *effectOfCall =
               (destAddr == (kptr_t)-1) ?
               monotone_bitwise_dataflow_fn::getDataflowFn(*(callOracle.whenDestAddrIsUnknown())):
               monotone_bitwise_dataflow_fn::getDataflowFn(*(callOracle.whenDestAddrIsKnown(destAddr)));
	 
            // quickly save a pointer to memory we are otherwise leaking...
            monotone_bitwise_dataflow_fn *temp = result;

            // compose: first 'result', then apply 'effectOfCall'
            result = effectOfCall->compose(*result);

            delete temp;
            delete effectOfCall;
         }

         // Time to handle *US*, the instruction's effects.
         // Reverse data-flow problem, so process in this order: 
         // (a) reg writes, (b) reg reads.
         
         // reg writes:
         // When we see a register write, apply STOP to the data flow fn for 
         // that reg. For a maybe-written register, we must be conservative 
         // and make no change.
         result->stopSet(*ru.definitelyWritten);

         // reg reads:
         // Next in our reverse data flow problem, apply registers reads
         // Anything that might read a register --> START that bit.
         result->startSet(*ru.maybeUsedBeforeWritten);
         result->startSet(*ru.definitelyUsedBeforeWritten);
      }

      return result;
   }
};

#endif /* X86_BB_H_ */
