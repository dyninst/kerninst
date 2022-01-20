// regAnalysisOracles.C

#include "regAnalysisOracles.h"
#include "moduleMgr.h"
#include "interProcRegAnalysis.h"

// Regrettably, this method cannot return a const reference, because in one
// rarely used case, we have to make a calculation and return that, as opposed
// to always returning some already-calculated dataflow function.

const RegAnalysisHandleCall_FirstTime::dataflow_fn* 
RegAnalysisHandleCall_FirstTime::whenDestAddrIsKnown_force(kptr_t destaddr) const {
   pair<pdstring, function_t*> modAndFn = theModuleMgr.findModAndFnOrNULL(destaddr, false);
      // false --> not startOnly [Shouldn't it be true???]

   if (modAndFn.second == NULL) {
      if (verbose)
         cout << "RegAnalysisHandleCall_FirstTime warning -- address "
              << addr2hex(destaddr) << ": unknown fn.  (Unparsed?)"
              << endl
              << "Returning conservative result." << endl;
      return getPessimisticDataflowFn(destaddr);
   }
   function_t *fn = modAndFn.second;

   // OK, we have the fn which is the dest of the call.  The action we take now
   // depends on whether the fn is UNVISITED, VISITED-BUT-NOT-COMPLETED, or
   // COMPLETED.  We're hoping for either the 1st or 3d alternatives.
   if (fn->getRegAnalysisDFSstate() == function_t::unvisited) {
      // visit this fn now!
      if (verbose) {
         cout << "_FirstTime -- recursively analyzing fn @"
              << addr2hex(fn->getEntryAddr()) << " name=" << fn->getPrimaryName()
              << endl;
      }

      doInterProcRegisterAnalysis1Fn(modAndFn.first, *fn,
                                     *this,
                                        // function call oracle (no change)
                                     verbose);
   }
      
   function_t::regAnalysisFnDFSstates fnstatus = fn->getRegAnalysisDFSstate();
   switch (fnstatus) {
      case function_t::unvisited:
         assert(0); // should have changed to visitedButNotCompleted by now!
         break;
      case function_t::visitedButNotCompleted: {
         // The destination basic block's parent function is colored "grey": it's
         // been partially completed at best.  Here's the best we can do for now:
         // examine the register analysis information for the destination
         // basic block. 
         // If its 'inIsolation' field is still undefined, then
         // we'll have to return a conservative result that, as always, can
         // cause trouble (because we never know how many pre-save-frames the
         // conservative result should contain).   On the other hand, if its
         // 'inIsolation' field is defined, then so must its 'atTopOfBB', so we
         // return it.

         bbid_t destbb_id = fn->searchForBB(destaddr);
         if (destbb_id == (bbid_t)-1) {
            if (fn->isUnparsed()) {
               cout << "RegAnalysisHandleCall_FirstTime: destination block lies in a 'grey' DFS function that is skipped...returning conservative" << endl;
               return getPessimisticDataflowFn(destaddr);
            }
            else {
               if (verbose) {
                  cout << "RegAnalysisHandleCall_FirstTime warning -- address "
                       << addr2hex(destaddr) << " falls w/in a fn but not w/in a bb?"
                       << endl
                       << "the fn name is " << fn->getPrimaryName()
                       << "; startaddr=" << addr2hex(fn->getEntryAddr()) << endl;
                  cout << "Returning conservative result" << endl;
               }
               return getPessimisticDataflowFn(destaddr);
            }
         }

         // With interprocedural register analysis, there is the chance that
         // fn->getLiveRegInfo(destbb_id) will assert fail, because
         // register analysis state for the callee hasn't yet been initialized
         // (not even to conservative).
         if (!fn->haveLiveRegsBeenPreparedYet()) {
            if (verbose)
               cout << "RegAnalysisHandleCall_FirstTime: destination block "
                    << addr2hex(destaddr) << " lies in a 'grey' DFS function that"
                    << endl
                    << "has not yet initialized its register state.  Returning conservative register usage for this block" << endl;

            return getPessimisticDataflowFn(destaddr);
         }
         
         const function_t::liveRegInfoPerBB &destBlockInfo =
            fn->getLiveRegInfo(destbb_id);
            
         if (!destBlockInfo.atTopOfBBIsDefined()) {
            if (verbose) {
               cout << "RegAnalysisHandleCall_FirstTime: destination block "
                    << addr2hex(destaddr) << " lies in a 'grey' DFS function whose"
                    << endl
                    << "reg state has been initialized, but this block has not yet been able to fully analyze.  Returning conservative." << endl;
            }

            return getPessimisticDataflowFn(destaddr);
               // could also return destBlockInfo.atTopOfBB, because it should
               // equal getPessimisticDataflowFn()
         }
         else {
            if (verbose) {
               cout << "RegAnalysisHandleCall_FirstTime: destination block "
                    << addr2hex(destaddr) << " lies in a 'grey' DFS function, but"
                    << endl
                    << "at least it has been initialized.  Using it as a guide for number of levels/pre-save frames, and returning a conservative version thereof." << endl;
            }

            return getPessimisticDataflowFn(destaddr);
         }

         assert(false);
      }
      case function_t::completed: {
         // Hooray!  The dest fn has already been analyzed, so the result we want
         // is ready for the taking, with no calculation needed.
         if (verbose) {
            cout << "_FirstTime -- yea, grabbing already-completed result for fn @"
                 << addr2hex(fn->getEntryAddr()) << " name=" << fn->getPrimaryName()
                 << endl;
         }
            
         bbid_t bb_id = fn->searchForBB(destaddr);
         if (bb_id == (bbid_t)-1) {
            if (fn->isUnparsed()) {
               // ah, that fn was skipped -- explains why bb couldn't be found.

               if (verbose)
                  cout << "RegAnalysisHandleCall_FirstTime note -- could not analyze destination block at " << addr2hex(destaddr) << " because its parent function, starting at " << addr2hex(fn->getEntryAddr()) << " (" << fn->getPrimaryName().c_str() << ") is skipped.  Returning conservative result for that destination basic block." << endl;

               return getPessimisticDataflowFn(destaddr);
            }
            else {
               if (verbose) {
                  cout << "RegAnalysisHandleCall_FirstTime warning -- address "
                       << addr2hex(destaddr) << " falls w/in a fn but not w/in a bb?"
                       << endl
                       << "the fn name is " << fn->getPrimaryName()
                       << "; startaddr=" << addr2hex(fn->getEntryAddr()) << endl;
                  cout << "Returning conservative result" << endl;
               }
               
               return getPessimisticDataflowFn(destaddr);
            }
         }

         const basicblock_t *bb = fn->getBasicBlockFromId(bb_id);
         if (bb->getStartAddr() != destaddr) {
            cout << "RegAnalysisHandleCall_FirstTime warning -- address "
                 << addr2hex(destaddr) << ", the destination of a call, doesn't"
                 << endl << "start a basic block! (bb start is "
                 << addr2hex(bb->getStartAddr()) << ")" << endl;
            assert(false && "interprocedural ensure start of basic blocks failed");
            return getPessimisticDataflowFn(destaddr);
         }
         const function_t::liveRegInfoPerBB &info = fn->getLiveRegInfo(bb_id);
         return &info.getAtTopOfBB();
      }
      default:
         assert(0);
   }

   abort(); // placate compiler when compiling with NDEBUG
}

// --------------------------------------------------------------------------------

const RegAnalysisHandleCall_AlreadyKnown::dataflow_fn*
RegAnalysisHandleCall_AlreadyKnown::whenDestAddrIsKnown(kptr_t destaddr) const {
   const function_t* fn = theModuleMgr.findFnOrNULL(destaddr, false);
      // false --> not startonly.  (Why can't it be true?)

   if (fn == NULL) {
      if (verbose)
         cout << "RegAnalysisHandleCall_AlreadyKnown warning -- address "
              << addr2hex(destaddr) << ": unknown fn."
              << endl;
      return getPessimisticDataflowFn(destaddr);
   }

   bbid_t bb_id = fn->searchForBB(destaddr);
   if (bb_id == (bbid_t)-1) {
      if (verbose)
         cout << "RegAnalysisHandleCall_AlreadyKnown warning -- address "
              << addr2hex(destaddr) << " falls w/in a fn but not w/in a bb?"
              << endl;
      return getPessimisticDataflowFn(destaddr);
   }
   const basicblock_t *bb = fn->getBasicBlockFromId(bb_id);
   if (bb->getStartAddr() != destaddr) {
      if (verbose)
         cout << "RegAnalysisHandleCall_AlreadyKnown warning -- address "
              << addr2hex(destaddr) << " isn't at the start of a basic block" << endl;
      return getPessimisticDataflowFn(destaddr);
   }

   const function_t::liveRegInfoPerBB &info = fn->getLiveRegInfo(bb_id);
   return &info.getAtTopOfBB();
}

// --------------------------------------------------------------------------------

const RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion::dataflow_fn* 
RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion::
whenDestAddrIsKnown(kptr_t destaddr) const {
   if (destaddr == fnStartAddr) {
      cout << "Detected recursion; returning conservative for analysis" << endl;
      return getPessimisticDataflowFn(destaddr);
   }

   const function_t *fn = theModuleMgr.findFnOrNULL(destaddr, false);
      // false --> not start-only

   if (fn == NULL) {
      cout << "RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion warning -- address "
           << addr2hex(destaddr) << ": unknown fn."
           << endl;
      return getPessimisticDataflowFn(destaddr);
   }

   bbid_t bb_id = fn->searchForBB(destaddr);
   if (bb_id == (bbid_t)-1) {
      cout << "RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion warning -- address "
           << addr2hex(destaddr) << " falls w/in a fn but not w/in a bb?"
           << endl;
      return getPessimisticDataflowFn(destaddr);
   }
   const basicblock_t *bb = fn->getBasicBlockFromId(bb_id);
   if (bb->getStartAddr() != destaddr) {
      cout << "RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion warning -- address "
           << addr2hex(destaddr) << " isn't at the start of a basic block" << endl;
      return getPessimisticDataflowFn(destaddr);
   }

   cout << "RegAnalysisHandleCall_AlreadyKnown_BailOnRecursion found a decent-looking call to "
        << addr2hex(destaddr) << endl;

   return getPessimisticDataflowFn(destaddr);
}



