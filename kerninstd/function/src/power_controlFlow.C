// power_controlFlow.C

#ifdef _KERNINSTD_

#include "power_controlFlow.h"
#include "power_kmem.h"
#include "simplePathSlice.h"
#include "funkshun.h"
#include "powerTraits.h"
#include "power_misc.h" // readFromKernelInto()
#include <algorithm> // STL's binary_search()
#include "util/h/popc.h" // ari_popc()
#include "simplePath.h"
#include "out_streams.h"
#include "parseOracle.h"
#include "power_cfgCreationOracle.h"
#include "moduleMgr.h"

// See funkshun.h for open issues and outright bugs in the control flow graph
// parsing.  Put simply, we need to better handle interprocedural branches
// (put them on edges), and we need to do a better job handling delay slot
// instructions that are also the destination of some other branch.

extern bool verbose_fnParse;
typedef basicblock_t::bbid_t bbid_t;

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


static bool handleBctrAsSimpleJumpTable(bbid_t bb_id,
                                          power_instr *instr,
                                          kptr_t jumpInsnAddr,
                                          const simplePath &pathFromEntryBB,
                                             // doesn't yet include curr bb
                                          const kmem *km,
                                          parseOracle *theParseOracle) {
   typedef function_t::bbid_t bbid_t;

   //Many (most? all?) jump tables are stored in .data section on 
   //POWER.  The code that acesses the jump table looks something
   //like this:
   //      l       r3,1112(r2)   #r3 here = addr1
   //      cal     r0,-1(r31)    #r0 here is offset inside jump table
   //      rlinm   r4,r0,2,27,29
   //      cmpli   0,r0,7
   //      lx      r0,r3,r4
   //      l       r3,1116(r2)   #r3 here = addr2
   //      add     r0,r0,r3
   //      bgt     0x804
   //      mtctr   r0 
   //      bctr
   //     
   // Note that the calculation going on here is 
   // jump addr = addr1 + addr2 + offset * 4
   // Of course offset will vary (is some sort of parameter). What we need to
   // discover from this sequence is the start and end of the jump table.  
   // The start is simple: it's addr1 + addr2, and we can get those by
   // doing a backwards slice on the register that was moved into counter 
   // (r0 above), and loading the offsets off TOC (r2 always points to TOC).  
   // The end of the jump table is a bit of a problem.  It's tempting
   // to simply look for the cmpli instruction.  However, this gets 
   // complicated fast (what if there are multiple cmpli instructions?).
   // Further, in some cases the comparison may not be in the preceding
   // basic block.  Nor will it be in the slice.  Instead, we take an 
   //approach similar to what is done  on SPARC: go through the jump table, 
   // and look for addresses that are either not inside the function, 
   // or are inside  existing
   // basic block.  The case of multiple jump tables within the
   // function also needs to be handled.

   const function_t *fn = theParseOracle->getFn();
   power_reg_set regsToSlice(power_reg::ctr); //slice on the ctr

     simplePathSlice_t theSlice(*fn,
                                simplePath(pathFromEntryBB, bb_id),
                                regsToSlice, false,
                                0);

   theSlice.beginBackwardsIteration();
   if (theSlice.isEndOfIteration())
      return false; // bw slice of jmp stmt empty

   kptr_t addr1 = 0, addr2 = 0;
   int r2refs = 0;

   //find TOCPtr
   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;
   std::pair<pdstring, const function_t*> modAndFn = theModuleMgr.findModAndFn
			(fn->getEntryAddr(), true); //true-->startOnly
   const loadedModule &mod = *theModuleMgr.find(modAndFn.first);
   kptr_t TOCPtr = mod.getTOCPtr();
   
   kptr_t startOffset = 0;
   dictionary_hash<kptr_t,bool>  paramInsns(addrHash4);
   unsigned numIterations=0;
   //go through the slice looking for r2 references.
   //is this too ad-hoc? Perhaps.  However, it is guaranteed
   //to work correctly, since any r2 references in the slice on
   // CTR must be forming part of the address to jump table.
   while ( ! theSlice.isEndOfIteration() ) {

      if (paramInsns.defines(theSlice.getCurrIterationInsnAddr())) {
         theSlice.advanceIteration();
         numIterations++;
         continue;
      }
  
      power_instr *currSliceInstr = (power_instr *)theSlice.getCurrIterationInsn();
     
      long TOCOffset;
      
      if (currSliceInstr->isRotateLeftWordImmediate()) {
         power_instr::registerUsage ru;
         currSliceInstr->getRegisterUsage(&ru);
         //This is the register that contains the parameter specifying the number of jump table
         //entry.  We do not need to analyze (in fact, this exercise is bound to be futile).
         power_reg_set paramSet = *(ru.definitelyUsedBeforeWritten);
         paramSet  -= power_reg::cr0;
         assert(paramSet.count() == 1);
         assert(paramInsns.size() == 0); //there should be only one parameter

         //    pair <unsigned, unsigned> currBBIdAndOffset = theSlice.getCurrIterationBlockIdAndOffset();
         //simplePath startPath(pathFromEntryBB, bb_id);
         //simplePathSlice_t  parameterSlice (*fn, 
         //                                 simplePath(startPath, currBBIdAndOffset.first), 
         //                                 paramSet, false,
         //                                 currBBIdAndOffset.second/4);
         simplePathSlice_t parameterSlice(theSlice, numIterations + 1, false, paramSet);
         
         parameterSlice.beginBackwardsIteration();
         while (!parameterSlice.isEndOfIteration()) {
            paramInsns[parameterSlice.getCurrIterationInsnAddr()] = true;
            parameterSlice.advanceIteration();
         }
      }
 
      if (currSliceInstr->isIntegerLoadWithOffset(&TOCOffset)) {
         power_instr::registerUsage ru;
         currSliceInstr->getRegisterUsage(&ru);
       
         // Check for r2 reference
         if (ru.definitelyUsedBeforeWritten->count() > 0 &&
             ru.definitelyUsedBeforeWritten->existsIntReg(2) ) {
            if (r2refs == 0) { //first reference
               addr1 = TOCPtr + TOCOffset;
            }
            else if (r2refs == 1) { //second reference
               addr2 = TOCPtr + TOCOffset;
            } else {  //problem: more than 2 references
               dout << "More than two references to r2 (TOC pointer) while" 
                    << " parsing a jump table for instruction at "
                    <<tohex(jumpInsnAddr)<<endl;
               //return false;
            }
            
            
            r2refs++;
         } else if (ru.definitelyUsedBeforeWritten->count() > 0){
            //offset off the start of the Jump Table
            if (!startOffset) 
               startOffset = TOCOffset;
            else 
               dout<<"WARNING:  duplicate offset off the start of the Jump Table detected!"<<endl;
         }
      }
      theSlice.advanceIteration();
      numIterations++;
   }
   if (!r2refs)
      return false; //no r2 references 

   assert (addr1); //this one better be something other than 0

   instr_t *p = km->getinsn(addr1); 
   kptr_t ptr1 =  p->getRaw();
   delete p;

   kptr_t ptr2 = 0;
   if (addr2) {
      instr_t *p2 = km->getinsn(addr2);
      ptr2 =  p2->getRaw();
      delete p2;
   }

   kptr_t jumpTableStartAddr;
   kptr_t jumpTableOffsetStart;
   if (ptr1 > ptr2) {
      jumpTableStartAddr = ptr1 + startOffset;
      jumpTableOffsetStart = ptr2;
   } else {
      jumpTableStartAddr = ptr2 + startOffset;
      jumpTableOffsetStart = ptr1;
   }
   
      

   // //Now it's time to calculate the jump table address: dereference addr1 and addr2, and then add.
//    kptr_t jumpTableStartAddr =   km->getinsn(addr1)->getRaw();
//    if (addr2)
//       jumpTableStartAddr += km->getinsn(addr2)->getRaw();
      
   ((power_cfgCreationOracle *)theParseOracle)->preparePowerJumpTable(bb_id, instr, jumpInsnAddr,
                                         jumpTableStartAddr);
   // How to handle a jump table: the tricky part is that we don't know where
   // the jump table ends.  So we process the first jump table entry
   // (as the first successor to this bb) recursively.  Then, in general, if
   // the next would-be jump table entry is the start of some basic block, then
   // we stop; otherwise we recursively process it as the 2d successor to this bb.
   // It all continues...until the next would-be jump table entry falls on top of
   // an existing basic block or the next known fn.


   if (verbose_fnParse)
      cout << "Jump table (power) begins at addr "
           << addr2hex(jumpTableStartAddr) << endl;


   unsigned jumpTableNdx = 0;
   while (true) {
      kptr_t nextJumpTableEntry = jumpTableStartAddr + 4*jumpTableNdx;
      
      instr_t *nextTarget = km->getinsn(nextJumpTableEntry);
      kptr_t nextJumpTableTarget =  jumpTableOffsetStart +  nextTarget->getRaw();
      delete nextTarget;

      // We detect the end of jmp table by a jmp table entry that
      // appears to be garbage (i.e jumps into another function

      // Is the jump table entry garbage (bad address)? 
      // Reminder: during parsing, chunks are if anything too large 
      //(they'll get trimmed when done parsing), so if the following 
      //test is false, then the jump table entry is certainly garbage.
      if (!fn->containsAddr(nextJumpTableTarget)) {
         break; //done 
      }
      assert(nextJumpTableEntry % 4 == 0);
      assert(fn->containsAddr(nextJumpTableTarget));
      
      //Maybe should change the name of this method to be less
      //sparc-specific, but it does precisely what we need on power.
      theParseOracle->process1SimpleJumpTable32Entry(bb_id, // uniquely id's 
						           // the jmp table
                                                     nextJumpTableTarget,
                                                     pathFromEntryBB);
      
      jumpTableNdx++;
   } // while (true)

   assert(jumpTableNdx); //we should have gone through at least one iteration here.

//     cout << fn->getPrimaryName().c_str()
//          << "SIMPLE-32; insnAddr=" << addr2hex(jumpInsnAddr)
//          << "; jmpTabData starts at " << addr2hex(jumpTableStartAddr) << endl;
   
   // Done processing the jump table
   theParseOracle->completeSimpleJumpTable32(bb_id, jumpTableStartAddr);
   
   return true; // true --> we successfully found a jump table
}


static bool handleCallInstr(bbid_t bb_id,
                            instr_t *callInstr,
                            kptr_t &instrAddr,
                            // NEW: A reference.  If returning true, modify this
                            // vrble s.t. it's the addr of the insn where parsing
                            // should continue
                            const power_instr::power_cfi *cfi,
                            const simplePath & pathFromEntryBB,
                               // next known bb startaddr, or (kptr_t)-1
                            const  kmem * /* km */,
                            parseOracle *theParseOracle) {
   // called by handleControlFlowInstr when it detects a call. 
   // Returns true iff the basic block parsing should be continued;
   // false if bb has ended.  POWER call instructions is simply a 
   // a branch and link instruction.  This can be a conditional 
   // instruction.  Other call-like instructions (interprocedural 
   // jumps) are handled elsewhere.
   // 
   //TODO: what if the callee does not return, i.e. returns for
   // us somehow?
   
   const function_t *fn = theParseOracle->getFn();

   // quick assertion checks
   assert(cfi->fields.controlFlowInstruction);
   assert (cfi->destination != power_instr::controlFlowInfo::regRelative); 
   //this has to contain an immediate addr
   
   kptr_t destAddr;
   if (cfi->destination == power_instr::controlFlowInfo::pcRelative)
      destAddr = instrAddr + cfi->offset_nbytes;
   else //destaddr == absolute
      destAddr = cfi->offset_nbytes;
   
   if (fn->containsAddr(destAddr)) {
      // intra-procedural call!  Perhaps recursion; perhaps just obtaining pc.
      
      const bool justObtainingPC = (destAddr == instrAddr + 4);
      
      const bool recursion = (destAddr == fn->getEntryAddr());
      
      // If just obtaining PC, then we treat as an unconditional branch.
      // If recursion, then we treat like a conditional branch (i.e. there will
      // be both taken [recursion] and not taken [fall through; end of recursion]
      // cases).  
      
      if (justObtainingPC) {
         unsigned callInstrNumBytes =  callInstr->getNumBytes();
         theParseOracle->processCallToObtainPC(bb_id, callInstr,
                                               instrAddr,
                                               false, //delaySlostInSameBlock
                                               NULL//delaySlotInsn
                                               );
         
         // Since returning true, adjust instrAddr as appropriate:
         instrAddr += callInstrNumBytes;
        
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
            //
            //****************POWER NOTE **************************
            //Above is written about sparc code, but probably applies to 
            //power as well.  I leave this in -Igor. 

            theParseOracle->noteHiddenIntraProceduralFn(destAddr);
            // No "return" here; from here on, pretend it was a normal interproc call
         }
         else if (recursion) {
            //
//              cout << "Recursion in fn " 
//                   << addr2hex(fn->getEntryAddr())
//                   << " "
//                   << fn->getPrimaryName().c_str()
//                   << endl;

//This case is disabled on sparc.  I wonder why. Seems
// reasonable to me.  Note that even if this is a 
//conditional branch, the code will work, since it takes
//care of both taken and not taken case. -Igor
            
            theParseOracle->processCallRecursion(bb_id, destAddr, callInstr,
                                                instrAddr,
                                                 false, NULL, //no delay slots
                                                pathFromEntryBB);
            delete cfi;
	    return false;
               // done with this basic block (fall-thru case was recursively parsed)
         }
         else
            assert(false && "unknown case -- intraprocedural call");
   } // intra-procedural call

   unsigned callInstrNumBytes =  callInstr->getNumBytes();
   // An interprocedural call:
   if (cfi->fields.isAlways) {
      //unconditional branch
    ((power_cfgCreationOracle *)theParseOracle)->
       processTruePowerCall(bb_id, callInstr,
                            instrAddr,
                            destAddr);
                                              
       delete cfi;
       if (bbEndsOnANormalCall)
          return false; //finished with this bb
       else {
          // Since returning true, adjust instrAddr as appropriate:
          instrAddr += callInstrNumBytes;
          return true; //continue parsing this basic block
      }
   } else { //conditional branch (or at least one that depends on CTR)
       ((power_cfgCreationOracle *)theParseOracle)->
          processCondPowerCall(bb_id, callInstr,
                               instrAddr,
                               destAddr,
                               pathFromEntryBB);
                                          
      delete cfi;
      return false; //finished with this bb
   }

   assert(0);
   return true; // placate compiler viz. return type
}

static bool
handleReturnInstr(bbid_t bb_id, // no basicblock & since refs are too volatile
                  instr_t *insn, kptr_t instrAddr,
                  const power_instr::power_cfi *cfi,
                  const simplePath &pathFromEntryBB,
                  const kmem * /* km */,
                  parseOracle *theParseOracle) {
   //this is branch to link register.  We are assuming jump tables
   //and indirect calls are done using CTR--true of any optimized
   //code on AIX.
   if (cfi->fields.isAlways) {
      //unconditional return--the common case
   theParseOracle->processReturn(bb_id, insn, instrAddr,
                                 false, NULL //delay slot stuff
                                 );
   } else { //conditional branch
        ((power_cfgCreationOracle*)theParseOracle)->
           processCondReturn(bb_id, insn, instrAddr, pathFromEntryBB);
   }
   delete cfi;
   return false; // false --> bb is finished; don't fall through
}

static bool handleJumpToFixedAddr(bbid_t bb_id,
                                  power_instr *instr,
                                  kptr_t jmpInsnAddr,
                                  const simplePath &pathFromEntryBB,
                                  // doesn't yet include curr bb
                                  const power_instr::power_cfi *cfi,
                                  parseOracle *theParseOracle) {
   //this handles unconditional branch (i-form) to fixed address.   
   
   power_instr::registerUsage ru;
   instr->getRegisterUsage(&ru);

   if (ru.definitelyWritten->count() > 0) //if we are linking, should be treated as a call 
      assert(false);

   kptr_t the_addr = 0;
   
   assert (cfi->destination != power_instr::controlFlowInfo::regRelative); 
   //this has to contain an immediate addr

   if (cfi->destination == power_instr::controlFlowInfo::pcRelative)
      the_addr = jmpInsnAddr + cfi->offset_nbytes;
   else //destaddr == absolute
      the_addr = cfi->offset_nbytes;

   if (theParseOracle->getFn()->containsAddr(the_addr)) {
      // Intra-procedural jump.  So treat it like a jump table entry, or a normal
      //We don't need to slice on power, but let's create an empty one
      //to make the compiler happy
      const function_t *fn = theParseOracle->getFn();
      power_reg_set regsToSlice(power_reg_set::empty); 
      simplePathSlice_t theSlice(*fn,
                                 simplePath(pathFromEntryBB, bb_id),
                                 regsToSlice, false,
                                 0);

      // unconditional branch.
      theParseOracle->processIntraproceduralJmpToFixedAddr(bb_id, instr,
                                                          jmpInsnAddr,
                                                           false, //delay
                                                           NULL,   //slot stuff
                                                           the_addr,
                                                           theSlice,
                                                           pathFromEntryBB);
      delete cfi;
      return false; // false --> done with this basic block
   }
   else {
      // Okay, it's interprocedural.  An optimized call without a stack frame?
      theParseOracle->processInterproceduralJmpToFixedAddr(bb_id, instr,
                                                           false, //delay
                                                           NULL,   //slot stuff
                                                          the_addr,
                                                          pathFromEntryBB);
      delete cfi;
      return false; // false --> done with this basic block
   }

}
/*
static bool is64BitKernel()
{
    return (sizeof(kptr_t) == sizeof(uint64_t));
}
*/


// Most calls through a function pointer get translated into indirect jumps
// Our goal is to distinguish these cases from the jump tables.  There
// are two different ways to discover this, depending on whether the code is
// C (core kernel) or C++ (most of the modules).  In C, indirect calls
// use bctrl instruction (i.e. puts return address into link reg), thus 
// making it different from jump tables.  In C++, function pointer call
// is a "call" to a kind of a function that sets up the parameters and then
// uses plain bctr to jump, since the return address has already
// been saved as part of the original "call. Below is an example of
// a code sequence for C++ indirect call.

//      50:           bl      0xcb8           #call
//     ....
//     cb8:           l       r12,4(r2)
//     cbc:           st      r2,20(r1)
//     cc0:           l       r0,0(r12)
//     cc4:           l       r2,4(r12)
//     cc8:           mtctr   r0
//     ccc:           btctr
//     cd0:           .long 0x0
//
// Interestingly, 4 bytes of zeroes always follow such a chunk.  
// We use those zeroes as a clue that we are handling indirect
// call.  Note that zero is guaranteed to be an illegal instruction.
//Lastly, if all of the above fails, we check if there are any references
//to the TOC pointer in the slice on the CTR.  In case, there aren't any,
//we know this cannot be a jump table.
//NOTE: the above analysis is based on AIX, but appears to work on Linux
//      just as well.
static bool
handleBctrAsIndirectTailCall(bbid_t bb_id,
                             power_instr *instr,
                             kptr_t insnAddr,
                             const simplePath &pathFromEntryBB,
                             const kmem *km,
                             parseOracle *theParseOracle)
{
   instr_t *next = km->getinsn(insnAddr + 4);
   kptr_t nextInsn = next->getRaw();
   delete next;
   const function_t *fn = theParseOracle->getFn();

   if (!nextInsn ||   instr->isBranchToCounterAndLink() ) {
      theParseOracle->processIndirectTailCall(bb_id, instr, insnAddr,
                                              false, NULL /*ds stuff */ );
      //dout << "Identified jmp at " << addr2hex(insnAddr) 
      //     << " as an indirect  call\n";
      return true; // for now
   }

   power_reg_set regsToSlice(power_reg::ctr); //slice on the ctr
   simplePathSlice_t theSlice(*fn,
                              simplePath(pathFromEntryBB, bb_id),
                              regsToSlice, false,
                              0);
   
   theSlice.beginBackwardsIteration();
   unsigned r2refs = 0;
   while ( ! theSlice.isEndOfIteration() ) {
      power_instr *currSliceInstr = (power_instr *)theSlice.getCurrIterationInsn();
      long TOCOffset;

      if (currSliceInstr->isIntegerLoadWithOffset(&TOCOffset)) {
         power_instr::registerUsage ru;
         currSliceInstr->getRegisterUsage(&ru);
       
         // Check for r2 reference
         if (ru.definitelyUsedBeforeWritten->count() > 0 &&
             ru.definitelyUsedBeforeWritten->existsIntReg(2) ) {
            r2refs++;
            break;
         }
      }
      theSlice.advanceIteration();
   }
   if (r2refs) //probably a jump table
      return false;

   theParseOracle->processIndirectTailCall(bb_id, instr, insnAddr,
                                            false, NULL /*ds stuff */ );
   //dout << "Identified jmp at " << addr2hex(insnAddr) 
   //     << " as an indirect  call\n";
   return true; // for now
}
   

//take care of conditional branches (non-linking, i.e. we do not
// expect this to be a call.
static bool
handleConditionalBranchInstr(bbid_t bb_id,
                             bool branchAlways,
                             kptr_t instrAddr,
                             power_instr *i,
                             power_instr::power_cfi *cfi,
                             const simplePath &pathFromEntryBB,
                             // doesn't include curr bb
                             const kmem * /* km */,
                             parseOracle *theParseOracle) {

   assert (cfi->destination != power_instr::controlFlowInfo::regRelative); 
   //this has to contain an immediate addr
   
   kptr_t destAddr;
   if (cfi->destination == power_instr::controlFlowInfo::pcRelative)
      destAddr = instrAddr + cfi->offset_nbytes;
   else //destaddr == absoluteImm
      destAddr = cfi->offset_nbytes;

   theParseOracle->processCondBranch(bb_id, branchAlways, false, //branchNever
                                     i, cfi, instrAddr,
                                     false, false, NULL, //ds stuff
                                     instrAddr + 4, //fall-through
                                     destAddr,
                                     pathFromEntryBB);

   return false; // don't continue the basic block; we've finished parsing it.
}

bool power_controlFlow::handleControlFlowInstr(bbid_t bb_id,
					       power_instr *instr,
					       kptr_t &instrAddr,
					       // iff returning true, we modify this
					       power_instr::power_cfi *cfi,
					       const simplePath &pathFromEntryBB,
					       // doesn't yet include curr bb
                                               kptr_t /* stumbleOntoAddr  */,
                      const kmem *km ,
                      parseOracle * theParseOracle) {
  // Return true if the basic block should be continued 
  //(call instr -- though some
  // changes have been made as of 11/2000 that may make it return false 
  //anyway), or false if the control flow instruction has ended the basic 
  //block (e.g. ret).
  // If (and only if) returning true, we adjust "instrAddr" so that it's the 
  //address of the instruction at which parsing should continue.  
  //It'll presumably be the value on entry to this routine plus 4
  
  //handle the special case of invalid instructions first
  if (cfi->power_fields.isBug) {
    //fake instruction placed here by BUG() macro
     theParseOracle->processDoneOrRetry(bb_id, instr, instrAddr);
     return false;
  }
  // Handle calls to other functions.
   if  (cfi->fields.isCall)
      return handleCallInstr(bb_id,
                             instr, instrAddr,
                             cfi, pathFromEntryBB,
                             km,
                             theParseOracle);
   
  // Handle return--could be conditional (branch to link register)
   if  (cfi->fields.isRet)
      return handleReturnInstr(bb_id,
                               instr, instrAddr, cfi,
                               pathFromEntryBB,
                               km, theParseOracle);
   
  //unconditional branch to immediate address
  if ( (cfi->destination != instr_t::controlFlowInfo::regRelative) &&
       cfi->fields.isAlways )
     return  handleJumpToFixedAddr(bb_id,
				instr, instrAddr, pathFromEntryBB, cfi,
			   theParseOracle);

  //conditional branch to immediate address
  if (! cfi->fields.isAlways && 
      cfi->destination != instr_t::controlFlowInfo::regRelative)
     return handleConditionalBranchInstr(bb_id,
					cfi->fields.isAlways,
					instrAddr,
					instr,
					cfi,
					pathFromEntryBB,
					km,
					theParseOracle);
  
  
  if ( (cfi->destination == instr_t::controlFlowInfo::regRelative) 
       && cfi->fields.isAlways &&
       ( *(cfi->destreg1) == (reg_t &)power_reg::ctr) )        {
     // Handle jump tables and indirect calls.  
     if (handleBctrAsIndirectTailCall(bb_id,
                                      instr, instrAddr, pathFromEntryBB,
                                       km, theParseOracle) ) {
        delete cfi;
        return false; //done with this basic block
     } else //if it did not parse as an indirect call, it better be a jmp table
        if (handleBctrAsSimpleJumpTable(bb_id, instr, instrAddr,  
                                        pathFromEntryBB, km, 
                                        theParseOracle) ) {
	   delete cfi;	
           return false; //done with this basic block
        } else {
           dout << "WARNING: could not analyze branch to CTR at " 
                <<tohex(instrAddr)<< endl;
           delete instr;
	   delete cfi;
	   throw function_t::parsefailed();
           return false; 
        }
  }
  
  //This leaves out conditional branches to CTR.  It's not clear 
  //whether they are ever used.  Must write code to handle them
  //as soon as they are encountered.  
  assert(false);
  return false; // placate compiler
}

#endif /* _KERNINSTD_ */
