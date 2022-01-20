// interProcRegAnalysis.h
// Interprocedural register analysis
// Moving what used to be a moduleMgr method into a file of its own is meant as
// a first step in making a generic register analysis function.  We're not really
// there yet; in particular, while taking merge op and optimistic & pessimistic
// lattice values is a nice step towards genericity, always writing to specific
// live-register sets stored within a function is not so generic.
// But we're making progress...

#ifndef _INTER_PROC_REG_ANALYSIS_H_
#define _INTER_PROC_REG_ANALYSIS_H_

#include "util/h/kdrvtypes.h"
#include "util/h/out_streams.h"
#include "funkshun.h"
#include "readySet.h"
#include "skips.h" // bypass_analysis()
#include "bitwise_dataflow_fn.h"
#include "LatticeInfo.h"

#include "instr.h" // needed since we're going to instantiate in this file
#include "out_streams.h"

typedef monotone_bitwise_dataflow_fn dataflowfn_t;

static void setAllBBsToConservative(function_t &fn,
                                    const LatticeInfo &theLatticeInfo) {
   typedef function_t::bbid_t bbid_t;
   typedef LatticeInfo::dataflow_fn dataflowfn_t;

   const dataflowfn_t *conservativeDataflowFnToUse = 
      theLatticeInfo.getPessimisticDataflowFn(fn.getEntryAddr());

   const unsigned numbbs = fn.numBBs();
   for (bbid_t blockid = 0; blockid < numbbs; ++blockid) {
      function_t::liveRegInfoPerBB &info = fn.getLiveRegInfo(blockid);
      info.setAtTopOfBB(dataflowfn_t::getDataflowFn(*conservativeDataflowFnToUse));
   }
}

static void giveUpOnRegisterAnalysis1Fn(const pdstring &modName,
                                        function_t &fn,
                                        const LatticeInfo &theLatticeInfo,
                                        bool /*verbose*/) {
   dout << "Punting on register analysis for function "
        << modName << '/' << fn.getPrimaryName()
        << " at " << addr2hex(fn.getEntryAddr()) << endl;

   setAllBBsToConservative(fn, theLatticeInfo);

   // We used to assert fn.isUnanalyzed(), but that's not really correct; an
   // unanalyzed dataflow fn does not have the same value as a pessimistic/conservative
   // one.
}


static bool isRidiculous(const monotone_bitwise_dataflow_fn &dfFn) {
   return (dfFn.getNumPreSaveFramesManufactured() >= 100 ||
           dfFn.getNumLevels() >= 100 ||
           dfFn.getNumSavedWindows() >= 100);
}

static void delete_mbdf_vec(pdvector<dataflowfn_t *> &vec) {
   //clean up
   dataflowfn_t **iter = vec.begin();
   dataflowfn_t **finish = vec.end();
   for  (;iter != finish;iter++) {
      delete *iter;
   }
}

// Implementation of templated fns are in the .h file so the compiler can easily
// find them for instantiation:
template <class Oracle>
void
doInterProcRegisterAnalysis1Fn(const pdstring &modName,
                                  // for the call to bypass_analysis
                               function_t &fn,
                               const Oracle &theOracle,
                                  // this oracle should not assume that the callee has
                                  // been analyzed yet.  So, e.g., use object of type
                                  // RegAnalysisHandleCall_FirstTime
                               bool verbose) {
   typedef function_t::bbid_t bbid_t;
   
   if (verbose)
      cout << "welcome to register analysis for " << addr2hex(fn.getEntryAddr())
           << ' ' << modName << '/' << fn.getPrimaryName() << endl;
   
   assert(fn.getRegAnalysisDFSstate() == function_t::unvisited);
      // is this too strict an assert?

   fn.setRegAnalysisDFSstateToVisitedButNotCompleted();

   bool done = false;
   
   if (fn.isUnparsed()) {
      // If it wasn't parsed then it won't have basic blocks, meaning that we cannot
      // so much as fill in the vector of live reg info, because the vector will be
      // of size 0...no dfs traversal will have been done...etc.

      if (verbose)
         cout << "doInterProcRegisterAnalysis1Fn: not doing ANYTHING for this fn ("
              << modName
              << "/" << fn.getPrimaryName() << ") because it is unparsed!" << endl;

      done = true;
   }

   pdvector<dataflowfn_t*> liveRegDataflowFnForBlocksInIsolation;
   if (!done) {
      //Note that in-place construction no longer makes sense now that we have
      //a vector of pointers.
      // Initialize "liveRegDataflowFnForBlocksInIsolation"
      //dataflowfn_t **iter = liveRegDataflowFnForBlocksInIsolation.reserve_for_inplace_construction(fn.numBBs());
      //dataflowfn_t **finish = liveRegDataflowFnForBlocksInIsolation.end();
      
      bbid_t bbid = 0;
      unsigned numBBsInitialized = 0;

      while (numBBsInitialized != fn.numBBs()) {
	 const basicblock_t *bb = fn.getBasicBlockFromId(bbid++);

	 //new((void**)&inIsolation)dataflowfn_t(bb->doLiveRegAnalysisWholeBlock(fn.getOrigCode(), theOracle));
         dataflowfn_t *inIsolation =  bb->doLiveRegAnalysisWholeBlock(fn.getOrigCode(), theOracle); 

         // theFunctionCallOracle shouldn't assume that the dest fn has been
         // analyzed yet (and if it hasn't, then we'll do so recursively...sort of
         // like a depth-first-search)
         liveRegDataflowFnForBlocksInIsolation.push_back(inIsolation);
         ++numBBsInitialized;
      }
      
      assert(numBBsInitialized == fn.numBBs());
   }
   
   // Initializes per-bb live reg info for each bb.
   if (!done) {
      fn.prepareLiveRegisters(*(theOracle.getOptimisticDataflowFn()));
   
      // Are we supposed to not analyze this function?
      if (bypass_analysis(modName, fn.getPrimaryName(),
                          fn.getAllAliasNames(), fn.getNumAliases())) {
         if (verbose)
            cout << "doInterProcRegisterAnalysis1Fn: not analyzing this fn (" << modName
                 << "/" << fn.getPrimaryName() << ") because user asked us not to"
                 << endl;

         // Loop thru all basic blocks, setting result to conservative
         setAllBBsToConservative(fn, theOracle.getLatticeInfo());

         done = true;
      }
   }
   
   if (!done) {
      typedef function_t::bbid_t bbid_t;
      
      pdvector<bbid_t> preOrderNum2BlockId;
      pdvector<bbid_t> postOrderNum2BlockId;
      pdvector<uint16_t> blockId2PreOrderNum;
      pdvector<uint16_t> blockId2PostOrderNum;

      fn.performDFS(preOrderNum2BlockId, postOrderNum2BlockId,
                    blockId2PreOrderNum, blockId2PostOrderNum);
      
      // ready set: indexed by postorder traversal numbers.  A true entry means the
      // node with that postorder traversal # is in the ready set.  Initially we must
      // put all items in the ready set.  Note that ExitBBPtr doesn't have a traversal #;
      // fortunately, there's no need to process ExitBB here.
      readySet theReadySet(fn.numBBs(), true);
      uint16_t readySetIndex = 0; // traverse in postorder for a reverse df problem
      while (!theReadySet.empty()) {
         // Take an entry from the ready set.

         while (true) {
            // readySetIndex is a postorder number.
            if (theReadySet.contains(readySetIndex))
               break; // found an entry

            if (readySetIndex==fn.numBBs()-1)
               readySetIndex=0;
            else
               readySetIndex++;
         }
      
         theReadySet -= readySetIndex;

         // readySetIndex is the post-order number of the basic block we've chosen.
         // So, convert from postorder #'s to bb now:
         const bbid_t bb_id = postOrderNum2BlockId[readySetIndex];
         const basicblock_t *bb = fn.getBasicBlockFromId(bb_id);
      
         dataflowfn_t *NU = NULL; //placate compiler
         NU = dataflowfn_t::getDataflowFn(*(theOracle.getOptimisticDataflowFn()));
         // is this right?  (Yea; Allen, ed. fig 4.16 says to set it to the top of
         // the lattice; i.e. set it to optimistic value)

         function_t::liveRegInfoPerBB &liveRegsInfoThisBB =
            fn.getLiveRegInfo(bb_id);
         const dataflowfn_t *inIsolation = liveRegDataflowFnForBlocksInIsolation[bb_id];

         bool firstSucc = true;
         const basicblock_t::ConstSuccIter start = bb->beginSuccIter();
         const basicblock_t::ConstSuccIter finish = bb->endSuccIter();
         for (basicblock_t::ConstSuccIter iter=start; iter != finish; ++iter) {
            bbid_t succ_id = *iter;
            dataflowfn_t *succfn = // regrettably, this cannot return a const reference
               fn.calc1EdgeResult(bb_id, succ_id, theOracle, true);
               // true --> optimistic if undefined

               // The interesting thing is that
               // in the case of undefined (not to be confused with unanalyzable),
               // we actually want to return an optimistic result.  This is in keeping
               // with the spirit of this algorithm, which successively refines the
               // registers deemed live at the top of the basic block by starting with
               // an optimistic value, and refining that result by merging with
               // successors as they change (and since the mergeop tends to make
               // things more pessimistic, it's OK to start with a completely
               // optimistic result, I think).   IS THIS RIGHT?

            if (firstSucc) {
               // general formula: NU is merge of all "bb o successor" for each succ
               // Since (so far) just 1 successor, an obvious optimization is simply not
               // to merge
               delete NU; 
               NU = succfn; 
               inIsolation->compose_overwrite_param(*NU);

               firstSucc = false;
            }
            else {
               dataflowfn_t *composition = succfn; 
               inIsolation->compose_overwrite_param(*composition);
            
               try {
                  NU->mergeWith(*composition, theOracle.getMergeOp());
               }
               catch (monotone_bitwise_dataflow_fn_ll::
                      DisagreeingNumberOfPreSaveFramesInMerge) {
                  dout << "doInterProcRegisterAnalysis1Fn -- caught exception (trying to merge dataflow functions w/ differing # of preSaveFrames)" << endl;

                  // We're giving up on this function.
                  // ***NOTE***: I think that calling fry is too much; we want to keep
                  // the basic blocks, since there's nothing wrong with them; we just
                  // want to kill live register analysis stuff, right?
                  // Or better yet, set all of the live register analysis stuff
                  // (i.e. each basic block) to conservative values.
                  giveUpOnRegisterAnalysis1Fn(modName, fn,
                                              theOracle.getLatticeInfo(), verbose);
                  fn.setRegAnalysisDFSstateToCompleted(); // I guess
                 delete_mbdf_vec(liveRegDataflowFnForBlocksInIsolation);
                  delete NU; 
		  delete composition;
		  return; // "done" analyzing this function's live reg info
               }
            
               if (NU->getNumPreSaveFramesManufactured() !=
                   composition->getNumPreSaveFramesManufactured()) {
                  cout << "YIKES2!!!" << endl;

                  giveUpOnRegisterAnalysis1Fn(modName, fn,
                                              theOracle.getLatticeInfo(), verbose);
                  fn.setRegAnalysisDFSstateToCompleted(); // I guess
                 delete_mbdf_vec(liveRegDataflowFnForBlocksInIsolation);
		 delete NU;
		 delete composition;
                  return; // "done" analyzing this function's live reg info
               }

               // Verify: merging should be commutative
               // (expensive, so normally not done)
#ifdef _DO_EVEN_EXPENSIVE_ASSERTS_
               assert(NU == composition.merge(NU, theOracle.getMergeOp()));
#endif
               delete composition;
            }
         } // loop through successors

         dataflowfn_t &atTopOfBB = liveRegsInfoThisBB.getAtTopOfBB();
            // do *not* omit the "&" in the above line, or the possible change to it
            // below won't get reflected (I know, "duh", but it's worth the reminder).
         if (atTopOfBB != *NU) {
            // a change was made.  Put all predecessors of bb (since this is a reverse
            // data flow problem) onto the ready set.  Note that there's no need to put
            // ExitBBPtr or UnanalyzableBBPtr onto the ready set, because we've already
            // calculated the final result for them.

            // NEW "feature": give up if it looks like we're going into an infinite
            // loop (due to ridiculously sized dataflow functions)
            if ( isRidiculous(*NU) )  {
               cout << "Assumed infinite loop while reg-analyzing ";
               cout << modName << '/';
               cout << fn.getPrimaryName().c_str();
               cout << " at " << addr2hex(fn.getEntryAddr()) << endl;
               
               giveUpOnRegisterAnalysis1Fn(modName, fn,
                                           theOracle.getLatticeInfo(), verbose);
               fn.setRegAnalysisDFSstateToCompleted(); // I guess
               delete_mbdf_vec(liveRegDataflowFnForBlocksInIsolation);
	       delete NU;
               return; // "done" analyzing this function's live reg info
            }

            atTopOfBB = *NU;
            assert(atTopOfBB == *NU);
            delete NU;
            // the entry bb of a fn will have 0 predecessors; all others should
            // have at least 1.
            const basicblock_t::ConstPredIter start  = bb->beginPredIter();
            const basicblock_t::ConstPredIter finish = bb->endPredIter();
            for (basicblock_t::ConstPredIter iter=start; iter != finish; ++iter) {
               const bbid_t pred_id = *iter;
            
               // To add pred to the ready set, we need to convert from basicblock to
               // its postorder number:

               const uint16_t predecessor_postorder_num = blockId2PostOrderNum[pred_id];
               theReadySet += predecessor_postorder_num;
            }
         } // change was made
      } // while ready set not empty
   }
   
   fn.setRegAnalysisDFSstateToCompleted();

   if (verbose) {
      cout << "interproc register analysis just finished for fn "
           << fn.getPrimaryName() << "; result is ";
      if (fn.isUnparsed())
         cout << "undefined (since skipped)" << endl;
      else {
         const function_t::liveRegInfoPerBB &liveRegsInfoEntryBB =
            fn.getLiveRegInfo(fn.xgetEntryBB());
         const dataflowfn_t &df_fn = liveRegsInfoEntryBB.getAtTopOfBB();
         if (df_fn.getNumPreSaveFramesManufactured() > 0)
            cout << "-" << df_fn.getNumPreSaveFramesManufactured() << endl;
         else
            cout << df_fn.getNumSavedWindows()+1 << endl;
      }
   }
   delete_mbdf_vec(liveRegDataflowFnForBlocksInIsolation);
}

#endif
