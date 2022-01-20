// This is the implementations for architecture-specific functionality
// of the function class, as required for SPARC

#include "sparc_funkshun.h"
#include "sparc_basicblock.h"
#include "fn_misc.h" // findModuleAndReturnStringPoolReference()
#include "simplePath.h"
#include "sparc_controlFlow.C"
#include "util/h/minmax.h"
#include "util/h/hashFns.h"
#include <algorithm> // binary_search(), adjacent_find()
#include "moduleMgr.h" // ugh
#include "util/h/xdr_send_recv.h" // xdr_recv_fail
#include "xdr_send_recv_common.h" // P_xdr_send() & _recv() for insnVec, e.g.
#include "util/h/out_streams.h"

#ifdef _KERNINSTD_
#include "cfgCreationOracle.h"
#include "codeObjectCreationOracle.h"
#include "xdr_send_recv_kerninstd_only.h"
#include "sparc_kmem.h"
extern bool verbose_fnParse; 
extern bool verbose_hiddenFns; 
#endif

kptr_t sparc_function::getBBInterProcBranchDestAddrIfAny(bbid_t bbid) const {
   // WARNING: The following is WAY too sparc-specific.

   assert(instr_t::hasUniformInstructionLength());

   const basicblock_t *bb = getBasicBlockFromId(bbid);
   
   const kptr_t lastInsnAddr = bb->getEndAddrPlus1() - 4;
   instr_t *lastInsn = get1OrigInsn(lastInsnAddr);

   bool lastInsnIsTheOne = false;
   kptr_t lastInsnBranchDestAddr = 0;
   if(lastInsn->isBranchToFixedAddress(lastInsnAddr, lastInsnBranchDestAddr)) {
      // OK, it was the last instruction in the basic block that did the
      // interprocedural branch.  Probably ba,a...but could be another kind of
      // condition, if for some reason the delay slot fell in a different block.
      lastInsnIsTheOne = true;
      assert(!containsAddr(lastInsnBranchDestAddr));
   }

   bool secondToLastInsnIsTheOne = false;
   kptr_t secondToLastInsnBranchDestAddr = 0;
   if (bb->getNumInsnBytes() > 4 &&
       get1OrigInsn(lastInsnAddr-4)->isBranchToFixedAddress(lastInsnAddr-4,
							    secondToLastInsnBranchDestAddr)) {
      // The second instruction contained the goods
      secondToLastInsnIsTheOne = true;
      assert(!containsAddr(secondToLastInsnBranchDestAddr));
   }

   if (lastInsnIsTheOne && secondToLastInsnIsTheOne)
      assert(false && "it can't be both");

   if (lastInsnIsTheOne)
      return lastInsnBranchDestAddr;

   if (secondToLastInsnIsTheOne)
      return secondToLastInsnBranchDestAddr;

   // neither
   return (kptr_t)-1;
}

pdvector<kptr_t> sparc_function::getCurrExitPoints() const {
   // TODO: If ExitBB (basicblock.h) was a "real" basic block in the sense of having
   // predecessor and successor ptrs, then we could quickly find the exit blocks
   // (though not the insn w/in such blocks): all of its predecessors.

   // Instead, we search for basic blocks having ExitBB as a successor.

   // XXX TODO: it's proper to have the search for exit blocks here, but once
   // an exit block has been identified, picking the particular exit point within
   // that block should, arguably, be left to code in basicblock.C.  Of course,
   // such a routine would have to be passed in the fnCode_t, but note that
   // basicblock.C's getExitPoint() already does just that.  Therefore, go ahead
   // and MAKE THIS CHANGE XXX

   pdvector<kptr_t> result;

   const_iterator bb_start = begin();
   const_iterator bb_iter = bb_start;
   const_iterator bb_finish = end();

   for (; bb_iter != bb_finish; ++bb_iter) {
      const basicblock_t *bb = *bb_iter;

      basicblock_t::ConstSuccIter succ_iter = bb->beginSuccIter();
      const basicblock_t::ConstSuccIter succ_finish = bb->endSuccIter();

      assert(succ_iter != succ_finish && "a bb with no successors is not allowed");
      // too harsh?  No, I don't think it is.

      for (; succ_iter != succ_finish; ++succ_iter) {
         const bbid_t succ_bb_id = *succ_iter;
         if (succ_bb_id == basicblock_t::interProcBranchExitBBId) {
            // Interprocedural ba,a (no delay slot)?
            const kptr_t lastInsnAddr = bb->getEndAddrPlus1() - 4;
            instr_t *lastInsn = get1OrigInsn(lastInsnAddr);

            kptr_t lastInsnDestAddr;
            if (lastInsn->isBranchToFixedAddress(lastInsnAddr, lastInsnDestAddr)) {
               assert(!containsAddr(lastInsnDestAddr && "expected interproc"));
               result += lastInsnAddr;
               continue;
            }

            assert(bb->getNumInsnBytes() > 4);
            const kptr_t secondToLastInsnAddr = lastInsnAddr - 4;
            sparc_instr *secondToLastInsn = (sparc_instr*)get1OrigInsn(secondToLastInsnAddr);

            kptr_t secondToLastInsnDestAddr;
            if (secondToLastInsn->isBranchToFixedAddress(secondToLastInsnAddr,
							 secondToLastInsnDestAddr)) {
               assert(!containsAddr(secondToLastInsnDestAddr &&
                                    "expected interproc"));
               result += secondToLastInsnAddr;
               continue;
            }
         }
         else if (succ_bb_id == basicblock_t::xExitBBId) {
            // Has xExitBBId as a successor; this is definitely an exit block!
            // But *how* does it exit?  Normally? (ret; restore or retl;nop)
            // Tail call? (call; restore or other variants)
            // Interprocedural branch? (No, that was handled above.)
            // Interprocedural fall-thru?
            // Other?...

            // We used to be a little brain-dead here, assuming that the exit point
            // was always endAddr+1-8.  The -8 is at best curious, since it pretty much
            // assumes that each basic block ends in a delayed control transfer such
            // as ret;restore or retl;nop.  That doesn't work for some kinda of exits,
            // such as:
            // 1) ba,a inter-procedural branch (no delay slot)
            // 2) interprocedural fall-through
            // 
            // XXX TODO: The solution is to correctly (re-?)
            // parse the basic block to see exactly how it ends (i.e. to identify the
            // above cases).  Then, a sensible calculation can be made.
            // 
            // Until then, we'll go ad-hoc approaches, such as checking the function's
            // blocksHavingInterProcFallThru[] (to handle one such case listed above).

            const bbid_t bbid = bb_iter - bb_start;

            // Interprocedural fall-thru?
            if (std::find(blocksHavingInterProcFallThru.begin(),
                     blocksHavingInterProcFallThru.end(), bbid) !=
                blocksHavingInterProcFallThru.end()) {
               // This block ends in an interprocedural fall-through.
               // WARNING OF INCORRECT IMPLEMENTATION:
               // Interproc fall-thru could also happen in the not-taken part of
               // a conditional branch.
               // THIS ISN'T DETECTED CORRECTLY RIGHT NOW.
               kptr_t addr = bb->getEndAddr() + 1 - 4;
               result += addr;
               continue;
            }

            // Done or retry?
            const kptr_t lastInsnAddr = bb->getEndAddrPlus1() - 4;
            sparc_instr *lastInsn = (sparc_instr*)get1OrigInsn(lastInsnAddr);
            sparc_instr::sparc_cfi lastInsn_cfi;
            lastInsn->getControlFlowInfo(&lastInsn_cfi);

            if (lastInsn_cfi.sparc_fields.isDoneOrRetry) {
               // Found an exit point
               result += lastInsnAddr;
               continue;
            }

            // All other kinds of exit points take place with a delayed control
            // transfer instruction, I believe.  (Darn, no assert.  It would take
            // a while and itself be prone to error.  For example, we'd have to
            // check for ret;restore, v9return;nop, retl;nop, call;restore, other
            // kinds of tail calls, and who knows what else.

            kptr_t addr = bb->getEndAddr() + 1 - 8;
            assert(containsAddr(addr));
            // too harsh?  Not harsh enough?  (Also assert in same basic block?)
            result += addr;
            continue;
         }
      }
   }

   // NOTE: Sorry, it's not safe to assert that result.size() > 0, even though
   // it holds 99% of the time.  The classic legitimate example that will
   // always make this true is that idle() never returns.
   // The cases where result.size()==0, not necessarily correctly are:
   // 1) An infinite loop on purpose.  idle(), fsflush(), sched(), background(),
   //    freebs(), qwriter_outer_thread(), hotplug_daemon(), thread_reaper(),
   //    pageout(), pageout_scanner(), seg_pasync_thread(), mod_uninstall_daemon(),
   //    ufs_thread_delete(), ufs_thread_idle(), ufs_thread_hlock(),
   //    ufsfx_thread_fix_failures(), svc_run(), door_unref(),
   //    door_unref_kernel(), trans_roll(), nfs_async_start(),
   //    ksyms_update_thread(), and other such background threads that run forever...
   // 
   // 2) unix/intr_thread_exit(): calls swtch(), which doesn't return.
   //    This is a general case that we should try to handle.  Can we calculate
   //    this property?
   //    Another example would be anything that calls panic(), though in practice
   //    we are able to find at least one other exit point in such routines.
   //    That doesn't excuse the fact that we're not incorrectly flagging the call to
   //    panic as an exit point, though!

   // Can we assert an upper limit on result.size()?  Certainly not; a function can
   // have a zillion normal retl;nop return sequences.

   return result;
}

void sparc_function::getMemUsage(unsigned &forBasicBlocks,
				 unsigned &forFns_OrigCode,
				 unsigned &forLiveRegAnalysisInfo,
				 unsigned &num1Levels,
				 unsigned &num2Levels,
				 unsigned &numOtherLevels
				 ) const {
   forBasicBlocks += (sizeof(sparc_basicblock) * basic_blocks.size()) +
                     (sizeof(basicblock_t*) * basic_blocks.capacity());
   forBasicBlocks += sizeof(bbid_t) * sorted_blocks.capacity();
   forBasicBlocks += sizeof(bbid_t) * unsorted_blocks.capacity();
   forBasicBlocks += sizeof(bbid_t) * blocksHavingInterProcFallThru.capacity();

   const_iterator bbiter = begin();
   const_iterator bbfinish = end();
   for (; bbiter != bbfinish; ++bbiter) {
      const basicblock_t *bb = *bbiter;
      if (bb->hasMoreThan2Succs())
         forBasicBlocks += sizeof(bbid_t) * bb->numSuccessors();
      if (bb->hasMoreThan2Preds())
         forBasicBlocks += sizeof(bbid_t) * bb->numPredecessors();
   }

   forFns_OrigCode += origCode.getMemUsage_exceptObjItself();

   forLiveRegAnalysisInfo += sizeof(liveRegisters); // just 12 bytes
   forLiveRegAnalysisInfo += liveRegisters.capacity() * sizeof(liveRegInfoPerBB);
      // still not too much
   pdvector<liveRegInfoPerBB>::const_iterator iter = liveRegisters.begin();
   pdvector<liveRegInfoPerBB>::const_iterator finish = liveRegisters.end();
   for (; iter != finish; ++iter) {
      const liveRegInfoPerBB &info = *iter;
      forLiveRegAnalysisInfo += info.getMemUsage_exceptObjItself();

      if (info.numLevels() == 1)
         ++num1Levels;
      else if (info.numLevels() == 2)
         ++num2Levels;
      else
         ++numOtherLevels;
   }
}

#ifdef _KERNINSTD_

void sparc_function::parseCFG(const fnCode_t &iCode,
	   // The idea here is to pass *at least* all of the
	   // code (+ embedded data perhaps..think of jump
	   // tables) of this function, so origCode[] can be 
	   // quickly filled in.
           // *After parsing*, we will trim the chunks as 
	   // appropriate to account for the *actual* range 
	   // of the fn (as determined by the basic block
           // addresses).  However, be careful not to trim 
	   // away useful fn data, such as jump table data 
	   // stuff, which won't appear to be within any bb
           // bounds and thus is tempting to trim!
           // NOTE: no need to say which of "iCode"'s chunks 
	   // are the entry chunk; we can figure that out from 
	   // this->entryAddr, which has already been set.
			      const pdvector<unsigned> &chunkNdxsNotToTrim,
           // these will be the exceptions to the above rule, 
	   // enabling to avoid, for example, trimming away 
	   // useful jump table data from "origCode".
			      dictionary_hash<kptr_t,bool> &interProcJumps,
			      pdvector<hiddenFn> &hiddenFns,
			      bbParsingStats &theBBParsingStats
			      ) {
   basicblock_t *newbb = new sparc_basicblock(entryAddr, (bbid_t)-1);
      // explicit ctor call.  (bbid_t)-1 indicates that this bb has no parent.
   const bbid_t entry_bb_id = addBlock_new(newbb);

   assert(origCode.isEmpty());
   origCode.ownerCopy(iCode);
      // NOTE: since iCode could have been way too big, we should keep an
      // eye out on trimming this later on, after parsing has completed!
      // We don't want to waste memory!

   const simplePath pathFromEntryBB; // a custom allocator is called for

   try {
      const sparc_kmem *km = new sparc_kmem(*this);
         // a nice fast ctor (uses our origInsns[] without making a copy of it.)
      cfgCreationOracle *ptheCFGCreationOracle = 
	cfgCreationOracle::getCfgCreationOracle(*this,
						interProcJumps,
						hiddenFns,
						km,
						verbose_fnParse,
						verbose_hiddenFns,
						theBBParsingStats);
      parse(entry_bb_id, pathFromEntryBB, ptheCFGCreationOracle);
      delete ptheCFGCreationOracle;
   }
   catch (function_t::parsefailed) {
      // clean up the function...fry its basic blocks, but keep its name, startAddr, etc.
      fry(true); // verbose
      throw function_t::parsefailed();
   }
   catch (instr_t::unimplinsn) {
      fry(true); // verbose
      throw function_t::parsefailed();
   }
   catch (instr_t::unknowninsn) {
      fry(true); // verbose
      throw function_t::parsefailed();
   }
   catch (...) {
      cout << "class sparc_function caught an UNKNOWN exception!" << endl;
      fry(true); // verbose
      throw function_t::parsefailed();
   }

   // Now we need to max out each chunk.
   // We'll keep a record of each chunk of the function's code, and ipmax using the
   // endAddr of each basic block.
   dictionary_hash<kptr_t, unsigned> updatedChunkSizes(addrHash4);
   fnCode_t::const_iterator citer = iCode.begin();
   fnCode_t::const_iterator cfinish = iCode.end();
   for (; citer != cfinish; ++citer)
      if (citer->numBytes() > 0) { // don't forget this line
         if (std::find(chunkNdxsNotToTrim.begin(), chunkNdxsNotToTrim.end(),
                  (unsigned)(citer - iCode.begin())) !=
             chunkNdxsNotToTrim.end()) {
            // We must not trim this chunk.  So initialize updatedChunkSizes for
            // this chunk to be the present size
            updatedChunkSizes.set(citer->startAddr, citer->numBytes());
         }
         else
            updatedChunkSizes.set(citer->startAddr, 0); // 0 --> chunk size for now
      }
   
   const_iterator bbiter = begin();
   const_iterator bbfinish = end();
   for (; bbiter != bbfinish; ++bbiter) {
      const basicblock_t *bb = *bbiter;
      // Find the start addr of the chunk that it belongs to.
      fnCode_t::const_iterator citer = iCode.findChunk(bb->getStartAddr(), false);
         // false --> can't be sure it's start of a chunk
      assert(citer != iCode.end());
      
      const kptr_t chunkStartAddr = citer->startAddr;
      assert(bb->getStartAddr() >= chunkStartAddr);
      assert(bb->getEndAddr() < chunkStartAddr + citer->numBytes());
      
      // Now that we have chunkStartAddr, we can update "updatedChunkSizes"
      unsigned &theChunkSize = updatedChunkSizes.get(chunkStartAddr);
      ipmax(theChunkSize, (uint32_t)(bb->getEndAddrPlus1() - chunkStartAddr));

      // While we're busy making sure that fnCode doesn't contain too much
      // unnecessary stuff outside the range of basic blocks, we can assert
      // the opposite already holds: that we don't have a case where the fn code
      // isn't big enough to hold any basic block:
      assert(origCode.enclosesAddr(bb->getStartAddr()));
      assert(origCode.enclosesAddr(bb->getEndAddr())); // not bb.getEndAddrPlus1()
   }

   origCode.shrinkChunksIfAppropriate(updatedChunkSizes.keysAndValues());

   // Sort the basic blocks, and free up memory used for unsorted_blocks:
   makeSorted();
   assert(unsorted_blocks.size() == 0);
   unsorted_blocks.zap();
      // free up memory that wasn't freed from a prior shrink() or clear()
}

void sparc_function::parse(bbid_t bb_id, 
			   const simplePath &pathFromEntryBB,
			   parseOracle *theParseOracle) const {
   // No need to put the following calculation under the ParseOracle, I think,
   // because it would remain the same.
   kptr_t stumbleOntoAt = (kptr_t)-1;
   const bbid_t firstBBIdGrStartAddr =
      searchForFirstBlockStartAddrGThan(basic_blocks[bb_id]->getStartAddr());
   if (firstBBIdGrStartAddr != (bbid_t)-1) {
      const basicblock_t *stumbleOntoBB = basic_blocks[firstBBIdGrStartAddr];
      stumbleOntoAt = stumbleOntoBB->getStartAddr();
      assert(stumbleOntoAt > basic_blocks[bb_id]->getStartAddr() &&
             "function<>::searchForFirstBlockStartAddrGThan() returned <=");
   }

   kptr_t currAddr = basic_blocks[bb_id]->getStartAddr();

   kptr_t currNonControlFlowCodeChunkStartAddr = currAddr;
   unsigned currNonControlFlowCodeChunkNumInsns = 0;

   // Get the codeChunk for this basic block
   fnCode_t::const_iterator chunk_iter = origCode.findChunk(currAddr, false);
   assert(chunk_iter != origCode.end());

   const fnCode_t::codeChunk &theCodeChunk = *chunk_iter;
   const sparc_kmem *km = new sparc_kmem(*this);

   while (true) {
      // loop until the basic block ends

      // Have we stumbled onto the next function? ("interprocedural stumble"?)
      // Example: resume() stumbles onto _resume_from_idle() in the kernel.
      if (!theCodeChunk.enclosesAddr(currAddr) && !containsAddr(currAddr)) {
         // not in the current chunk, nor in any other chunk of this function,
         // so it's definitely an interprocedural stumble.  (The second clause
         // is needed, in the case that 2 chunks of the same function happen
         // to be right next to each other -- quite possible when outlining.)

         processNonControlFlowCode
            (bb_id,
             currNonControlFlowCodeChunkStartAddr,
             currNonControlFlowCodeChunkNumInsns,
             theParseOracle);

         theParseOracle->processInterproceduralFallThru(bb_id);
         break; // we're all done creating this basic block
      }
      
      // Have we stumbled onto an existing basic block?
      if (currAddr == stumbleOntoAt) {
         processNonControlFlowCode
            (bb_id,
             currNonControlFlowCodeChunkStartAddr,
             currNonControlFlowCodeChunkNumInsns,
             theParseOracle);

         theParseOracle->processStumbleOntoExistingBlock(bb_id,
                                                        firstBBIdGrStartAddr,
                                                        pathFromEntryBB);
         break; // we're all done creating this basic block
      }
      
      try {
         instr_t *instr = theCodeChunk.enclosesAddr(currAddr) ? 
	   instr_t::getInstr(*(theCodeChunk.get1Insn(currAddr))) 
	   : km->getinsn(currAddr);
         //Note that we used getInstr above, because km->getinsn
	 //always returns a pointer we should destroy

         kptr_t instrAddr = currAddr;
         currAddr += instr->getNumBytes();
            // increment now, so if an exception is thrown, it still takes effect.
            // Make sure to *not* use currAddr anywhere below this code...

         // get control flow properties of instr
         sparc_instr::sparc_cfi *cfi = new sparc_instr::sparc_cfi();
         instr->getControlFlowInfo(cfi);

	 if (!cfi->fields.controlFlowInstruction) {
	   // common case; good thing the following code is cheap:
	   ++currNonControlFlowCodeChunkNumInsns;
           delete cfi;
           delete instr;
	 }
         else {
	    processNonControlFlowCode
               (bb_id,
                currNonControlFlowCodeChunkStartAddr,
                currNonControlFlowCodeChunkNumInsns,
                theParseOracle);

            const kptr_t origInstrAddr = instrAddr;

            const bool keepParsingSameBlock =
               cfanalysis_t::handleControlFlowInstr(bb_id,
                                                    (sparc_instr*)instr,
                                                    instrAddr,
                                                    cfi,
                                                    pathFromEntryBB,
                                                    stumbleOntoAt,
                                                    km,
                                                    theParseOracle);

	    if (!keepParsingSameBlock)
               break; // all done with this basic block if false is returned

            // Was instrAddr modified by the call to handleControlFlowInstr()?
            // If so, then they're telling us something: continue parsing there.
            if (instrAddr != origInstrAddr)
               currAddr = instrAddr;

            // reset:
            assert(currNonControlFlowCodeChunkNumInsns == 0);
            currNonControlFlowCodeChunkStartAddr = currAddr; // not instrAddr
         }
      }
//        catch (fnCodeObjects::RelocationForThisCodeObjNotYetImpl) {
//           cout << "function parse: caught RelocationForThisCodeObjNotYetImpl; rethrowing" << endl;
//           throw;
//        }
      // if a parsefailed() was thrown, we don't bother catching it, since we'll
      // just want to re-throw it.  Similar for code object creation exceptions.
      // So we only need to catch instruction type exceptions, and convert them
      // to parsefailed(), right?
      catch (instr_t::unimplinsn) {
         throw parsefailed();
      }
      catch (instr_t::unknowninsn) {
         throw parsefailed();
      }
//        catch (...) {
//           cout << "function parse: caught ... exception; throwing parsefailed" << endl;
//           throw parsefailed();
//        }
   }

   delete(km);
}

#endif /* _KERNINSTD_ */
