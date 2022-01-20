// launchSite.C

#include "launchSite.h"
#include "util/h/minmax.h"
#include "util/h/mrtime.h"
#include "util/h/out_streams.h"
#include "kernelDriver.h"
#include <algorithm> // stl's find()
#include "abi.h"
#include "freeUpCodePatchInternalEvent.h"
#include "loop.h" // doOneTimeInternalEventInTheFuture()

bool LaunchSite::verboseTimingsSummary = false;
bool LaunchSite::verboseTimingsDetailed = false;

bool justTesting = false;

void LaunchSite::setVerboseTimingsSummary() {
   verboseTimingsSummary = true;
   verboseTimingsDetailed = false;
}
void LaunchSite::setVerboseTimingsDetailed() {
   verboseTimingsSummary = false;
   verboseTimingsDetailed = true;
}

extern abi *theKernelABI;

static unsigned snippetsHash(const pair<unsigned,unsigned> &key) {
   unsigned clientUniqueId = key.first;
   unsigned spliceReqId = key.second;
   
   unsigned result = 0;
   
   while (clientUniqueId) {
      result = result + (result << 1) + (clientUniqueId & 0x3);
      clientUniqueId >>= 2;
   }
   while (spliceReqId) {
      result = result + (result << 1) + (spliceReqId & 0x3);
      spliceReqId >>= 2;
   }

   return result;
}

static uint64_t msec2nsec(uint64_t msecs) {
   return msecs * 1000000;
}

LaunchSite::LaunchSite(kptr_t ilaunchAddr,
                       bool isterilizedByCompetingReplaceFn,
                       const moduleMgr &iModuleMgr,
                       kernelDriver &iKernelDriver,
                       patchHeap &ipatchHeap,
                       SpringBoardHeap &iSpringBoardHeap) :
            theKernelDriver(iKernelDriver),
            thePatchHeap(ipatchHeap),
            theSpringBoardHeap(iSpringBoardHeap),
            theModuleMgr(iModuleMgr),
            fn(theModuleMgr.findFn(ilaunchAddr, false)), // false --> not start-only
            launchAddr(ilaunchAddr),
            originsn(NULL),
	    cfi(NULL),
            intRegsAvailBeforeInsn(NULL),
            intRegsAvailAfterInsn(NULL),
            sterilizedByCompetingReplaceFn(isterilizedByCompetingReplaceFn),
            preInsnSnippets(snippetsHash),
            preReturnSnippets(snippetsHash)
{
   originsn = instr_t::getInstr(*fn.get1OrigInsn(launchAddr));
   intRegsAvailBeforeInsn = regset_t::getRegSet(regset_t::empty);// for now
   intRegsAvailAfterInsn = regset_t::getRegSet(regset_t::empty);// for now

#ifdef sparc_sun_solaris2_7
   // Is this instruction a delay slot?  If so, then barf (don't splice at
   // delay slot instructions)
   const function_t *previnsn_fn = theModuleMgr.findFnOrNULL(ilaunchAddr-4, false);
   if (previnsn_fn != NULL) {
      instr_t *previnsn = previnsn_fn->get1OrigInsn(ilaunchAddr-4);
      sparc_instr::sparc_cfi previnsn_cfi;
      previnsn->getControlFlowInfo(&previnsn_cfi);
      
      if (previnsn_cfi.fields.controlFlowInstruction &&
          previnsn_cfi.delaySlot != instr_t::dsNone)
         assert(false && "don't splice at a delay slot instruction please");
   }
   cfi = new sparc_instr::sparc_cfi();
#elif defined(rs6000_ibm_aix5_1)
   cfi = new power_instr::power_cfi();
#elif defined (i386_unknown_linux2_4)
   cfi = new x86_instr::x86_cfi();
#else
   assert(false);
#endif

   originsn->getControlFlowInfo(cfi);
   installedLRAndR = NULL;
   installedPatchAddr = 0;

#ifndef i386_unknown_linux2_4 
   // On x86, we always want a save/restore to be emitted due to the 
   // possibility we're generating 64-bit code that uses pseudoregisters, 
   // so we keep the intRegsAvail{Before/After}Insn sets empty

   theModuleMgr.getDeadRegsBeforeAndAfterInsnAddr(fn,
                                                  launchAddr,
                                                  *intRegsAvailBeforeInsn,
                                                  *intRegsAvailAfterInsn,
                                                  false); // not verbose
   // For safety, above call always treats %fp and %sp as live (not dead)
   assert(!intRegsAvailBeforeInsn->exists(regset_t::getAlwaysLiveSet()));
   
   makeJustIntRegs(intRegsAvailBeforeInsn);
   makeJustIntRegs(intRegsAvailAfterInsn);
#endif
}

LaunchSite::~LaunchSite() {
   // assert that unInstrument() was called.  A nice, super-strong assert.  I bet
   // we'll see this one triggered a bunch of times while working out kinks.
   assert(installedPatchAddr == 0 && "forgot to unInstall LaunchSite?");
   assert(installedLRAndR == NULL);

   // Another super-strong assertion:
   assert(snippets.size() == 0);
   assert(preInsnSnippets.size() == 0);
   assert(preReturnSnippets.size() == 0);

   if(originsn) delete originsn;
   if(cfi) delete cfi;
   if(intRegsAvailBeforeInsn) delete intRegsAvailBeforeInsn;
   if(intRegsAvailAfterInsn) delete intRegsAvailAfterInsn;
}

void LaunchSite::instrument() {
   // A private method (believe it or not)

   if (sterilizedByCompetingReplaceFn) {
      cout << "LaunchSite::instrument() at " << launchAddr
           << " doing nothing since sterilizedByCompetingReplaceFn is true" << endl;
      return;
   }

   // Would prefer to create the LRAndR in the ctor, not here.
   // But we can't since creating the LRAndR needs to know there are any preReturn
   // snippets, which can change at runtime as snippets are added & removed.
   assert(snippets.size() == preInsnSnippets.size() + preReturnSnippets.size());
   if (snippets.size() == 0) {
      unInstrument();
      return;
   }

   const mrtime_t allocateNeededCodePatchSizeStartTime =
      (verboseTimingsSummary || verboseTimingsDetailed) ? getmrtime() : 0;
   
   // Step 1: create LRAndR (needs to know: any preReturn snippets?)
   const bool anyPreReturnSnippets = preReturnSnippets.size();

 
#ifdef sparc_sun_solaris2_7
   // create_LRAndR demands only int reg sets for params.  Assert it now.
   assert(*(sparc_reg_set*)intRegsAvailBeforeInsn == 
	  (*(sparc_reg_set*)intRegsAvailBeforeInsn & sparc_reg_set::allIntRegs));
   assert(*(sparc_reg_set*)intRegsAvailAfterInsn == 
	  (*(sparc_reg_set*)intRegsAvailAfterInsn & sparc_reg_set::allIntRegs));
#elif defined(i386_unknown_linux2_4)
   assert(*(x86_reg_set*)intRegsAvailBeforeInsn == 
          (*(x86_reg_set*)intRegsAvailBeforeInsn & x86_reg_set::allIntRegs));
   assert(*(x86_reg_set*)intRegsAvailAfterInsn == 
	  (*(x86_reg_set*)intRegsAvailAfterInsn & x86_reg_set::allIntRegs));

#endif

   //let's make a copy of all pointed-to objects, so that LRAndR doesn't
   //get to refer to our own ever-changing data structures
   instr_t *LRAndR_originsn = instr_t::getInstr(*originsn);
   regset_t *LRAndR_intRegsAvailBeforeInsn = regset_t::getRegSet(*intRegsAvailBeforeInsn);
   regset_t *LRAndR_intRegsAvailAfterInsn = regset_t::getRegSet(*intRegsAvailAfterInsn);
   
   LRAndR *beingBuiltLRAndR =
      LRAndR::create_LRAndR(launchAddr, fn, LRAndR_originsn, cfi,
			    LRAndR_intRegsAvailBeforeInsn,
			    LRAndR_intRegsAvailAfterInsn,
			    anyPreReturnSnippets,
			    true // verbose flag
			    // verbose if, e.g., a particular kind of tail call
			    // unwinding is presently unimplemented
			    );

   // Step 2: Ask LRAndR for free regs that preInsn/preReturn snippets will execute in.
   regset_t *intRegsForExecutingPreInsnSnippets = 
      regset_t::getRegSet(*beingBuiltLRAndR->getInitialIntRegsForExecutingPreInsnSnippets());
   regset_t *intRegsForExecutingPreReturnSnippets = 
      regset_t::getRegSet(*beingBuiltLRAndR->getInitialIntRegsForExecutingPreReturnSnippets());
      // we say 'Initial' because it doesn't take into account the
      // possibility of more regs via emitting a save

   // Step 2.5: If preReturn isn't a meaningful placement for this 
   // kind of  LRAndR, then changed the placement from preReturn to 
   // preInsn for the snippets.
   pdvector<codeSnippet*> actualPreInsnSnippets;
   pdvector<codeSnippet*> actualPreReturnSnippets;

   // The original preInsn snippets go first
   for (dictionary_hash<pair<unsigned,unsigned>,codeSnippet*>::const_iterator 
	    iter = preInsnSnippets.begin(); 
	iter != preInsnSnippets.end(); ++iter) {
       actualPreInsnSnippets += iter.currval();
   }

   bool convert = (!beingBuiltLRAndR->preReturnHasMeaning());

   for (dictionary_hash<pair<unsigned,unsigned>,codeSnippet*>::const_iterator 
	    iter = preReturnSnippets.begin(); 
	iter != preReturnSnippets.end(); ++iter) {
       if (convert) {
	   // IMPORTANT: we rely on placing all "converted" preInsnSnippets
	   // AFTER normal preInsnSnippets. Otherwise, if the function is 
	   // empty (entry == return) the stop routine would be emitted 
	   // in front of the start one 
	   actualPreInsnSnippets += iter.currval();
       }
       else {
	   // no conversion is necessary
	   actualPreReturnSnippets += iter.currval();
       }
   }
   assert(preInsnSnippets.size() + 
	  preReturnSnippets.size() == snippets.size());
   assert(actualPreInsnSnippets.size() + 
	  actualPreReturnSnippets.size() == snippets.size());
   
   // Step 2.75: Get conservative guesses for num insns & num regs needed for all of the
   // snippets (preInsn/preReturn).
   // Unfortunately, this can't be moved to addCodeSnippet/removeCodeSnippet, because
   // it's only when the LRAndR is picked (this routine) that some snippets change
   // from preReturn to preInsn.
   unsigned maxInsnBytesForPreInsnSnippets_nosaveneeded = 0;
   unsigned maxInsnBytesForPreInsnSnippets_ifsaveneeded = 0;
   unsigned maxNumInt32RegsForPreInsnSnippets = 0;
   unsigned maxNumInt64RegsForPreInsnSnippets = 0;
   for (pdvector<codeSnippet*>::const_iterator 
	    iter = actualPreInsnSnippets.begin(); 
	iter != actualPreInsnSnippets.end(); ++iter) {
      codeSnippet *theSnippet = (*iter);

      const unsigned maxbytes_nosaveneeded =
         theSnippet->getMaxNumInsnBytes(true, // preinsn
                                        false // nosave
                                        );
      if (maxInsnBytesForPreInsnSnippets_nosaveneeded == -1U ||
          maxbytes_nosaveneeded == -1U)
         maxInsnBytesForPreInsnSnippets_nosaveneeded = -1U; // useful sentinel
      else
         maxInsnBytesForPreInsnSnippets_nosaveneeded += maxbytes_nosaveneeded;


      const unsigned maxbytes_ifsaveneeded =
         theSnippet->getMaxNumInsnBytes(true, // preinsn
                                        true // if save needed
                                        );
      if (maxInsnBytesForPreInsnSnippets_ifsaveneeded == -1U ||
          maxbytes_ifsaveneeded == -1U)
         maxInsnBytesForPreInsnSnippets_ifsaveneeded = -1U;
      else
         maxInsnBytesForPreInsnSnippets_ifsaveneeded += maxbytes_ifsaveneeded;

      ipmax(maxNumInt32RegsForPreInsnSnippets, theSnippet->getMaxNumInt32RegsNeeded());
      ipmax(maxNumInt64RegsForPreInsnSnippets, theSnippet->getMaxNumInt64RegsNeeded());
   }

   unsigned maxInsnBytesForPreReturnSnippets_nosaveneeded = 0;
   unsigned maxInsnBytesForPreReturnSnippets_ifsaveneeded = 0;
   unsigned maxNumInt32RegsForPreReturnSnippets = 0;
   unsigned maxNumInt64RegsForPreReturnSnippets = 0;
   for (pdvector<codeSnippet*>::const_iterator 
	    iter = actualPreReturnSnippets.begin(); 
	iter != actualPreReturnSnippets.end(); ++iter) {
      codeSnippet *theSnippet = (*iter);

      const unsigned maxbytes_nosaveneeded = 
         theSnippet->getMaxNumInsnBytes(false, // prereturn
                                        false // nosave
                                        );
      if (maxInsnBytesForPreReturnSnippets_nosaveneeded == -1U ||
          maxbytes_nosaveneeded == -1U)
         maxInsnBytesForPreReturnSnippets_nosaveneeded = -1U; // useful sentinel
      else
         maxInsnBytesForPreReturnSnippets_nosaveneeded += maxbytes_nosaveneeded;


      const unsigned maxbytes_ifsaveneeded =
         theSnippet->getMaxNumInsnBytes(false, // prereturn
                                        true // if save needed
                                        );
      if (maxInsnBytesForPreReturnSnippets_ifsaveneeded == -1U ||
          maxbytes_ifsaveneeded == -1U)
         maxInsnBytesForPreReturnSnippets_ifsaveneeded = -1U;
      else
         maxInsnBytesForPreReturnSnippets_ifsaveneeded += maxbytes_ifsaveneeded;
      
      ipmax(maxNumInt32RegsForPreReturnSnippets,
            theSnippet->getMaxNumInt32RegsNeeded());
      ipmax(maxNumInt64RegsForPreReturnSnippets,
            theSnippet->getMaxNumInt64RegsNeeded());
   }

   // Step 3: Using step 2.75's calculations, now we can make the
   // determination: is a save/restore needed?  If so, add to the number of
   // insn bytes needed.  And in either event, fill in the important
   // variables int32RegsForPreInsnSnippets and int64RegsForPreInsnSnippets, 
   // which we'll heretofore use instead of intRegsForExecutingPreInsnSnippets.

   bool preInsnSaveRestoreNeeded = false; // for now
   unsigned maxInsnBytesForPreInsnSnippets =
      maxInsnBytesForPreInsnSnippets_nosaveneeded; // for now...
   regset_t *intRegsToSaveForPreInsnSnippets = NULL;
   if (!theKernelABI->enough_int_regs(intRegsForExecutingPreInsnSnippets,
                                      maxNumInt32RegsForPreInsnSnippets,
                                      maxNumInt64RegsForPreInsnSnippets)) {
      // a save/restore *will* be needed!
      preInsnSaveRestoreNeeded = true;

      assert(maxInsnBytesForPreInsnSnippets_ifsaveneeded != -1U);
         // check for this sentinel; should catch "isDummy" relocatableCode's being
         // queried for numbytesneeded.
      
      
      unsigned numBytesForSaveRestore = 
	directToMemoryEmitter_t::getNumBytesForEmitSaveRestore (
                        maxNumInt32RegsForPreInsnSnippets, maxNumInt64RegsForPreInsnSnippets);

      maxInsnBytesForPreInsnSnippets = maxInsnBytesForPreInsnSnippets_ifsaveneeded + 
	      numBytesForSaveRestore;
#ifdef sparc_sun_solaris2_7   
      assert(!intRegsForExecutingPreInsnSnippets->exists(sparc_reg::g7));
#endif
      intRegsForExecutingPreInsnSnippets =
        directToMemoryEmitter_t::getSaveRegSet(intRegsForExecutingPreInsnSnippets,
                                               &intRegsToSaveForPreInsnSnippets, 
					       maxNumInt32RegsForPreInsnSnippets + 
					       maxNumInt64RegsForPreInsnSnippets);
      *intRegsForExecutingPreInsnSnippets -= regset_t::getAlwaysLiveSet();
#ifdef sparc_sun_solaris2_7
      assert(!intRegsForExecutingPreInsnSnippets->exists(sparc_reg::g7));
#endif
      // Now we'll be able to successfully fill in int32RegsForPreInsnSnippets and
      // int64RegsForPreInsnSnippets.

      if (!theKernelABI->enough_int_regs(intRegsForExecutingPreInsnSnippets,
                                         maxNumInt32RegsForPreInsnSnippets,
                                         maxNumInt64RegsForPreInsnSnippets))
         assert(false && "not enough int regs for preinsn snippet, even after a save!");
   }
   
   // ...do the same thing, for prereturn snippets.
   bool preReturnSaveRestoreNeeded = false; // for now
   unsigned maxInsnBytesForPreReturnSnippets =
      maxInsnBytesForPreReturnSnippets_nosaveneeded; // for now...
   regset_t *intRegsToSaveForPreReturnSnippets = NULL;
   if (!theKernelABI->enough_int_regs(intRegsForExecutingPreReturnSnippets,
                                      maxNumInt32RegsForPreReturnSnippets,
                                      maxNumInt64RegsForPreReturnSnippets)) {
      // prereturn save/restore needed
      preReturnSaveRestoreNeeded = true;

      assert(maxInsnBytesForPreReturnSnippets_ifsaveneeded != -1U);
         // check for this sentinel; should catch "isDummy" relocatableCode's being
         // queried for numbytesneeded.

      unsigned numBytesForSaveRestore = 
	directToMemoryEmitter_t::getNumBytesForEmitSaveRestore (
                        maxNumInt32RegsForPreReturnSnippets, maxNumInt64RegsForPreReturnSnippets);

      maxInsnBytesForPreReturnSnippets = maxInsnBytesForPreReturnSnippets_ifsaveneeded + 
	      numBytesForSaveRestore;
#ifdef sparc_sun_solaris2_7
      assert(!intRegsForExecutingPreReturnSnippets->exists(sparc_reg::g7));
#endif
      intRegsForExecutingPreReturnSnippets =
	directToMemoryEmitter_t::getSaveRegSet(intRegsForExecutingPreReturnSnippets,
                                               &intRegsToSaveForPreReturnSnippets, 
					       maxNumInt32RegsForPreReturnSnippets +
					       maxNumInt64RegsForPreReturnSnippets);

      *intRegsForExecutingPreReturnSnippets -= regset_t::getAlwaysLiveSet();
    
#ifdef sparc_sun_solaris2_7
      assert(!intRegsForExecutingPreReturnSnippets->exists(sparc_reg::g7));
#endif
      // Now we'll be able to successfully fill in int32RegsForPreReturnSnippets and
      // int64RegsForPreReturnSnippets.

      if (!theKernelABI->enough_int_regs(intRegsForExecutingPreReturnSnippets,
                                         maxNumInt32RegsForPreReturnSnippets,
                                         maxNumInt64RegsForPreReturnSnippets))
         assert(false && "not enough int regs for prereturn snippet, even after a save!");
   }

   // Step 4: Allocate code patch
   unsigned beingBuiltPatchAllocatedNumBytes = maxInsnBytesForPreInsnSnippets +
      maxInsnBytesForPreReturnSnippets +
      beingBuiltLRAndR->conservative_numInsnBytesForRAndR();
   const mrtime_t allocateNeededCodePatchSizeTotalTime =
      (verboseTimingsSummary || verboseTimingsDetailed) ?
      getmrtime()-allocateNeededCodePatchSizeStartTime : 0;

   kptr_t beingBuiltPatch = 0; // for now...
   try {
      const mrtime_t allocateCodePatchStartTime = 
         (verboseTimingsSummary || verboseTimingsDetailed) ? getmrtime() : 0;

#ifdef sparc_sun_solaris2_7
      // If launchAddr is in (or below) the nucleus, try to allocate
      // patch space in the nucleus. If the nucleus is full, the allocator
      // will try the below-nucleus region and then the kmem32 region.
      const bool tryToAllocateCodePatchInNucleus =
         theKernelDriver.isInNucleus(launchAddr) || 
	  theKernelDriver.isBelowNucleus(launchAddr);
//      const bool tryToAllocateCodePatchInNucleus = true; // TEMP FOR TIMING INFO
#elif defined(ppc64_unknown_linux2_4)
     //on ppc64, we'll call the kernel space (0xC*) the "nucleus" and
     //module space (0xD*) "non-nucleus"

      const bool tryToAllocateCodePatchInNucleus =
         theKernelDriver.isInNucleus(launchAddr); 

#else
      const bool tryToAllocateCodePatchInNucleus = false;
#endif
      
      beingBuiltPatch = thePatchHeap.alloc(beingBuiltPatchAllocatedNumBytes,
                                           tryToAllocateCodePatchInNucleus);
         // tryToAllocateCodePatchInNucleus==true --> patch heap tries, but
         // doesn't insist on, allocating within the nucleus

      // dout << "- creating launchSite : launchAddr<" << launchAddr 
// 	   << "> patch<addr=" << beingBuiltPatch << ",len="
//            << beingBuiltPatchAllocatedNumBytes << ">\n";

      dout << "- creating launchSite : launchAddr<" << addr2hex(launchAddr) 
	   << "> patch<addr=" << addr2hex(beingBuiltPatch) << ",len="
           << beingBuiltPatchAllocatedNumBytes << ">\n";

      const mrtime_t allocateCodePatchTotalTime =
         (verboseTimingsSummary || verboseTimingsDetailed) ? getmrtime()-allocateCodePatchStartTime : 0;

      // Step 5: Inform the LRAndR of the patch's start address
      beingBuiltLRAndR->
         registerPatchAddrAndCreateLauncher(beingBuiltPatch,
                                            instr_t::getInstr(*originsn)
                                            // when all done, restore to this
                                            // instruction (constant info
                                            // about this launch site)
                                            );
   
      // Step 6: create emitter

      // the patchHeap class no longer has an mmap interface, so we must mmap --
      // temporarily -- here and now.
      const mrtime_t mmapStartTime =
         (verboseTimingsSummary || verboseTimingsDetailed) ? getmrtime() : 0;
#ifdef sparc_sun_solaris2_7      
       void *mapped_ptr = theKernelDriver.map_for_rw(beingBuiltPatch,
                                              beingBuiltPatchAllocatedNumBytes,
                                              false);
         // false --> can't assert page-sized or page-aligned
#else
      //MJB FIXME - following is a kludge to avoid mmapped kernel memory
      void *mapped_ptr = malloc(beingBuiltPatchAllocatedNumBytes);
#endif
      const mrtime_t mmapTotalTime =
         (verboseTimingsSummary || verboseTimingsDetailed) ? getmrtime()-mmapStartTime : 0;
      
      directToMemoryEmitter_t *em = 
         directToMemoryEmitter_t::getDirectToMemoryEmitter(*theKernelABI,
                                                           (dptr_t)mapped_ptr, 
                                                           // in-kerninstd addr
                                                           beingBuiltPatch, 
                                                           // in-kernel addr
                                                           beingBuiltPatchAllocatedNumBytes);

      // Step 7: emit stuff before preinsn snippets
      beingBuiltLRAndR->emitStuffBeforePreInsnSnippets(*em, fn, cfi);
      if (actualPreInsnSnippets.size() > 0)
         //we need to save system registers, such as condtion register,
         //even if we are not saving any registers for execution
         em->emitSave(preInsnSaveRestoreNeeded, 
                      intRegsToSaveForPreInsnSnippets);
      // Step 8: preInsn snippets
      for (pdvector<codeSnippet*>::const_iterator
	       snippetiter = actualPreInsnSnippets.begin();
           snippetiter != actualPreInsnSnippets.end();
           ++snippetiter) {
	  const codeSnippet *theSnippet = *snippetiter;
	  theSnippet->relocate_and_emit(*em, true, // preInsn
					preInsnSaveRestoreNeeded);
      }

      // Step 9: emit stuff after preinsn snippets
      if (actualPreInsnSnippets.size() > 0)
         em->emitRestore(preInsnSaveRestoreNeeded, 
                         intRegsToSaveForPreInsnSnippets);
      beingBuiltLRAndR->emitStuffAfterPreInsnSnippets(*em, fn, cfi);

      // Step 10: emit stuff before prereturn snippets
      if (actualPreReturnSnippets.size() > 0)
         //we need to save system registers, such as condtion register,
         //even if we are not saving any registers for execution
         em->emitSave(preReturnSaveRestoreNeeded,
                      intRegsToSaveForPreReturnSnippets);
      
      // Step 11: preReturn snippets
      for (pdvector<codeSnippet*>::const_iterator
	       snippetiter = actualPreReturnSnippets.begin();
           snippetiter != actualPreReturnSnippets.end();
           ++snippetiter) {
         const codeSnippet *theSnippet = *snippetiter;
         theSnippet->relocate_and_emit(*em,
                                       false, // preReturn
                                       preReturnSaveRestoreNeeded);
      }

      // Step 12: emit stuff after prereturn snippets
      if (actualPreReturnSnippets.size() > 0)
         em->emitRestore(preReturnSaveRestoreNeeded,
                         intRegsToSaveForPreReturnSnippets);
      beingBuiltLRAndR->emitStuffAfterPreReturnSnippets(*em, fn, cfi);

      // Step 13: complete the emit
      em->complete();
#if defined(i386_unknown_linux2_4) || defined (ppc64_unknown_linux2_4)
      // MJB FIXME - following is a kludge to avoid using mmapped kernel memory
      theKernelDriver.poke_block(beingBuiltPatch, beingBuiltPatchAllocatedNumBytes, mapped_ptr);
#endif
      // Code patch has been written.
      
      // Step 13 1/2: unmap kernel memory
      const mrtime_t unmapStartTime =
         (verboseTimingsSummary || verboseTimingsDetailed) ? getmrtime() : 0;
#if defined(i386_unknown_linux2_4) || defined(ppc64_unknown_linux2_4)
      // MJB FIXME - following is a kludge to avoid using mmapped kernel memory
      free(mapped_ptr);
#else      
      theKernelDriver.unmap_from_rw(mapped_ptr, beingBuiltPatchAllocatedNumBytes);
#endif
      const mrtime_t unmapTotalTime =
         (verboseTimingsSummary || verboseTimingsDetailed) ? getmrtime()-unmapStartTime : 0;

      // Step 14: install launcher (will install springboard & do splice)
      const bool replacingAnExistingLauncher = (installedPatchAddr != 0);

      const mrtime_t installLauncherStartTime =
         verboseTimingsSummary ? getmrtime() : 0;
      // Note that if verboseTimingsDetailed is true, we instead go with
      // whatever beingBuiltLRAndR->installLauncher() will print out, which is
      // presumably more detailed.

      if (replacingAnExistingLauncher)
         beingBuiltLRAndR->
            installLauncherOverwritingExisting(justTesting,
                                               installedLRAndR->getLaunchersSplicingInsn(),
                                                  // other guy's curr installed insn
                                               installedLRAndR->getLaunchersWhenAllDoneRestoreToInsn(),
                                                  // other guy's restore to insn
                                               theKernelDriver,
                                               verboseTimingsDetailed);
      else
         beingBuiltLRAndR->
            installLauncherNotOverwritingExisting(justTesting,
                                                  theKernelDriver,
                                                  verboseTimingsDetailed);

      const mrtime_t installLauncherTotalTime = verboseTimingsSummary ? getmrtime()-installLauncherStartTime : 0;

      // OK, the new code is in place and executing even as we speak
      // Time to fry the old LRAndR and old patch, if applicable

      if (replacingAnExistingLauncher) {
         // installedPatchAddr, installedPatchNumBytes, installedLRAndR
         assert(installedLRAndR);
         installedLRAndR->
            unInstallLauncherAndFryAnotherAlreadyInstalled(justTesting,
                                                           theKernelDriver,
                                                           verboseTimingsDetailed);
         
         delete installedLRAndR;
         installedLRAndR = NULL; // help purify find mem leaks
         
         thePatchHeap.free(installedPatchAddr, installedPatchAllocatedNumBytes);
      }

      installedPatchAddr = beingBuiltPatch;
      installedPatchAllocatedNumBytes = beingBuiltPatchAllocatedNumBytes;
      installedPatchActuallyUsedNumBytes = em->numInsnBytesEmitted();
      assert(installedPatchActuallyUsedNumBytes <= installedPatchAllocatedNumBytes);
      assert(installedLRAndR == NULL);
      installedLRAndR = beingBuiltLRAndR;
      
      // All done splicing!

      delete em; // keep after last use above

      // Print some stats
      if (verboseTimingsSummary || verboseTimingsDetailed) {
         cout << "LaunchSite::instrument() timings:" << endl;
         cout << "calc code patch size: "
              << allocateNeededCodePatchSizeTotalTime / 1000
              << " usecs." << endl;
         cout << "allocate from thePatchHeap: "
              << allocateCodePatchTotalTime / 1000 << " usecs." << endl;
         cout << "mmap the code patch: "
              << mmapTotalTime / 1000 << " usecs." << endl;
         cout << "un-map the code patch: "
              << unmapTotalTime / 1000 << " usecs." << endl;
         if (verboseTimingsSummary) {
            cout << "install launcher total time: "
                 << installLauncherTotalTime / 1000 << " usecs" << endl;
         }
         else if (verboseTimingsDetailed) {
            cout << "install launcher: you already heard about that, in detail, I think"
                 << endl;
         }
      }
   } // try to allocate code patch from patch heap
   catch (patchHeap::AllocSizeTooBig) {
      throw NeedMorePatchSpace(); // perhaps a param: beingBuiltPatchNumBytes
   }
   catch (patchHeap::OutOfFreeSpace) {
      throw NeedMorePatchSpace(); // perhaps a param: beingBuiltPatchNumBytes
   }
}

void
LaunchSite::addCodeSnippetCommon(client_connection &theClient,
                                 unsigned spliceReqId,
                                 unsigned maxNumInt32RegsNeeded,
                                 unsigned maxNumInt64RegsNeeded,
                                 bool preInsn, // false --> preReturn
                                 const relocatableCode_t *code_preinsn_ifnosave,
                                 const relocatableCode_t *code_preinsn_ifsave,
                                 const relocatableCode_t *code_prereturn_ifnosave,
                                 const relocatableCode_t *code_prereturn_ifsave,
                                 bool instrumentNow) {
   codeSnippet *theSnippet = new codeSnippet(theClient, spliceReqId,
                                             maxNumInt32RegsNeeded,
                                             maxNumInt64RegsNeeded,
                                             code_preinsn_ifnosave,
                                             code_preinsn_ifsave,
                                             code_prereturn_ifnosave,
                                             code_prereturn_ifsave);
   assert(theSnippet);
   
   snippets += theSnippet;

   pair<unsigned, unsigned> theKey(theClient.getUniqueId(), spliceReqId);
   
   assert(!preInsnSnippets.defines(theKey));
   assert(!preReturnSnippets.defines(theKey));
   
   if (preInsn)
      preInsnSnippets.set(theKey, theSnippet);
   else
      preReturnSnippets.set(theKey, theSnippet);

   if (instrumentNow && !sterilizedByCompetingReplaceFn)
      instrument();
}

void
LaunchSite::addCodeSnippetPreInsn(client_connection &theClient,
                                  unsigned spliceReqId,
                                  unsigned maxNumInt32RegsNeeded,
                                  unsigned maxNumInt64RegsNeeded,
                                  const relocatableCode_t *code_ifnosave,
                                  const relocatableCode_t *code_ifsave,
                                  bool instrumentNow) {

   relocatableCode_t *rc1 = relocatableCode_t::getRelocatableCode(relocatableCode_t::_dummy);
   relocatableCode_t *rc2 = relocatableCode_t::getRelocatableCode(relocatableCode_t::_dummy);
   addCodeSnippetCommon(theClient, spliceReqId,
                        maxNumInt32RegsNeeded,
                        maxNumInt64RegsNeeded,
                        true, // preInsn
                        code_ifnosave, code_ifsave,
                        rc1,// prereturn code not needed
                        rc2,// prereturn code not needed
                        instrumentNow);
}

void
LaunchSite::addCodeSnippetPreReturn(client_connection &theClient,
                                    unsigned spliceReqId,
                                    unsigned maxNumInt32RegsNeeded,
                                    unsigned maxNumInt64RegsNeeded,
                                    const relocatableCode_t *code_prereturn_ifnosave,
                                    const relocatableCode_t *code_prereturn_ifsave,
                                    const relocatableCode_t *code_preinsn_ifnosave,
                                    const relocatableCode_t *code_preinsn_ifsave,
                                    bool instrumentNow) {
   addCodeSnippetCommon(theClient, spliceReqId,
                        maxNumInt32RegsNeeded,
                        maxNumInt64RegsNeeded,
                        false, // preReturn
                        code_preinsn_ifnosave,
                        code_preinsn_ifsave,
                        code_prereturn_ifnosave,
                        code_prereturn_ifsave,
                        instrumentNow);
}

bool LaunchSite::removeCodeSnippet(client_connection &theClient,
                                   unsigned spliceReqId,
                                   bool launchInstrumentNow) {
   // returns true iff there's nothing left at this launch site, orig code has
   // been put back in place, etc.
   
   pair<unsigned, unsigned> theKey = make_pair(theClient.getUniqueId(), spliceReqId);

   const bool inPreInsnSnippets = preInsnSnippets.defines(theKey);
   const bool inPreReturnSnippets = preReturnSnippets.defines(theKey);
   const bool inBoth = inPreInsnSnippets && inPreReturnSnippets;
   assert(inPreInsnSnippets || inPreReturnSnippets);
   assert(!inBoth);

   codeSnippet *theSnippet = inPreInsnSnippets ?
      preInsnSnippets.get_and_remove(theKey) :
      preReturnSnippets.get_and_remove(theKey);
   pdvector<codeSnippet*>::iterator iter = std::find(snippets.begin(), snippets.end(),
                                                   theSnippet);
   assert(iter != snippets.end());
   delete *iter;
   *iter = NULL;
   *iter = snippets.back();
   snippets.pop_back();

   assert(preInsnSnippets.size() + preReturnSnippets.size() == snippets.size());

   if (launchInstrumentNow)
      if (snippets.size() == 0) {
         if (!sterilizedByCompetingReplaceFn)
            unInstrument();
         return true;
      }
      else {
         if (!sterilizedByCompetingReplaceFn)
            instrument();
         return false;
      }
   else
      return false;
}

void LaunchSite::makeJustIntRegs(regset_t *theset) {
#ifdef sparc_sun_solaris2_7
   *(sparc_reg_set*)theset &= sparc_reg_set::allIntRegs;
   *(sparc_reg_set*)theset -= sparc_reg::g0;
#elif defined(i386_unknown_linux2_4)
   *(x86_reg_set*)theset &= x86_reg_set::allIntRegs;
#elif defined(ppc64_unknown_linux2_4)
   *(power_reg_set*)theset &= power_reg_set::allIntRegs;
#endif
}

void LaunchSite::unInstrument() {
   // A private method (believe it or not)

   assert(!sterilizedByCompetingReplaceFn &&
          "should not call unInstrument() when sterilizedByCompetingReplaceFn is true");
   
   assert(installedPatchAddr);
   assert(installedLRAndR);

   const mrtime_t unInstallLauncherStartTime =
      verboseTimingsSummary ? getmrtime() : 0;
   
   installedLRAndR->
      unInstallLauncherAndFryNoOtherAlreadyInstalled(justTesting,
                                                     theKernelDriver,
                                                     verboseTimingsDetailed);
      // restores original code

   if (verboseTimingsSummary) {
      const mrtime_t unInstallLauncherTotalTime =
         getmrtime() - unInstallLauncherStartTime;

      cout << "LaunchSite::unInstrument completed in "
           << unInstallLauncherTotalTime / 1000 << " usecs." << endl;
   }

   
   internalEvent *e = new freeUpCodePatchInternalEvent(getmrtime() + msec2nsec(3000),
                                                       thePatchHeap,
                                                       installedPatchAddr,
                                                       installedPatchAllocatedNumBytes,
                                                       verboseTimingsSummary ||
                                                          verboseTimingsDetailed);
      // I think that it's appropriate to print out verbose timing events
      // whether collecting summary timing info or whether collecting
      // detailed timing info.
   assert(e);

   doOneTimeInternalEventInTheFuture(e); // loop.C
   
//     thePatchHeap.free(installedPatchAddr, installedPatchAllocatedNumBytes);
//        // BUG: only safe to free (actually, free & reuse) if no thread is executing
//        // in this code!

   installedPatchAddr = 0;

   // In normal (all?) circumstances, this routine would only get called when
   // snippets.size() == 0, so the following is probably harmless but useless.
   // Note that since we want to keep snippets around if we're being sterilized
   // due to a competing replaceFn, this is NOT the appropriate routine to implement
   // sterilization.
   for (pdvector<codeSnippet*>::iterator iter = snippets.begin();
        iter != snippets.end(); ++iter) {
      delete *iter;
      *iter = NULL; // help purify find mem leaks
   }
   snippets.clear();
   preInsnSnippets.clear();
   preReturnSnippets.clear();

   delete installedLRAndR;
   installedLRAndR = NULL; // help purify find mem leaks
}

void LaunchSite::kerninstdIsGoingDown(kptr_t addr) {
   // kerninstd is dying; put back the original instruction NOW!

   if (installedLRAndR == NULL) {
      // perhaps due to sterilization, or perhaps just due to a normal uninstrument
      cout << "LaunchSite emergency unsplice for address " << addr2hex(addr)
           << " doing nothing since bookeeping shows no LRAndR" << endl;

      // pacify later dtor call:
      installedPatchAddr = 0;
   }
   else {
      unInstrument();
   }
}
