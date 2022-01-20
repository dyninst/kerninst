// vtimerMgr_ultrasparc_2_6.C
// Solaris2.6-specific routines of class vtimerMgr

#include "util/h/kdrvtypes.h"
#include "util/h/rope-utils.h"
#include "util/h/out_streams.h"
#include "moduleMgr.h"
#include "vtimerMgr.h"
#include "machineInfo.h"

#include <algorithm> // stl's sort
#include "sparc_instr.h"

static void add_intr_thread_points(pdvector<kptr_t> &switchout_splicepoints2,
                                   pdvector<kptr_t> &switchin_splicepoints2,
                                   const function_t &intr_thread_fn) {
   // There are 4 points in intr_thread that we want to instrument:
   // (note: don't confuse the terms "interrupted thread" with "interrupt thread";
   // they're quite different!)
   // 1) switchout of interrupted thread
   //    -- on entry to intr_thread()
   // 2) switchin of the interrupt thread
   //    -- just before the wrpr that precedes SERVE_INTR()
   // 3) switchout of the interrupt thread
   //    -- in the basic block containing jmp %l0 + 8: the *top* of that bb
   //       (before %g7 changes)
   // 4) switchin of the interrupted thread
   //    -- considering the same basic block as in (3) above: just before the exit
   //       (after %g7 has changed)
   pdvector<kptr_t> out_points, in_points;

   // 1) switchout at entry
   pdvector<kptr_t> entry_points = intr_thread_fn.getCurrEntryPoints();
   assert(entry_points.size() == 1);
   out_points += entry_points;
   assert(out_points.size() == 1);
   
   // 2) switchin before the wrpr preceding SERVE_INTR() macro
   // note that wrpr %g0, %l1, %pil is near the end of the entry basic block.
   const unsigned intr_thread_entry_bbid = intr_thread_fn.xgetEntryBB();
   const basicblock_t *intr_thread_fn_entry_bb =
      intr_thread_fn.getBasicBlockFromId(intr_thread_entry_bbid);

   bool found_wrpr_before_SERVE_INTR = false;
   for (kptr_t insnaddr = intr_thread_fn_entry_bb->getStartAddr();
        insnaddr < intr_thread_fn_entry_bb->getEndAddrPlus1(); ) {
      const sparc_instr *i = (const sparc_instr*)intr_thread_fn.get1OrigInsn(insnaddr);

      if (i->isWrPr()) {
         sparc_instr::sparc_ru ru;
         i->getRegisterUsage(&ru);
         if (ru.definitelyUsedBeforeWritten->count() == 2 &&
             ru.definitelyUsedBeforeWritten->exists(sparc_reg::g0) &&
             ru.definitelyUsedBeforeWritten->exists(sparc_reg::l1)) {
            // match
            assert(insnaddr <= intr_thread_fn_entry_bb->getEndAddrPlus1()-4);
            in_points += insnaddr;
            found_wrpr_before_SERVE_INTR = true;
            break;
         }
      }
      
      insnaddr += i->getNumBytes();
   }
   assert(found_wrpr_before_SERVE_INTR);
   
   // 3) switchout at top of the basic block containing jmpl %l0 + 8
   // 4) switchin at the exit of that basic block
   function_t::const_iterator intr_thread_bbfinish = intr_thread_fn.end();
   for (function_t::const_iterator intr_thread_bbiter = intr_thread_fn.begin();
        intr_thread_bbiter != intr_thread_bbfinish; ++intr_thread_bbiter) {
      const basicblock_t *bb = *intr_thread_bbiter;
      if (bb->getNumInsnBytes() < 8)
         // it must have at least 2 insns to be a candidate
         continue;

      basicblock_t::ConstSuccIter succiter = bb->beginSuccIter();
      basicblock_t::ConstSuccIter succfinish = bb->endSuccIter();
      if (succiter == succfinish)
         continue;
      unsigned succid = *succiter;
      ++succiter;
      if (succiter != succfinish)
         continue;
      if (succid == basicblock_t::xExitBBId) {
         // just 1 successor, and it's ExitBB.  So far it's a candidate.
         // See if it ends in jmpl %l0 + 8, <delay>
         
         const kptr_t bbAddrOfLastInsn = bb->getEndAddrPlus1() - 4;
         const kptr_t bbAddrOfSecToLastInsn = bbAddrOfLastInsn - 4;

         const sparc_instr *secToLastInsn = (const sparc_instr*)intr_thread_fn.get1OrigInsn(bbAddrOfSecToLastInsn);
         sparc_instr::sparc_ru ru;
         sparc_instr::sparc_cfi cfi;

         secToLastInsn->getInformation(&ru, NULL, NULL, &cfi, NULL);
         if (ru.definitelyWritten->count() != 1)
            // hmmm, a non-%g0 link register (1 is for %pc).  Not a candidate after all.
            continue;
         
         secToLastInsn->getControlFlowInfo(&cfi);
         if (cfi.fields.controlFlowInstruction &&
             cfi.fields.isJmpLink &&
             cfi.destination == instr_t::controlFlowInfo::regRelative &&
             *(sparc_reg*)cfi.destreg1 == sparc_reg::l0 && 
	     cfi.offset_nbytes == 8) {
            // bingo
            out_points += bb->getStartAddr();
            in_points += bbAddrOfSecToLastInsn;
            break; // out of the while loop iterating thru the basic blocks
         }
      }
      
      ++intr_thread_bbiter;
   }

   assert(out_points.size() == 2);
   assert(in_points.size() == 2);

   switchout_splicepoints2 += out_points;
   switchin_splicepoints2 += in_points;
}

// Find a place to insert the context switch-in code into the
// resume_from_idle() function. We want to do this at the end of the
// function, but before the interrupt level is lowered with the "wrpr
// %o0, 0, %pil" instruction. So we splice our code at the place of wrpr.
static void add_resume_from_idle_in(pdvector<kptr_t> &switchin_splicepoints1,
				    const function_t &resume_from_idle_fn)
{
   pdvector<basicblock_t*>::const_iterator bb_iter;
   pdvector<kptr_t> in_points;

   // Loop through all the instructions of resume_from_idle looking
   // for the "wrpr %o0, 0, %pil"
   for (bb_iter = resume_from_idle_fn.begin(); 
	bb_iter < resume_from_idle_fn.end();
	bb_iter++) {
      kptr_t insnaddr = (*bb_iter)->getStartAddr();
      while (insnaddr < (*bb_iter)->getEndAddrPlus1()) {
         const sparc_instr *ins = (const sparc_instr*)resume_from_idle_fn.get1OrigInsn(insnaddr);
	 if (ins->isWrPr()) {
	    sparc_instr::sparc_ru ru;
	    ins->getRegisterUsage(&ru);
	    if (ru.definitelyUsedBeforeWritten->count() == 1 &&
		ru.definitelyUsedBeforeWritten->exists(sparc_reg::o0) &&
		ru.definitelyWritten->count() == 1 &&
		ru.definitelyWritten->exists(sparc_reg::reg_pil) &&
		ru.maybeUsedBeforeWritten->count() == 0 &&
		ru.maybeWritten->count() == 0) {
	       in_points += insnaddr;
	    }
	 }
	 insnaddr += ins->getNumBytes();
      }
   }
   // Assert there is only one such point
   assert(in_points.size() == 1);
   switchin_splicepoints1 += in_points;
}

pair<
pair< pdvector<kptr_t>, pdvector<kptr_t> >, // category 1 out/in points
pair< pdvector<kptr_t>, pdvector<kptr_t> > // category 2 out/in points
>
vtimerMgr::obtainCSwitchOutAndInSplicePoints() {
   // a static method.

   // when we say category 1 vs. category 2:
   // category 1 points have lots of free regs
   // category 2 points have few or no free int regs

   extern machineInfo *theGlobalKerninstdMachineInfo;
   unsigned os_version = theGlobalKerninstdMachineInfo->getOSVersion();

   extern moduleMgr *global_moduleMgr;
   const moduleMgr &theModuleMgr = *global_moduleMgr;

   const loadedModule &unixmod = *theModuleMgr.find("unix");

   const function_t &swtch_fn =
      theModuleMgr.findFn(unixmod.find_1fn_by_name("swtch"), true);

   const function_t &swtch_to_fn =
      theModuleMgr.findFn(unixmod.find_1fn_by_name("swtch_to"), true);

   const function_t &swtch_from_zombie_fn =
      theModuleMgr.findFn(unixmod.find_1fn_by_name("swtch_from_zombie"), true);

   const function_t &resume_from_idle_fn =
      theModuleMgr.findFn(unixmod.find_1fn_by_name("_resume_from_idle"), true);

   const function_t &resume_from_intr_fn =
      theModuleMgr.findFn(unixmod.find_1fn_by_name("resume_from_intr"), true);

   const function_t &resume_from_zombie_fn =
      theModuleMgr.findFn(unixmod.find_1fn_by_name("resume_from_zombie"), true);

   const function_t &resume_fn =
      theModuleMgr.findFn(unixmod.find_1fn_by_name("resume"), true);

   const function_t &intr_thread_fn =
      theModuleMgr.findFn(unixmod.find_1fn_by_name("intr_thread"), true);

   // Step 1: find category 1 instrumentation points for cswitchout
   // 1) swtch(), just before calling resume_from_intr()
   // 2) swtch(), just before calling resume()
   // 3) swtch_to(), just before calling resume()
   // 4) swtch_from_zombie(), just before calling resume_from_zombie()

   pdvector<kptr_t> switchout_splicepoints1;
   switchout_splicepoints1 += swtch_fn.getCallsMadeTo_asOrigParsed(resume_from_intr_fn.getEntryAddr(), false); // false --> exclude interproc *branches*
   switchout_splicepoints1 += swtch_fn.getCallsMadeTo_asOrigParsed(resume_fn.getEntryAddr(), false); // false --> exclude interproc *branches*
   switchout_splicepoints1 += swtch_to_fn.getCallsMadeTo_asOrigParsed(resume_fn.getEntryAddr(), false); // false --> exclude interproc *branches*
   switchout_splicepoints1 += swtch_from_zombie_fn.getCallsMadeTo_asOrigParsed(resume_from_zombie_fn.getEntryAddr(), false); // false --> exclude interproc *branches*
   
   dout << "switch-out1 splice points:" << endl;
   for (pdvector<kptr_t>::const_iterator iter = switchout_splicepoints1.begin();
        iter != switchout_splicepoints1.end(); ++iter)
      dout << " " << addr2hex(*iter);
   dout << endl;

   // Step 2: find category 1 switchin points
   pdvector<kptr_t> switchin_splicepoints1;
   
   add_resume_from_idle_in(switchin_splicepoints1, resume_from_idle_fn);

   const unsigned in_splicepts1 = switchin_splicepoints1.size();
   switchin_splicepoints1 += resume_from_intr_fn.getCurrExitPoints();
   assert(switchin_splicepoints1.size() > in_splicepts1);

   dout << "switch-in1 splice points:" << endl;
   for (pdvector<kptr_t>::const_iterator iter = switchin_splicepoints1.begin();
        iter != switchin_splicepoints1.end(); ++iter)
      dout << " " << addr2hex(*iter);
   dout << endl;

   // Step 3: find category 2 switchout points
   pdvector<kptr_t> switchout_splicepoints2;
   pdvector<kptr_t> switchin_splicepoints2;

   // switch points in intr_thread():
   add_intr_thread_points(switchout_splicepoints2, switchin_splicepoints2,
                          intr_thread_fn);
   assert(switchout_splicepoints2.size() == 2);
   assert(switchin_splicepoints2.size() == 2);

   // Solaris 8 does not have the clk_thread handler, don't bother
   // instrumenting it there
   if (os_version == 6 || os_version == 7) {
       const function_t &clk_thread_fn =
	   theModuleMgr.findFn(unixmod.find_1fn_by_name("clk_thread"), true);

       // switch points in clk_thread_fn():
       // category 2 switch-out & -in points #3: within clk_thread_fn, find the basic block
       // containing "call genunix/clock"; instrument switchout at the top of this bb;
       // instrument switchin just before "call clock" (still within this bb).
       const loadedModule &genunixmod = *theModuleMgr.find("genunix");

       const function_t &clock_fn =
	   theModuleMgr.findFn(genunixmod.find_1fn_by_name("clock"), true);

       pdvector<kptr_t> calls_vec = clk_thread_fn.getCallsMadeTo_asOrigParsed(clock_fn.getEntryAddr(), false); // false --> exclude interproc *branches*
       assert(calls_vec.size() == 1);
       const kptr_t pointWhereClkThreadCallsClock = calls_vec[0];
   
       const unsigned call_to_clock_bbid = clk_thread_fn.searchForBB(pointWhereClkThreadCallsClock);
       const basicblock_t *call_to_clock_bb = clk_thread_fn.getBasicBlockFromId(call_to_clock_bbid);
   
       switchout_splicepoints2 += call_to_clock_bb->getStartAddr();
       switchin_splicepoints2 += pointWhereClkThreadCallsClock;
       assert(call_to_clock_bb->containsAddr(pointWhereClkThreadCallsClock));
   
       assert(switchout_splicepoints2.size() == 3);
       assert(switchin_splicepoints2.size() == 3);
   
       // category 2 switch-out & -in points #3: within clk_thread_fn, find the first
       // (by instruction address) of the two basic blocks ending in "jmp %l0 + 8; nop".
       // (It will have 6 total insns; the other one will have 2).
       const pdvector<kptr_t> clk_thread_fn_exitpoints = clk_thread_fn.getCurrExitPoints();
       assert(clk_thread_fn_exitpoints.size() > 0);

       pdvector<kptr_t> clk_thread_fn_jmpl_exitpoints;
       for (pdvector<kptr_t>::const_iterator iter = clk_thread_fn_exitpoints.begin();
	    iter != clk_thread_fn_exitpoints.end(); ++iter) {
	   const kptr_t addr = *iter;
	   const sparc_instr *insn = (const sparc_instr*)clk_thread_fn.get1OrigInsn(addr);
      
	   sparc_instr::sparc_cfi cfi;
	   insn->getControlFlowInfo(&cfi);
      
	   if (cfi.fields.controlFlowInstruction &&
	       cfi.fields.isJmpLink &&
	       cfi.destination == instr_t::controlFlowInfo::regRelative &&
	       *(sparc_reg*)cfi.destreg1 == sparc_reg::l0 &&
	       cfi.offset_nbytes == 8) {
	       clk_thread_fn_jmpl_exitpoints += addr;
	   }
       }

       assert(clk_thread_fn_jmpl_exitpoints.size() == 2);
       std::sort(clk_thread_fn_jmpl_exitpoints.begin(),
	         clk_thread_fn_jmpl_exitpoints.end());
   
       const unsigned clk_thread_fn_firstjmplexit_addr = clk_thread_fn_jmpl_exitpoints[0];
       const unsigned clk_thread_fn_firstjmplexit_bbid = clk_thread_fn.searchForBB(clk_thread_fn_firstjmplexit_addr);
       assert(clk_thread_fn_firstjmplexit_bbid != -1U);
       const basicblock_t *clk_thread_fn_firstjmplexit_bb = clk_thread_fn.getBasicBlockFromId(clk_thread_fn_firstjmplexit_bbid);
   
       switchout_splicepoints2 += clk_thread_fn_firstjmplexit_bb->getStartAddr();
       switchin_splicepoints2 += clk_thread_fn_firstjmplexit_addr;
       assert(clk_thread_fn_firstjmplexit_bb->containsAddr(clk_thread_fn_firstjmplexit_addr));
       assert(switchout_splicepoints2.size() == 4);
       assert(switchin_splicepoints2.size() == 4);

       // category 2 switch-out & -in points #4: within clk_thread_fn, switchout
       // at entry (before SERVE_INTR() macro) and switchin after the SERVE_INTR() macro.
       // To find the "after SERVE_INTR()" point, we note that at present, the code after
       // SERVE_INTR makes a cond branch to label 0f; it's the only block that does that.
       // 0f is the end of the function.  Anyway, so we look at the predecessors of the
       // 0f basic block, and there should be 2...discard the fall-through case and go
       // with the other basic block and we should get the bb that made the branch
       // to 0f.  Go with the *second* instruction of that basic block (the 1st insn
       // is a nop in the delay slot of a ba...we must *not* instrument there!)

       switchout_splicepoints2 += clk_thread_fn.getEntryAddr();

       const unsigned clk_thread_fn_secondjmplexit_addr = clk_thread_fn_jmpl_exitpoints[1];
       const unsigned clk_thread_fn_secondjmplexit_bbid = clk_thread_fn.searchForBB(clk_thread_fn_secondjmplexit_addr);
       assert(clk_thread_fn_secondjmplexit_bbid != -1U);
       const basicblock_t *clk_thread_fn_secondjmplexit_bb = clk_thread_fn.getBasicBlockFromId(clk_thread_fn_secondjmplexit_bbid);
       assert(clk_thread_fn_secondjmplexit_bb->numPredecessors() == 2);
       basicblock_t::ConstPredIter prediter(*clk_thread_fn_secondjmplexit_bb);
       unsigned pred = *prediter;
       const basicblock_t *pred0bb = clk_thread_fn.getBasicBlockFromId(pred);
       if (pred0bb->getEndAddr() + 1 == clk_thread_fn_secondjmplexit_bb->getStartAddr()) {
	   // The first one was not a match.  Try the second one
	   ++prediter;
	   pred = *prediter;
	   const basicblock_t *pred1bb = clk_thread_fn.getBasicBlockFromId(pred);
	   switchin_splicepoints2 += pred1bb->getStartAddr() + 4; // +4: skip the nop
       }
       else
	   switchin_splicepoints2 += pred0bb->getStartAddr() + 4; // +4: skip the nop
   }

   dout << "switch-out2 splice points:" << endl;
   for (pdvector<kptr_t>::const_iterator iter = switchout_splicepoints2.begin();
        iter != switchout_splicepoints2.end(); ++iter)
      dout << " " << addr2hex(*iter);
   dout << endl;

   dout << "switch-in2 splice points:" << endl;
   for (pdvector<kptr_t>::const_iterator iter = switchin_splicepoints2.begin();
        iter != switchin_splicepoints2.end(); ++iter)
      dout << " " << addr2hex(*iter);
   dout << endl;

   return make_pair(make_pair(switchout_splicepoints1, switchin_splicepoints1),
                    make_pair(switchout_splicepoints2, switchin_splicepoints2));
}

