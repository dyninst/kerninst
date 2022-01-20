// funkshun.h
// Yep, you read it right.  We went with this name not just to get down & groove, but
// because "function.h" is already taken by some egcs STL file.

/*

Parsing open issues:

1) A lot of times, a conservative result must be returned for a function.
(It happens *all* the time when not doing interprocedural analysis,
by design.  Plus, it happens for unanalyzable call destinations, loops
in the call graph, and other times, when doing interprocedural analysis.)

What's the correct designation for a conservative dataflow function?
Sounds obvious: all regs made live.  But on the SPARC, thanks to register
windows, there is no such thing as an obvious dataflow function.

I'm seeing weird results, like dataflow functions 3 or more levels deep,
thanks to propagation of incorrect "conservative" dataflow fns like this.
When you think about it, this actually occurs more often when
interprocedural register analysis is turned off.  Either way, it's
a real problem, leading me not to really trust register analysis results
that we calculate.

2) Code that calls a function guaranteed to be a black hole, like panic,
should probably be marked with a special edge.  At a minimum, note that
we're incorrectly parsing such functions as if the panic returns!
At a maximum, even if the CFG is properly parsed, live register analysis
is going to have to recognize such calls, and take special action,
especially in deciding on a dataflow function for such a call (how many
levels should it have?)

3) Delay slots that are also destinations of some other branches.
Oh boy, does this mess up live register analysis!  (Fortunately
it's a rare case, maybe 4 times in the kernel.)
Let's talk about that fun, fun case now.

Say we're parsing along and see a branch-always instruction, with a nop
in its delay slot.  So for now at least, both the branch and the nop go
in the same basic block.  But then as we parse the rest of the function,
we see a branch to that nop instruction (the delay slot of the first branch).
So we end up splitting the first basic block, to ensure the nop is at the
start of a basic block.  In theory, that's great (and that's what's being
done right now).  BUT THERE IS A BIG, BIG PROBLEM WITH REGARDS TO
REGISTER ANALYSIS IN THIS SITUATION.

--------------------
0x1000: ba to 0x2000
0x1004: nop
--------------------
preds: none
succs: 0x2000

The above is what the basic block looks like before it was split.
After splitting, we need something like this:

--------------------
0x1000: ba to 0x2000
--------------------
preds: none
succs: fall thru to 0x1004

--------------------
0x1004: nop
--------------------
preds: 0x1000
succs: 0x2000 (the destination of the original branch at 0x1000)

That's fine until the branch to 0x1004 comes along.  It will split
the block at 0x1004, as just shown, (so far so good).  But then
of course it will add itself (say that branch was at 0x3000) as
a predecessor of the 0x1004 basic block.  That's TERRIBLE, because
it will then appear as if all of the effects of block 0x1004
can be seen no matter how it was reached, whether falling through
from block 0x1000, or from the other branch at 0x3000.  In
particular, the successor edge of 0x1004, the branch to
0x2000, CANNOT OCCUR when the block was reached via 0x3000.
So it's wrong!
And that's not all; there is another bug!  Note that the basic block
at 0x1004 only contains a single instruction; the nop.  Well that's
sort of correct, when the block was reached via 0x1000.  But if the
branch was reached via 0x3000, then the nop is going to fall through
to MORE CODE THAT NEEDS TO BE PARSED!

What a mess this is.  It's almost as if 0x1004 should be a part
of two basic blocks, one having 0x2000 as the only successor
(in this case, 0x1004 is acting as the delay slot of the branch
at 0x1000), and another having succeeding instructions at 0x1008,
0x100c, etc. (in this case, 0x1004 is acting as a rather normal
basic block, reached by a rather normal branch at 0x3000, having
little or nothing to do with delay slots and such.)

Yes, yes, hmmm, it's almost as if 0x1004 should be part of two
basic blocks.  That's the only way we can correctly have
different successor edges (in one case, to 0x2000; in the
other case, fall thru to more instructions at 0x1008, etc.).

SO THERE NEEDS TO BE A RATHER MAJOR INFRASTRUCTURE CHANGE THAT
ALLOWS FOR BASIC BLOCKS HAVING OVERLAPPING ADDRESSES!!!!!
Needless to say, this will make things interesting.  For example,
what will it now mean to search for a basic block by address?
Should we return 2 matches if there are two?

--Ari 12/2000
 */

#ifndef _FUNKSHUN_H_
#define _FUNKSHUN_H_

#include <inttypes.h> // uint32_t
#include "basicblock.h"
#include "bitwise_dataflow_fn.h"
#include "fnCode.h"

class simplePath; // fwd decl

#include "util/h/rope-utils.h" // addr2hex()
#include "util/h/mrtime.h"
#include "bbParsingStats.h"

#ifdef _KERNINSTD_
#include "hiddenFns.h"
#include "util/h/Dictionary.h"
#endif

#include "pair.h"
#include "util/h/StaticString.h"

class insnVec_t; //fwd decl
class parseOracle; //fwd decl

class function_t {
 public:
   // Exceptions:
   class parsefailed {};

   // Architecture specific types:
   typedef fnCode fnCode_t;
   typedef basicblock_t::bbid_t bbid_t;
   typedef arch_cfa_t cfanalysis_t;
   typedef monotone_bitwise_dataflow_fn dataflowfn_t;

   class liveRegInfoPerBB {
    private:
      // We no longer store "inIsolation" here; instead, it's a local variable
      // initialized within interProcRegAnalysis.h/doInterProcRegisterAnalysis1Fn()

      dataflowfn_t *atTopOfBB;
         // the actual regs which are live at the *top* of the bb

         // TODO: when inIsolation is "dummy", then is this value defined?  I would
         // think not, yet we're finding that in some cases, this field is indeed
         // referenced even when inIsolation is "dummy"; in particular, when
         // doing live register analysis on a basic block having itself as a successor
         // (not a common case, but it happens...)

    public:
      liveRegInfoPerBB(XDR *xdr) {
	 atTopOfBB = dataflowfn_t::getDataflowFn(xdr);
      }

      unsigned getMemUsage_exceptObjItself() const {
         return atTopOfBB->getMemUsage_exceptObjItself();
      }
      unsigned numLevels() const {
         return atTopOfBB->getNumLevels();
      }
      
      bool send(XDR *xdr) const {
         return atTopOfBB->send(xdr);
      }
      
      liveRegInfoPerBB(dataflowfn_t &iatTopOfBB) :
         atTopOfBB(&iatTopOfBB) {}

      liveRegInfoPerBB(const liveRegInfoPerBB &src) {
         atTopOfBB = dataflowfn_t::getDataflowFn(*(src.atTopOfBB));
      }

      liveRegInfoPerBB(bool, const dataflowfn_t &optimistic_dataflowfn) {
         atTopOfBB = dataflowfn_t::getDataflowFn(optimistic_dataflowfn); 
      }

      ~liveRegInfoPerBB() {
         delete atTopOfBB;
      }

      bool atTopOfBBIsDefined() const {
         return (atTopOfBB != NULL) && (!atTopOfBB->isDummy());
      }

      const dataflowfn_t &getAtTopOfBB() const {
	 assert(atTopOfBB != NULL);
         return *atTopOfBB;
      }
      dataflowfn_t &getAtTopOfBB() {
         assert(atTopOfBB != NULL);
         return *atTopOfBB;
      }

      void setAtTopOfBB(dataflowfn_t *dff) {
         delete atTopOfBB;
         atTopOfBB = dff;
      }
   };

   enum regAnalysisFnDFSstates {unvisited, visitedButNotCompleted, completed};

 protected:
   kptr_t entryAddr;
      // not necessarily the same as the startAddr of the first chunk within origCode
      // below, so we must keep it around.
   fnCode_t origCode;

   StaticString primary_name; // the first name we see
   StaticString* alternate_names; // NULL if none (the common case), else a vector
   unsigned num_alternate_names; // num elems in the vector

   pdvector<basicblock_t *> basic_blocks;
      // vector indexed by "basic block id", which has no particular ordering (e.g.
      // don't interpret this id as a preorder number or anything like that).

   // Sorting the basic blocks by their addresses; essential during parsing; also
   // needed later when doing live register analysis.
   mutable pdvector<bbid_t> sorted_blocks;
   mutable pdvector<bbid_t> unsorted_blocks;

   regAnalysisFnDFSstates regAnalysisDFSstate;
   pdvector<liveRegInfoPerBB> liveRegisters;
      // indexed by bb_id, not by bb preorder number.

   pdvector<bbid_t> blocksHavingInterProcFallThru;
      // Now that functions can be disjoint, we need to store this vector instead
      // of a plain "fallsThruToNextFunction" flag.
      // The above vector is (for now at least) unsorted.  It must contain
      // no duplicates.

   virtual void performDFS(bbid_t bb_id,
			   uint16_t &currPreOrderNum,
			   uint16_t &currPostOrderNum,
			   pdvector<bbid_t> &preOrderNum2BlockId,
			   pdvector<bbid_t> &postOrderNum2BlockId,
			   pdvector<uint16_t> &blockId2PreOrderNum,
			   pdvector<uint16_t> &blockId2PostOrderNum) const;
   // Don't call this until all bbs have been created (including all splits).
   // Call this routine before you call calculateImmediateDominators() and
   // calculateDominanceFrontiers().

   void sort_1() const;
   // optimized for a particular case: when sorted_blocks.size() > 0 &&
   // unsorted_blocks.size() == 1

   void merge(pdvector<bbid_t> &,
              const pdvector<bbid_t> &,
              const pdvector<bbid_t> &) const;
   // would like to make static but needs basic_blocks[]

   void assert_sorted() const {
      assert(sorted_blocks.size() == basic_blocks.size() &&
             unsorted_blocks.size() == 0);
   }
   
   void check_no_dups();
   void mergeSort() const; // const -- just plays with mutable members
   void makeSorted() const;
   void makeSortedIf10() const {
      if (unsorted_blocks.size() > 10)
         makeSorted();
   }

   bbid_t binary_search(kptr_t addr) const;
      // assumes sorted.  Returns (bbid_t)-1 if not found.
   
   bbid_t searchForFirstBlockStartAddrGThan(kptr_t addr) const;

   template <class calcEffectOfCall>
   dataflowfn_t * // unfortunately, cannot return a const reference
   getLiveRegFnForAnExitEdge(bbid_t bb_id, bbid_t succ_bb_id,
                             const calcEffectOfCall &callOracle) const;

   void processNonControlFlowCode(bbid_t bb_id,
                                  kptr_t codeStartAddr,
                                  unsigned &codeNumInsns,
                                  const parseOracle *theParseOracle) const;

   // private to ensure they're not used:
   function_t(const function_t &);
   function_t &operator=(const function_t &);

 public:
   function_t(XDR *);
   function_t(kptr_t startAddr, const StaticString &fnName);
   function_t(kptr_t startAddr, const pdvector<StaticString> &fnName);

   static function_t * getFunction(XDR *);
   static function_t * getFunction(kptr_t startAddr, const StaticString &fnName);
   static function_t * getFunction(kptr_t startAddr, 
				   const pdvector<StaticString> &fnNames);

   virtual ~function_t(); 

   virtual void getMemUsage(unsigned &forBasicBlocks,
			    unsigned &forFns_OrigCode,
			    unsigned &forLiveRegAnalysisInfo,
			    unsigned &num1Levels, // reg analysis
			    unsigned &num2Levels,
			    unsigned &numOtherLevels
			    ) const = 0;
   
   bool send(XDR *) const;

   void fnHasAlternateName(const StaticString &);
   void fnHasSeveralAlternateNames(const pdvector<StaticString> &);
   const StaticString &getPrimaryName() const { return primary_name; }
   unsigned getNumAliases() const { return num_alternate_names; }
   const StaticString *getAllAliasNames() const { return alternate_names; }
   const StaticString &getAliasName(unsigned ndx) const {
      assert(ndx < num_alternate_names);
      return alternate_names[ndx];
   }
   bool matchesName(const pdstring &) const;

   unsigned totalNumBytes_fromBlocks() const;
      // Iterates thru each bb (--> slow!), counting the number of instruction
      // bytes.  Can give a different result from
      // getOrigCode().totalNumBytes_fromChunks(), which includes any embedded
      // data and dead code, if any.

   const fnCode_t &getOrigCode() const {
      return origCode;
   }
   instr_t *get1OrigInsn(kptr_t addr) const {
      return origCode.get1Insn(addr); // could throw fnCode::BadAddr()
   }

   kptr_t getEntryAddr() const {
      return entryAddr;
         // not nec. the same as origCode[0].startAddr
   }

   bool containsAddr(kptr_t addr) const {
      return origCode.enclosesAddr(addr);
   }
   
   pdvector<kptr_t> getAllBasicBlockStartAddressesSorted() const;
      // gives you the entry point of each basic block, properly sorted.
      // In other words, the blocks are sorted by address, not by bb_id
   pdvector<bbid_t> getAllBasicBlockIdsSortedByAddress() const;
      // Basically exactly the same routine as above.  We return the same
      // basic blocks, in the same ordering.  The only difference is that
      // instead of returning the start addr of a given basic block, you
      // get its bb_id.  Otherwise, it's exactly the same.

   const basicblock_t *getBasicBlockFromId(bbid_t bb_id) const {
      return basic_blocks[bb_id];
   }

   // ------------------------------------------------------------

   void markBlockAsHavingInterProcFallThru(bbid_t bbid);
   virtual kptr_t getBBInterProcFallThruAddrIfAny(bbid_t bbid) const;
      // returns (kptr_t)-1 if none
   virtual kptr_t getBBInterProcBranchDestAddrIfAny(bbid_t bbid) const = 0;
      // returns (kptr_t)-1 if none

   // ------------------------------------------------------------

   unsigned numBBs() const { return basic_blocks.size(); }

   bool isSorted() const { return (unsorted_blocks.size() == 0); }
   const pdvector<bbid_t> &getSortedBlocks() const { return sorted_blocks; }

   bbid_t xgetEntryBB() {
      // x added because entry block execution counts isn't the same as saying how
      // many times the fn is called...necessarily.
      makeSorted();
      return sorted_blocks[0];
   }
   bbid_t xgetEntryBB() const {
      // x added because entry block execution counts isn't the same as saying how
      // many times the fn is called...necessarily.
      makeSorted();
      return sorted_blocks[0];
   }
   // We might prefer to return basicblock &, but we need the ability to return
   // a sentinel ((bbid_t)-1) if not found.  Perhaps it would be better to throw an
   // exception if not found?
   bbid_t searchForBB(kptr_t addr) {
      // returns (bbid_t)-1 if not found
      makeSorted();
      return binary_search(addr);
   }
   bbid_t searchForBB(kptr_t addr) const {
      // returns (bbid_t)-1 if not found
      makeSorted();
      return binary_search(addr);
   }

   virtual void performDFS(pdvector<bbid_t> &preOrderNum2BlockId,
			   pdvector<bbid_t> &postOrderNum2BlockId,
			   pdvector<uint16_t> &blockId2PreOrderNum,
			   pdvector<uint16_t> &blockId2PostOrderNum) const;

   void fry(bool verbose);

   pdvector<kptr_t> getCurrEntryPoints() const;
   virtual pdvector<kptr_t> getCurrExitPoints() const = 0;

   bool isUnparsed() const {
      // this used to be: return orig_insns.size() == 0;
      // but a function that has to be discarded because basic block parsing
      // failed will still have an orig_insns[].  So a better check is to see
      // if basic_blocks[] is empty, I think
      return basic_blocks.size() == 0;
   }
   
   // Splitting:
   void ensureBlockStartInsnIsAlsoBlockOnlyInsn(kptr_t addr,
							instr_t *i);
      // assert addr begins a basic block, then make sure it's the only
      // insn in the basic block, splitting if necessary.

   void ensure_start_of_bb(kptr_t addr);
      // make sure that this fn has a bb starting at 'destAddr'.  If it
      // doesn't then split() the appropriate bb to make it so.

   bbid_t splitBasicBlock(bbid_t bb_id, kptr_t splitAddr);
      // create a new basic block which splits the specified basic block at the
      // specified address.  Return the id of the new bb.

   // Non-const methods for building a function during parsing.
   // These used to be private, but parsing is now usually performed by outside
   // code (e.g. the entire sparc_controlFlow.C/x86_controlFlow.C)
   void bumpUpBlockSizeBy1Insn(bbid_t bb_id, const instr_t *instr);
      // formerly appendToBBContents()
   
   bbid_t addBlock_new(basicblock_t *);
      // returns the new basic block's id.
      // adds to both basic_blocks[] and unsorted_blocks[], and resorts if 10 unsorted
      // blocks exist.  addBlock() (as opposed to addBlock_new()) curiously omitted
      // adding to basic_blocks[].  Note also that the parameter type is now
      // basicblock<>, instead of a bb_id.
   void assertSorted() const;
   void addSuccessorToBlockByIdNoDup(bbid_t bb_id,
                                     bbid_t successor_id) {
      basic_blocks[bb_id]->addSuccessorNoDup(successor_id);
   }
   bool addSuccessorToBlockByIdUnlessDup(bbid_t bb_id,
						 bbid_t successor_id) {
      return basic_blocks[bb_id]->addSuccessorUnlessDup(successor_id);
   }
   bool addPredecessorToBlockByIdUnlessDup(bbid_t bb_id,
						   bbid_t pred_id) {
      return basic_blocks[bb_id]->addPredecessorUnlessDup(pred_id);
   }

   void recreateJumpTableEdgesForFewerEntries(bbid_t bb_id,
					      const pdvector<bbid_t> &newSuccessors);

   // Depth-first search through functions (not to be confused w/ dfs of the basic
   // blocks _within_ this function)
   regAnalysisFnDFSstates getRegAnalysisDFSstate() const { return regAnalysisDFSstate; }
   void setRegAnalysisDFSstateToVisitedButNotCompleted() {
      assert(regAnalysisDFSstate == unvisited);
      regAnalysisDFSstate = visitedButNotCompleted;
   }
   void setRegAnalysisDFSstateToCompleted() {
      assert(regAnalysisDFSstate == visitedButNotCompleted);
      regAnalysisDFSstate = completed;
   }

   // Live register analysis, at granularity of basic blocks:

   bool haveLiveRegsBeenPreparedYet() const {
      return liveRegisters.size() > 0;
   }
   
   void prepareLiveRegisters(const dataflowfn_t &optimistic_dataflowfn);
      // resize 'liveRegisters' to numBBs; initialize its "inIsolation" to "undefined"
      // and its "atTopOfBB" to optimistic_dataflowfn.

   liveRegInfoPerBB &getLiveRegInfo(bbid_t bb_id) {
      // note the non-const return type; this fn is used when *setting* the live regs.
      // see moduleMgr.C's doGlobalRegisterAnalysis1Fn()
      return liveRegisters[bb_id];
   }
   const liveRegInfoPerBB &getLiveRegInfo(bbid_t bb_id) const {
      return liveRegisters[bb_id];
   }

   template <class calcEffectOfCall>
   dataflowfn_t* calc1EdgeResult(bbid_t bb_id, bbid_t succ_bb_id,
				const calcEffectOfCall &c,
				bool optimisticIfUndefined) const;
      // A helper function, used by mergeSuccessors() below, and
      // (more importantly) by interProcRegAnalysis.h
   
   template <class calcEffectOfCall>
   dataflowfn_t* mergeSuccessors(bbid_t bb_id, 
 				 const calcEffectOfCall &c,
				 bool optimisticIfUndefined) const;
      // Some of the successors may be exit-type edges.
      // optimisticIfUndefined --> if false then we barf if result for any
      // non-exit successor that has not had its result defined atTopOfBB yet.
      // Caller note: we don't kludge the returned value to ensure that %sp and %fp are
      // "made live" if they aren't already.  Of course, you must treat them as
      // always live...just be advised that routines dealing with
      // monotone_bitwise_dataflow_fn's don't "start()" these regs

   template <class calcEffectOfCall>
   dataflowfn_t *
   getLiveRegFnBeforeInsnAddr(kptr_t insnAddr, const calcEffectOfCall &,
                              bool useCachedAnswerIfAvailable) const;
   // useCachedAnswerIfAvailable parameter: if false, we don't return the value already
   // calculated at the top of this basic block (if applicable).  This forces manual
   // calculation, and is helpful for assertion checking the result of this function
   // with that of getLiveRegFnBeforeAndAfterInsnAddr().
   
   template <class calcEffectOfCall>
   dataflowfn_t *
   getLiveRegFnAfterInsnAddr(kptr_t insnAddr, const calcEffectOfCall &) const;
   // Caller note: we don't kludge the returned value to ensure that %sp and %fp are
   // "made live" if they aren't already.  Of course, you must treat them as
   // always live...just be advised that routines dealing with
   // monotone_bitwise_dataflow_fn's don't "start()" these regs
   
   template <class calcEffectOfCall>
   pair<dataflowfn_t *, dataflowfn_t * >
   getLiveRegFnBeforeAndAfterInsnAddr(kptr_t insnAddr,
                                      const calcEffectOfCall &) const;
   // Caller note: we don't kludge the returned value to ensure that %sp and %fp are
   // "made live" if they aren't already.  Of course, you must treat them as
   // always live...just be advised that routines dealing with
   // monotone_bitwise_dataflow_fn's don't "start()" these regs

   // Iteration (through basic blocks), stl style.
   // NOTE: Iteration is unordered...will jump from basic block to basic block.
   typedef pdvector<basicblock_t *>::const_iterator const_iterator;
   typedef pdvector<basicblock_t *>::iterator iterator;

   const pdvector<basicblock_t *> &get_all_bbs() const {
      // used by xdr send routine
      return basic_blocks;
   }

   iterator begin() { return basic_blocks.begin(); }
   const_iterator begin() const { return basic_blocks.begin(); }
   
   iterator end() { return basic_blocks.end(); }
   const_iterator end() const { return basic_blocks.end(); }

#ifdef _KERNINSTD_
   virtual void parse(bbid_t bb_id,
		      const simplePath &pathFromEntryBB,
		      parseOracle *theParseOracle) const = 0;

   virtual void parseCFG(const fnCode_t &iCode,
		 // Above param will be used to initialize, en masse, the fn's code.
		 // After parsing has completed, unused parts of the code will be
		 // trimmed, so it's only important to pass *at least* enough bytes
		 // to cover the entire function.
		 // NOTE: no need to say which of "iCode"'s chunks are the entry
		 // chunk; we can figure that out from this->entryAddr, which has
		 // already been set.
			 const pdvector<unsigned> &chunkNdxsNotToTrim,
			 // e.g. for jump table data
			 dictionary_hash<kptr_t,bool> &interProcJumps,
			 pdvector<hiddenFn> &,
			 bbParsingStats &) = 0;
#endif

   void getCallsMadeBy_AsOrigParsed(pdvector< pair<kptr_t, kptr_t> > &regularCalls,
				    bool interProcBranchesToo,
				    pdvector< pair<kptr_t, kptr_t> > &interProcBranches,
				    bool unAnalyzableCallsToo,
				    pdvector<kptr_t> &unAnalyzableCallsThruRegister
				    ) const {
      // Appends to "regCalls" and, if interProcBranchesToo==true, to interProcBranches.
      // We do NOT assert that these vectors are empty when passed in.
      return getCallsMadeBy_WithSuppliedCode(origCode, regularCalls,
                                             interProcBranchesToo, interProcBranches,
                                             unAnalyzableCallsToo,
                                             unAnalyzableCallsThruRegister);
   }

   void getCallsMadeBy_WithSuppliedCode(const fnCode_t &fc,
                                        pdvector<pair<kptr_t,kptr_t> > &regularCalls,
                                        bool interProcBranchesToo,
                                        pdvector<pair<kptr_t,kptr_t> > &interProcBranches,
                                        bool unAnalyzableCallsToo,
                                        pdvector<kptr_t> &unAnalyzableCallsThruRegister)
                                        const {
      // regularCalls and interProcBranches: .first: insn addr, .second: callee addr
      // note: result will be UNSORTED, either by insnAddr or callee, believe it or not
      // Appends to "regCalls" and, if interProcBranchesToo==true, to interProcBranches.
      // We do NOT assert that these vectors are empty when passed in.
      assert(sorted_blocks.size() == numBBs() && unsorted_blocks.size() == 0);
      getCallsMadeBy_WithSuppliedCodeAndBlocks(fc, sorted_blocks,
                                               regularCalls,
                                               interProcBranchesToo,
                                               interProcBranches,
                                               unAnalyzableCallsToo,
                                               unAnalyzableCallsThruRegister);
   }

   void getCallsMadeBy_WithSuppliedCodeAndBlocks
     (const fnCode_t &,
      const pdvector<bbid_t> &blocks,
      pdvector< pair<kptr_t, kptr_t> > &regCalls,
      bool interProcBranchesToo,
      pdvector< pair<kptr_t, kptr_t> > &interProcBranches,
      bool unAnalyzableCallsToo,
      pdvector<kptr_t> &unAnalyzableCallsThruRegister) const;
      // returns a subset of what getCallsMadeBy_WithSuppliedCode() returns, a
      // determined by the "blocks[]" param.  Of course, that param is "all bbs of fn",
      // then the result will be the same, after all.
      // Appends to "regCalls" and, if interProcBranchesToo==true, to interProcBranches.
      // We do NOT assert that these vectors are empty when passed in.

   // ----------------------------------------------------------------------
   // getCallsMadeTo - type routines: not to be confused with getCallsMadeFrom routines
   // ----------------------------------------------------------------------

   pdvector<kptr_t> getCallsMadeTo_asOrigParsed(kptr_t destAddr,
                                              bool interProcBranchesToo) const {
      return getCallsMadeTo_withSuppliedCode(destAddr, origCode,
                                             interProcBranchesToo);
   }
   pdvector<kptr_t> getCallsMadeTo_withSuppliedCode(kptr_t destAddr,
                                                  const fnCode_t &,
                                                  bool interProcBranchesToo) const;
   
};

// As usual, templated methods go in .h file for easy auto-instantiation by the
// compiler.  Other methods can stay in .h file, even though the class as a whole
// is templated, provided we're willing to manually instantiate the class as a whole
// in some kind of templates.C file.

template <class calcEffectOfCall>
function_t::dataflowfn_t *
function_t::getLiveRegFnBeforeInsnAddr(kptr_t insnAddr,
				     const calcEffectOfCall &c,
				     bool useCachedAnswerIfAvailable) const {
   // useCachedAnswerIfAvailable parameter: if false, we don't return the value already
   // calculated at the top of this basic block (if applicable).  This forces manual
   // calculation, and is helpful for assertion checking the result of this function
   // with that of getLiveRegFnBeforeAndAfterInsnAddr().

   bbid_t bb_id = searchForBB(insnAddr);
   if (bb_id == (bbid_t)-1) {
      cout << "function::getLiveRegFnBeforeInsnAddr() -- WARNING: could not associate a basic block with address " << addr2hex(insnAddr) << endl;
      dataflowfn_t *result = 
          dataflowfn_t::getDataflowFn(*(c.getPessimisticDataflowFn(entryAddr)));
      return result;
   }
   
   const basicblock_t *bb = basic_blocks[bb_id];

   if (useCachedAnswerIfAvailable && insnAddr == bb->getStartAddr()) {
      // easy case -- user asked for regs free at the top of this bb, which has
      // already been calculated!
      const liveRegInfoPerBB &info = getLiveRegInfo(bb_id);
      dataflowfn_t *result = dataflowfn_t::getDataflowFn(info.getAtTopOfBB());
      return result;
   }

   dataflowfn_t *merge_of_succs = mergeSuccessors(bb_id, c, false);
      // simply takes the 'merge' (union) of live reg fns at the _top_ of its
      // successor basic blocks.
      // false --> barf if any successor result is undefined

   // Now walk backwards, up the basic block.  Handle calls by consulting the oracle "c"

   dataflowfn_t *effectsOfBlock =
      bb->doLiveRegAnalysisFromBottom(insnAddr, // stop up here
                                     origCode,
				     c);

   dataflowfn_t *result = effectsOfBlock->compose(*merge_of_succs);
      // do merge_of_succs first, then the effects of the block

   //we now have two dangling pointers, get rid of them
   delete effectsOfBlock;
   delete merge_of_succs;

   return result;
}

template <class calcEffectOfCall>
function_t::dataflowfn_t *
function_t::getLiveRegFnAfterInsnAddr(kptr_t insnAddr,
				      const calcEffectOfCall &c) const {
   const bbid_t bb_id = searchForBB(insnAddr);
   if (bb_id == (bbid_t)-1) {
      cout << "function::getLiveRegFnAfterInsnAddr() -- WARNING: could not associate a basic block with address " << addr2hex(insnAddr) << endl;
      dataflowfn_t *result = 
         dataflowfn_t::getDataflowFn(*(c.getPessimisticDataflowFn(entryAddr)));
      return result;
   }
   
   const basicblock_t *bb = basic_blocks[bb_id];

   dataflowfn_t *result = mergeSuccessors(bb_id, c, false);
      // simply takes the 'merge' (union) of live reg fns at the _top_ of its
      // successor basic blocks.
      // false --> barf if any successor result is undefined

   instr_t *theInsnAtInsnAddr = origCode.get1Insn(insnAddr);
   const unsigned numBytesOfInsnAtInsnAddr = theInsnAtInsnAddr->getNumBytes();

   // Is "insnAddr" the start of the last instruction in its basic block?
   const kptr_t addrJustPastInsn = insnAddr + numBytesOfInsnAtInsnAddr;
   if (addrJustPastInsn == bb->getEndAddrPlus1())
      // the easy case; don't need to walk up the basic block.
      return result;
   
   // Now walk backwards, up the basic block.  Handle calls by consulting the oracle "c"
   dataflowfn_t *effectsOfBlock =
      bb->doLiveRegAnalysisFromBottom(insnAddr + numBytesOfInsnAtInsnAddr,
                                         // stop up here (*after* insnAddr)
				      origCode,
				      c);

   dataflowfn_t *final_result = effectsOfBlock->compose(*result);
   // do result first, then the effects of the block
   
  //we now have two dangling pointers, get rid of them
   delete effectsOfBlock;
   delete result;
   
   return final_result;
}

template <class calcEffectOfCall>
pair<function_t::dataflowfn_t *, function_t::dataflowfn_t *>
function_t::getLiveRegFnBeforeAndAfterInsnAddr(kptr_t insnAddr,
					     const calcEffectOfCall &c) const {
   extern bool verbose_runtimeRegisterAnalysisTimings;

   const mrtime_t searchForBlockStartTime =
      verbose_runtimeRegisterAnalysisTimings ? getmrtime() : 0;
   
   bbid_t bb_id = searchForBB(insnAddr);
   if (bb_id == (bbid_t)-1) {
      cout << "function::getLiveRegFnBeforeAndAfterInsnAddr() -- WARNING: could not associate a basic block with address " << addr2hex(insnAddr) << endl;
      return make_pair (
          dataflowfn_t::getDataflowFn(*(c.getPessimisticDataflowFn(entryAddr))),
          dataflowfn_t::getDataflowFn(*(c.getPessimisticDataflowFn(entryAddr))));
   }
   
   const basicblock_t *bb = basic_blocks[bb_id];

   const mrtime_t searchForBlockTotalTime = verbose_runtimeRegisterAnalysisTimings ? getmrtime() - searchForBlockStartTime : 0;

   const mrtime_t mergingStartTime = verbose_runtimeRegisterAnalysisTimings ? getmrtime() : 0;
   
   dataflowfn_t *result_bottom = mergeSuccessors(bb_id, c, false);
      // simply takes the 'merge' (union) of live reg fns at the _top_ of its
      // successor basic blocks.
      // false --> barf if any successor result is undefined

   const mrtime_t mergingTotalTime = verbose_runtimeRegisterAnalysisTimings ? getmrtime()-mergingStartTime : 0;

   // Now walk up the basic block.  Handle calls by consulting the oracle "c"

   instr_t *theInsnAtInsnAddr = origCode.get1Insn(insnAddr);
   const unsigned numBytesOfInsnAtInsnAddr = theInsnAtInsnAddr->getNumBytes();
   
   const mrtime_t effectOfPartialBasicBlockStartTime = verbose_runtimeRegisterAnalysisTimings ? getmrtime() : 0;
   
   dataflowfn_t *effectsOfBlockUpToAfterTheInsn =
      bb->doLiveRegAnalysisFromBottom(insnAddr + numBytesOfInsnAtInsnAddr,
				      origCode,
				      c);

   const mrtime_t effectOfPartialBasicBlockTotalTime = verbose_runtimeRegisterAnalysisTimings ? getmrtime()-effectOfPartialBasicBlockStartTime : 0;

   dataflowfn_t *effectsOfTheInsn =
      bb->doLiveRegAnalysis(insnAddr + numBytesOfInsnAtInsnAddr,
                               // start analyzing down here
			    insnAddr, // stop analyzing up here
			    origCode,
			    c);

   dataflowfn_t *effectsOfBlockUpToBeforeTheInsn =
      effectsOfTheInsn->compose(*effectsOfBlockUpToAfterTheInsn);

   // ----------------------------------------------------------------------

   const mrtime_t composeStartTime = verbose_runtimeRegisterAnalysisTimings ? getmrtime() : 0;

   dataflowfn_t *result1 =
      effectsOfBlockUpToBeforeTheInsn->compose(*result_bottom);

   const mrtime_t composeTotalTime = verbose_runtimeRegisterAnalysisTimings ? getmrtime()-composeStartTime : 0;

   dataflowfn_t *result2 =
      effectsOfBlockUpToAfterTheInsn->compose(*result_bottom);

   // ----------------------------------------------------------------------

   // Expensive, but useful, asserts:
   assert(*result1 == *getLiveRegFnBeforeInsnAddr(insnAddr, c, false));
      // false --> do NOT used cached result, if available -- force manual calculation,
      // as we have done here.  ("Manual calculation" meaning a call to
      // doLiveRegAnalysisFromBottom().)
   assert(*result2 == *getLiveRegFnAfterInsnAddr(insnAddr, c));

   if (verbose_runtimeRegisterAnalysisTimings) {
      cout << "getLiveRegFnBeforeAndAfterInsnAddr timings [as if asking for one, not both]" << endl;
      cout << "   search for bb: " << searchForBlockTotalTime/1000 << " usecs."
           << endl;
      cout << "   merging: " << mergingTotalTime/1000 << " usecs." << endl;
      cout << "   calc effect of partial bb: " << effectOfPartialBasicBlockTotalTime/1000 << " usecs." << endl;
      cout << "   one compose: " << composeTotalTime/1000 << " usecs." << endl;
      cout << "that's all." << endl;
   }
   
   //we now have some dangling pointers, get rid of them
   delete effectsOfBlockUpToAfterTheInsn;
   delete effectsOfBlockUpToBeforeTheInsn;
   delete effectsOfTheInsn;
   delete result_bottom;

   return make_pair(result1, result2);
}

// Live register analysis, at the granularity of basic blocks:

template <class calcEffectOfCall>
function_t::dataflowfn_t *// unfortunately, cannot return a const reference
function_t::getLiveRegFnForAnExitEdge(bbid_t bb_id,
				    // the "from" basic block; will be
				    // a legitimate block
				    bbid_t succ_bb_id,
				    // will be an exit-type edge, not a legitimate block
				    const calcEffectOfCall &theOracle) const {
   // A helper routine for the next function:

   assert(bb_id != basicblock_t::xExitBBId &&
          bb_id != basicblock_t::interProcBranchExitBBId);

   if (succ_bb_id == basicblock_t::xExitBBId) {
      // maybe an interprocedural fall-thru.  But more likely, a plain return or
      // a tail call.

      kptr_t fallThruAddrIfAny = getBBInterProcFallThruAddrIfAny(bb_id);
      if (fallThruAddrIfAny != (kptr_t)-1)
         return dataflowfn_t::
            getDataflowFn(*(theOracle.whenDestAddrIsKnown(fallThruAddrIfAny)));

      // A regular return or a tail call.
      return const_cast<dataflowfn_t *>(theOracle.getIdentityDataflowFn());
   }
   else if (succ_bb_id == basicblock_t::interProcBranchExitBBId) {
      // BRAND NEW 12/2000: this case used to be handled in the basic block itself,
      // with disastrous effects when the branch was conditional instead of
      // branch-always.

      const kptr_t branchDestAddr = getBBInterProcBranchDestAddrIfAny(bb_id);
      assert(branchDestAddr != (kptr_t)-1 && "expected basic block to end in a branch");
      assert(!containsAddr(branchDestAddr) && "expected branch to be inter-procedural");
      
      return dataflowfn_t::
         getDataflowFn(*(theOracle.whenDestAddrIsKnown(branchDestAddr)));
   }
   else
      assert(false);

   return NULL; //placate compiler
}

template <class calcEffectOfCall>
function_t::dataflowfn_t * // regrettably, cannot return a const reference
function_t::calc1EdgeResult(bbid_t bb_id, bbid_t succ_bb_id,
                const calcEffectOfCall &c,
                bool optimisticIfUndefined // if false, barf if undefined
                ) const {
   if (succ_bb_id == basicblock_t::xExitBBId ||
       succ_bb_id == basicblock_t::interProcBranchExitBBId)
      return getLiveRegFnForAnExitEdge(bb_id, succ_bb_id, c);
   else if (succ_bb_id == basicblock_t::UnanalyzableBBId) {
      dataflowfn_t *result =  
         dataflowfn_t::getDataflowFn(*(c.getPessimisticDataflowFn(0)));
      return result;
   }
   else if (liveRegisters[succ_bb_id].atTopOfBBIsDefined())
      return dataflowfn_t::getDataflowFn(liveRegisters[succ_bb_id].getAtTopOfBB());
   else if (optimisticIfUndefined) {
      dataflowfn_t *result =  
         dataflowfn_t::getDataflowFn(*(c.getOptimisticDataflowFn()));
      return result;
   } else {
      assert(false);
      abort(); // placate compiler when compiling with NDEBUG
   }
}

template <class calcEffectOfCall>
function_t::dataflowfn_t *
function_t::mergeSuccessors(bbid_t bb_id, const calcEffectOfCall &c,
			    bool optimisticIfUndefined // if false, barf if undefined
			    ) const {
   // simply the 'merge' (union) of all successor basic blocks

   assert(bb_id != basicblock_t::xExitBBId &&
          bb_id != basicblock_t::interProcBranchExitBBId);

   const basicblock_t *bb = basic_blocks[bb_id];
   basicblock_t::ConstSuccIter iter   = bb->beginSuccIter();
   basicblock_t::ConstSuccIter finish = bb->endSuccIter();

   assert(iter != finish); // every block has a successor

   bbid_t succ_bb_id = *iter;

   // Take the merge of all successor basic blocks.  Note that it's possible for
   // one or more of the successors to be an exit-type basic block, which in turn
   // could be a plain return, interproc branch, interproc fall-thru, ...

   dataflowfn_t *result = calc1EdgeResult(bb_id, succ_bb_id, c, optimisticIfUndefined);
   
   ++iter; // if just 1 successor, iter will equal finish, and we'll skip the loop!

   for (; iter != finish; ++iter) {
      // This loop only gets executed if there was >1 successor.  Nice.
      succ_bb_id = *iter;

      result->mergeWith(*(calc1EdgeResult(bb_id, succ_bb_id, c, optimisticIfUndefined)),
                       c.getMergeOp());
         // Something unfortunate performance wise is going on here.  If the
         // successor block is an exit block, then it usually returns all-pass.
         // Merging with all-pass is a nop -- a waste of time.
         // 
         // On the upside, most blocks having ExitBBId as a successor have it as
         // its only successor, in which case this doesn't get executed.
   }
   
   return result;
}

#endif /* _FUNCTION_H_ */
