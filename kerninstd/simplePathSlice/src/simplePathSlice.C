// simplePathSlice.C

#include "simplePathSlice.h"
#include "common/h/String.h"
#include "util/h/rope-utils.h" // addr2hex()
#include "insnVec.h"
#include "funkshun.h"
#include "simplePath.h"
#include <pair.h>

// Delay slots are tricky to handle.  We must note that delay slots are
// not always executed; thus, they shouldn't always show up in a slice.
// For example, bgu,a annuls the delay slot unless the branch is taken.
// Thus, if the branch isn't taken (judging by the input path), then the
// slice shouldn't include the delay slot insn!


simplePathSlice::simplePathSlice(const function_t &ifn,
				 const simplePath &thePath,
				 regset_t &regsToSlice,
				 bool includeControlDependencies,
				 unsigned numInsnBytesToSkipAtEnd) : fn(ifn) 
{
   // XXX -- the following is a sparc-centric assert
   assert(numInsnBytesToSkipAtEnd % 4 == 0);
   
   if (thePath.size() == 0 || regsToSlice.isempty())
      return;

   // Handle the last bb in the path specially, due to numInsnBytesToSkipAtEnd
   unsigned bbNdx = thePath.size()-1;

   // skip 0 or more entire basic blocks at the end of the path, as dictated
   // by numInsnBytesToSkipAtEnd
   while (true) {
      unsigned lastBB_id = thePath[bbNdx];
      const basicblock_t *lastBB = fn.getBasicBlockFromId(lastBB_id);
      const unsigned lastBB_numInsnBytes = lastBB->getNumInsnBytes();

      if (numInsnBytesToSkipAtEnd >= lastBB_numInsnBytes) {
         // skip this whole basic block
         numInsnBytesToSkipAtEnd -= lastBB_numInsnBytes;
         if (bbNdx-- == 0)
            return;
      }
      else {
         // we are not going to skip this basic block, yet we *might* process just part
         // of it, as dictated by numInsnBytesToSkipAtEnd.
         // This is the first basic block that we'll process.

	 pdvector<bool> vb = lastBB->backwardsSlice(-1U, // there is no successor bb
                                                      // in the slice yet (actually
                                                      // this param is ignored anyway)
                                                 regsToSlice,
                                                 includeControlDependencies, // false??
                                                 fn.getOrigCode(),
                                                 numInsnBytesToSkipAtEnd);

	 // We have the slice for this bb; add to the result slice.
	 unsigned lcv = vb.size()-1;
	 do {
	    if (vb[lcv])
	       addInsnToSlice(lastBB_id, lcv);
	 } while (lcv-- > 0);

	 // Have we reached the end of the path, or is regsToSlice now empty?
         // If so, then we're done.
	 if (bbNdx-- == 0 || regsToSlice.isempty())
	    return;

	 break;
      }
   }

   // At this point, we have processed exactly one basic block in the path, so we
   // can assert that bbNdx is not the last ndx in the path.
   // (Don't be mislead by the while statement you see above; it's only for skipping
   // stuff at the end of the path).
   // Furthermore, there's still something left in the path to process.
   assert(bbNdx < thePath.size() - 1);

   do {
      // There's something still left in the path to process.

      if (regsToSlice.isempty())
	 return; // nothing left to slice

      unsigned bb_id = thePath[bbNdx];
      const basicblock_t *bb = fn.getBasicBlockFromId(bb_id);
      const basicblock_t *succ_bb = fn.getBasicBlockFromId(thePath[bbNdx+1]);
      pdvector<bool> vb = bb->backwardsSlice(succ_bb->getStartAddr(),
                                             // this param may be ignored
                                          regsToSlice, includeControlDependencies,
                                          fn.getOrigCode());
      if (vb.size() == 0)
	 continue;

      // Add insns in the bb slice to the result slice:
      unsigned lcv=vb.size()-1;
      do {
	 if (vb[lcv])
	    addInsnToSlice(bb_id, lcv);
      } while (lcv-- > 0);
   } while (bbNdx-- > 0);
}


simplePathSlice::simplePathSlice(const simplePathSlice &parentSlice,
				 unsigned numInsnsToSkip,
				 bool includeControlDependencies,
				 regset_t &regsToSlice) : fn(parentSlice.fn) 
{
   // sub-slice constructor
   // this->theSlice is initially a vector of size 0

   typedef function_t::bbid_t bbid_t;
 
   for (unsigned ndx = numInsnsToSkip;
	ndx < parentSlice.theSlice.size() && !regsToSlice.isempty(); ndx++) {
      bbid_t bb_id = parentSlice.theSlice[ndx].bb_id;
      unsigned ndxWithinBB = parentSlice.theSlice[ndx].ndxWithinBB;
      const basicblock_t *bb = fn.getBasicBlockFromId(bb_id);

      // TODO: Good lord, speed this up!!!!!  (Change slice info from bbid/insnNdx
      // to bbid/byteOffsetWithinBB).
      unsigned byteOffsetWithinBB = 0;
      for (unsigned ndx_lcv=0; ndx_lcv < ndxWithinBB; ++ndx_lcv) {
         instr_t *i = fn.get1OrigInsn(bb->getStartAddr() + byteOffsetWithinBB);
         byteOffsetWithinBB += i->getNumBytes();
      }
      instr_t *instr = fn.get1OrigInsn(bb->getStartAddr() + byteOffsetWithinBB);
      
      // control dependencies must be added if we're about to look at a CFI insn.

      // factoring in control dependencies must only be checked when we're at the
      // bottom of some basic block.  By the way, since the original slice can span
      // multiple basic blocks, we cannot hoist this if statement out of the for loop.
      const unsigned numInsnsInBB =
         fn.getOrigCode().calcNumInsnsWithinBasicBlock(bb->getStartAddr(),
                                                       bb->getEndAddrPlus1());
      
      if (includeControlDependencies && ndxWithinBB == numInsnsInBB-1) {
         const  fnCode_t::const_iterator chunk_iter =
            fn.getOrigCode().findChunk(bb->getStartAddr(), false);
         assert(chunk_iter != fn.getOrigCode().end());
         const  fnCode_t::codeChunk &theCodeChunk = *chunk_iter;
         
	 bb->factorInControlDependencyInRegsToSlice(regsToSlice, theCodeChunk);
      }
      
      instr_t::registerUsage ru;
      instr->getRegisterUsage(&ru);

      *(ru.definitelyUsedBeforeWritten) -= regset_t::getRegsThatIgnoreWrites();
      *(ru.maybeUsedBeforeWritten) -= regset_t::getRegsThatIgnoreWrites();
      
      const regset_t *regsToSlice_definitelyWritten = 
         ru.definitelyWritten->setIntersection(regsToSlice);
      const regset_t *regsToSlice_maybeWritten = 
         ru.maybeWritten->setIntersection(regsToSlice);
     
      if (!regsToSlice_definitelyWritten->isempty() ||
	  !regsToSlice_maybeWritten->isempty()) {
	 regsToSlice -= *regsToSlice_definitelyWritten;
	    // *definitely* written --> definitely dead

	 regsToSlice += *(ru.definitelyUsedBeforeWritten); 
	 regsToSlice += *(ru.maybeUsedBeforeWritten);

	 this->theSlice += parentSlice.theSlice[ndx];
   

      }
    //delete the result of setIntersection()
    delete regsToSlice_definitelyWritten;
    delete regsToSlice_maybeWritten;
   } // loop thru contents of the parent slice
}


simplePathSlice::simplePathSlice(const simplePathSlice &src) :
      fn(src.fn),
      theSlice(src.theSlice) 
{
   iterationNdx = 0; // we could just as well leave it undefined.
}


void simplePathSlice::addInsnToSlice(unsigned bb_id, unsigned ndxWithinBB) 
{
   sliceInsn theSliceInsn;
   theSliceInsn.bb_id = bb_id;
   theSliceInsn.ndxWithinBB = ndxWithinBB;
   theSlice += theSliceInsn;
}


void simplePathSlice::beginBackwardsIteration() const 
{
   // since the slice, to begin with, was a backwards slice, we want to start at ndx 0.
   if (theSlice.size() == 0)
      iterationNdx = UINT_MAX;
   else
      iterationNdx = 0;
}


bool simplePathSlice::isEndOfIteration() const 
{
   return (iterationNdx >= theSlice.size());
}


instr_t* simplePathSlice::getCurrIterationInsn() const 
{
   return fn.get1OrigInsn(getCurrIterationInsnAddr());
}


kptr_t simplePathSlice::getCurrIterationInsnAddr() const 
{
   sliceInsn theSliceInsn = theSlice[iterationNdx];
   unsigned bb_id = theSliceInsn.bb_id;
   const basicblock_t *bb = fn.getBasicBlockFromId(bb_id);

   const fnCode_t &theCode = fn.getOrigCode();
   unsigned insn_offset = theSliceInsn.ndxWithinBB;
   kptr_t result = bb->getStartAddr();
   while (insn_offset--) {
      instr_t *i = theCode.get1Insn(result);
      result += i->getNumBytes();
   }

   return result;
}


pair<unsigned, unsigned>
simplePathSlice::getCurrIterationBlockIdAndOffset() const 
{
   sliceInsn theSliceInsn = theSlice[iterationNdx];
   
   return make_pair(theSliceInsn.bb_id, theSliceInsn.ndxWithinBB);
}



void simplePathSlice::advanceIteration() const 
{
   iterationNdx++;
}

// ***************************************************************


void simplePathSlice::printx(ostream &os) const 
{
   // Since beginBackwardsIteration() will iterate through the slice "moving up",
   // it's not so suitable for printing out, since the asm instructions will
   // look to be in reverse order from how they actually occur in the program!
   // So we buffer up the result and reverse its order.

   pdvector<pdstring> result;
   
   beginBackwardsIteration();
   while (!this->isEndOfIteration()) {
      pdstring entry = addr2hex(getCurrIterationInsnAddr());
      entry += " ";
      entry += getCurrIterationInsn()->disassemble(getCurrIterationInsnAddr());
      result += entry;
      
      this->advanceIteration();
   }

   // print out in reverse order.  Could probably use some cute STL method
   unsigned num_printed = 0; // just for an assert check
   if (result.size()) {
      unsigned lcv = result.size()-1;
      do {
         os << result[lcv] << endl;
         num_printed++;
      } while (lcv--);
   }
   assert(num_printed == result.size());
}
