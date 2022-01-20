/* basicblock.h

To do list:

   1) Get rid of ExitBB and make it a real basic block (with 0 instrs, of course)
      what we could gain:
      a) be able to track the predecessors of ExitBB
      b) one fewer special case to test for
   2) Add an EntryBBPtr, to gain:
      a) the abillity to track successors of EntryBBPtr (i.e., to find all entry pts)

Notes on calls, unanalyzable calls, and interprocedural jumps/branches:
1) A call insn (and its delay slot) does not affect the basic block it belongs to.
There are no 'successor' edges to the call's destination.  Thus, (dataflow) analysis
routines must remember this fact and follow the call.

2) Unanalyzable calls (jumps which we know are calls [because they write to %o7] but
which jump to an unknown function) need runtime instrumentation code (to determine
which fn is being calls) before we can even give thought to "following" the callee.
For now, we know it's a call (that will fall through, etc.) but we don't know
what the callee will do.

3) Interprocedural branches/jumps are treated like calls; meaning there are no
out-edges to help you figure out what's going on.  That means that dataflow analysis
routines must look out for this situation and know how to follow the callee.

4) We try hard to turn unanlyzable jumps into organized jump tables, though it's
not done in this class.

5) ret and retl end a basic block.

6) The delay slot of a DCTI goes in the same block as the DCTI.  It would be nice
to put the ds on an annulled cond branch (executed only iftaken) in its own
basic block, reached only along the if-taken successor edge.  But we don't do that,
in order to save basic blocks.  Actually there's one exception to this rule: if
the annulled ds insn is a save/restore.

*/

// basicblock.h

#ifndef _BASICBLOCK_H_
#define _BASICBLOCK_H_

#include <limits.h>
#include "common/h/Vector.h"
#include "util/h/kdrvtypes.h"

#include "bitwise_dataflow_fn.h"

#include "insnVec.h"
#include "fnCode.h"

#include "bb.h"
#include "reg_set.h"

class basicblock_t {
 public:
   typedef uint16_t bbid_t;
   typedef fnCode fnCode_t;

   // for exception handling:
   class parsefailed {};
   class getinsn_failed {};

   // A basic block ends and has a certain number of successor edges.
   // Normal successor edges are the id of some other basic block in the same function.
   // Another kind is "UnanalyzableBBId", currently unused.
   // Another kind is "ReturnBBId", which is straightforward.
   // Another kind is "InterProcBranchBBId".  The edge doesn't identify the callee,
   // but it's good enough (tells you when you need to check for such a thing).
   // Another kind is "TailCallBBId".  This is a kind of exit.
   // Another kind is "A call that never returns", such as to panic.
   // Another kind is "A call that returns on our behalf", such as to .stret4/.stret8
   static bbid_t UnanalyzableBBId; // (bbid_t)-3
   
   static bbid_t interProcBranchExitBBId;
      // for when the if-taken case of a branch is interprocedural.
      // This is *not* used when the if-not-taken case of a branch falls thru
      // to the next function.

   static bbid_t xExitBBId; // XXX -- TODO -- change this to ReturnBBId, and
      // start using the others, below!!! (For now this is a catch-all and the
      // others aren't used.)
      // (bbid_t)-5.  Covers retl, ret, v9return, done, retry, and any other sequence
      // that acts like a return, except for an interprocedural branch, which is
      // covered above.

   // XXX the following are currently unused:
//     static bbid_t TailCallBBId;
//        // (bbid_t)-6.  Covers call; restore sequences and the like, including
//        // some unusual looking sequences that just jmp to the callee from a
//        // function having no stack frame.
//     static bbid_t ExitViaTailCallBBId; // (bbid_t)-7
//     static bbid_t ExitViaCallThatNeverReturnsBBId; // (bbid_t)-8
//     static bbid_t ExitViaCallThatReturnsOnOurBehalf; // (bbid_t)-9
//     static bbid_t ExitViaInterproceduralFallThru; // (bbid_t)-10
   
   // to do: add an EntryBB.

 protected:

   kptr_t startAddr;
   kptr_t lastInsnAddr;

   unsigned numInsnBytesInBB : 14;
      // 16K should be enough to hold any 1 function, and therefore
      // certainly the size of one basic block, right?
   unsigned moreThan2Succs : 1;
   unsigned moreThan2Preds : 1;
   
   // The insns of the bb are stored in the owning fn, not here (we get better memory
   // usage that way).

   // Predecessor and successor basic blocks.  Memory usage is a concern here.
   // You can run kerninstd with a "verbose_numSuccsPreds" flags.  Results are
   // something like:
   //    0 nodes having 0 successors
   //    71166 nodes having 1 successors
   //    70441 nodes having 2 successors
   //    145 nodes having >2 successors
   // 
   // The predecessors situation is not as straightforward.  We have:
   //    10490 nodes having 0 predecessors (entry basic blocks)
   //    91247 nodes having 1 predecessor
   //    31259 nodes having 2 predecessors
   //    5385 nodes having 3 predecessors
   //    1715 nodes having 4 predecessors
   //    1656 nodes having >4 predecessors

   // Organization of succs:
   // If "moreThan2Succs" is true then use actualSuccs[]
   // Else, use succ0 and (perhaps) succ1.

 public:
   
   // these unions are public due to compiler unhappiness when they are used by
   // the nested classes below ([Const]SuccIter, [Const]PredIter)
   // having those classes declared as friends doesn't seem to get the job done

   typedef struct { bbid_t succ0; bbid_t succ1; } sx;
   union {
      // Use the struct iff moreThan2Succs is false, else use actualSuccs:
      sx s;
      bbid_t *actualSuccs;
   };

   typedef struct { bbid_t pred0; bbid_t pred1; } pds;
   // if pred0 is -1, then no preds...
   // else if pred1 is -1 then there's 1 pred, else 2
   union {
      pds p;
      bbid_t *actualPreds; // (bbid_t)-1 terminated vector
   };

 protected:

   void assign(const basicblock_t &src);

 public:
   // needed by function, which uses a vector<basicblock_t>:
   basicblock_t(const basicblock_t &src);
   basicblock_t &operator=(const basicblock_t &src);

   basicblock_t(kptr_t bbStartAddr, bbid_t parent_bb_id);
      // parent_bb_id can be (bbid_t)-1 to indicate no parents (yet).
   static basicblock_t* getBasicBlock(kptr_t bbStartAddr, bbid_t parent_bb_id);

   class Split{};
   basicblock_t(Split, bbid_t parent_bb_id, basicblock_t *parent,
		kptr_t splitAddr, const fnCode_t &);
      // split ctor.  Needs to be public since function uses it.
   static basicblock_t* getBasicBlock(Split, bbid_t parent_bb_id, 
				      basicblock_t *parent,
				      kptr_t splitAddr, const fnCode_t &);

   basicblock_t(kptr_t bbStartAddr, kptr_t bbLastInsnAddr,
		uint16_t bbNumInsnBytes, const fnCode_t &);

   basicblock_t(XDR *);
   static basicblock_t* getBasicBlock(XDR *);

   virtual ~basicblock_t();

   bool send(XDR *) const;

   //   virtual size_t getSize() = 0; // arch specific sizeof(basicblock_t)

   void appendToBBContents(const instr_t *, kptr_t chunkStartAddr,
                           const insnVec_t &chunkContents);

   void bumpUpNumInsnsBy1(uint16_t numInsnBytes) {
      lastInsnAddr = startAddr + numInsnBytesInBB;
      numInsnBytesInBB += numInsnBytes;
   }

   void trimNumInsnBytesTo(uint16_t newval) {
      assert(newval < numInsnBytesInBB);
      numInsnBytesInBB = newval;
   }

   kptr_t getStartAddr() const {return startAddr;}
   kptr_t getLastInsnAddr() const {return lastInsnAddr;}
   kptr_t getEndAddr()   const {return startAddr+numInsnBytesInBB-1;}
   kptr_t getEndAddrPlus1() const { return startAddr + numInsnBytesInBB; }
      // This is in the spirit of STL iterators, returning an address *just* beyond
      // the end of this basic block.  Assuming for the moment that what follows this
      // block is another basic block, this routine returns the start address of
      // that basic block.
   bool containsAddr(kptr_t addr) const {
      return addr >= startAddr && addr < getEndAddrPlus1();
   }

   unsigned getNumInsnBytes()  const { return numInsnBytesInBB; }

   bool addPredecessorUnlessDup(bbid_t bb_id);
      // if it doesn't already exist in the set 'predecessors', add it.
      // Returns true if added; false if not (since would be a duplicate)
   void addPredecessorNoDup(bbid_t bb_id);
      // same as above, but assert fail if already exists.
   void clearAllPredecessors();
   void replace1Predecessor(bbid_t oldBBId, bbid_t replacementBBId);
   void erase1Predecessor(bbid_t old_bb_id);

   bool addSuccessorUnlessDup(bbid_t bb_id);
      // if it doesn't already exist in the set 'successors', add it.
      // Returns true if added; false if not (since would be a duplicate)
   void addSuccessorNoDup(bbid_t bb_id);
      // Same as above, but the caller's certain that it doen't already exist.
      // If it already exists, assert fail.
   void clearAllSuccessors();

   bool hasAsAPredecessor(bbid_t id) const; // try to use iterators instead
   bool hasAsASuccessor(bbid_t id) const; // try to use iterators instead

   unsigned numPredecessors() const; // try to use iterators instead
   bool hasMoreThan2Preds() const {
      return moreThan2Preds;
   }
   unsigned numSuccessors() const; // try to use iterators instead
   bool hasMoreThan2Succs() const {
      return moreThan2Succs;
   }

   // Unfortunately, basicblock.C still uses getsucc(0) and getpred(0) so we can't
   // fry 'em.
   bbid_t getpred(unsigned ndx) const; // use iterators instead
   bbid_t getsucc(unsigned ndx) const; // use iterators instead

   class SuccIter {
    private:
      basicblock_t &parentbb; // operator++() needs it
      bbid_t *curr; // *curr == (bbid_t)-1 --> end of iteration
      static bbid_t end;

      SuccIter &operator=(const SuccIter &);
    public:
      SuccIter(const SuccIter &src) : parentbb(src.parentbb), curr(src.curr) {}
      SuccIter(basicblock_t& iparentbb) : parentbb(iparentbb) {
         if (parentbb.hasMoreThan2Succs())
            // use actualSuccs instead
            curr = &parentbb.actualSuccs[0];
         else
            curr = &parentbb.s.succ0; // works if (bbid_t)-1 too
      }
      SuccIter(basicblock_t& iparentbb, void *) : parentbb(iparentbb), curr(&end) { }
      
      bbid_t operator*() const { // returns bb id
         return *curr;
      }
      
      void operator++() {
         if (parentbb.hasMoreThan2Succs())
            // using actualSuccs
            curr++; // within actualSuccs[]
         else if (parentbb.s.succ0 == (bbid_t)-1)
            assert(0); // no succs; can't move to next one
         else {
            if (curr == &parentbb.s.succ0)
               curr = &parentbb.s.succ1;
            else if (curr == &parentbb.s.succ1)
               // since not using actualSuccs, we want end now.
               curr = &end;
            else
               assert(0);
         }
      }
   
      bool operator==(const SuccIter &other) const { return (*curr == *other.curr); }
      bool operator!=(const SuccIter &other) const { return (*curr != *other.curr); }
   };
   class ConstSuccIter {
    private:
      const basicblock_t &parentbb;
      const bbid_t *curr; // *curr==NULL --> end of iteration.
      static const bbid_t end;

      ConstSuccIter &operator=(const ConstSuccIter &);

    public:
      ConstSuccIter(const ConstSuccIter &src) : parentbb(src.parentbb),
         curr(src.curr) {}
      ConstSuccIter(const basicblock_t &iparentbb) : parentbb(iparentbb) {
         if (parentbb.hasMoreThan2Succs())
            curr = &parentbb.actualSuccs[0];
         else
            curr = &parentbb.s.succ0; // works if it's -1 too
      }
      ConstSuccIter(const basicblock_t &iparentbb, void *) :
         parentbb(iparentbb), curr(&end) { }
      
      bbid_t operator*() const { return *curr; }
      
      void operator++() {
         if (parentbb.hasMoreThan2Succs()) {
            assert(*curr != (bbid_t)-1); // verify that we haven't gone too far
            ++curr; // within actualSuccs[]
         }
         else if (parentbb.s.succ0 == (bbid_t)-1)
            assert(0); // no succs; can't move to next one
         else {
            if (curr == &parentbb.s.succ0)
               curr = &parentbb.s.succ1;
            else if (curr == &parentbb.s.succ1)
               // since not using actualSuccs[], we want end now
               curr = &end;
            else
               assert(0);
         }
      }
   
      bool operator==(const ConstSuccIter &other) const {
         return (*curr == *other.curr);
      }
      bool operator!=(const ConstSuccIter &other) const {
         return (*curr != *other.curr);
      }
   };

   class PredIter {
    private:
      basicblock_t &parentbb;
      bbid_t *curr; // *curr==-1 --> end of iteration.
      static bbid_t end;

      PredIter &operator=(const PredIter &);
      
    public:
      PredIter(const PredIter &src) : parentbb(src.parentbb), curr(src.curr) {}
      PredIter(basicblock_t &iparentbb) : parentbb(iparentbb) {
         if (parentbb.hasMoreThan2Preds())
            curr = &parentbb.actualPreds[0];
         else
            curr = &parentbb.p.pred0; // works if it's -1 too
      }
      PredIter(basicblock_t &iparentbb, void *) : parentbb(iparentbb), curr(&end) { }
      
      bbid_t &operator*() const { return *curr; }
         // must be a reference so we can write to it (think of replace1Predecessor())
      
      void operator++() {
         assert(*curr != (bbid_t)-1); // verify we haven't gone too far
         
         if (parentbb.hasMoreThan2Preds())
            ++curr; // within actualPreds[]
         else if (curr == &parentbb.p.pred0)
            curr = &parentbb.p.pred1;
         else if (curr == &parentbb.p.pred1)
            curr = &end;
         else
            assert(false);
      }
   
      bool operator==(const PredIter &other) const { return (*curr == *other.curr); }
      bool operator!=(const PredIter &other) const { return (*curr != *other.curr); }
   };
   class ConstPredIter {
    private:
      const basicblock_t &parentbb;
      const bbid_t *curr; // *curr==NULL --> end of iteration.
      static bbid_t end;

      ConstPredIter &operator=(const ConstPredIter &);
      
    public:
      ConstPredIter(const ConstPredIter &src) :
         parentbb(src.parentbb), curr(src.curr) {}
      ConstPredIter(const basicblock_t &iparentbb) : parentbb(iparentbb) {
         if (parentbb.hasMoreThan2Preds())
            curr = &parentbb.actualPreds[0];
         else
            curr = &parentbb.p.pred0; // works if it's -1 too
      }
      ConstPredIter(const basicblock_t &iparentbb, void *) :
         parentbb(iparentbb), curr(&end) { }
      
      bbid_t operator*() const { return *curr; }
      
      void operator++() {
         assert(*curr != (bbid_t)-1); // verify we haven't gone too far
         
         if (parentbb.hasMoreThan2Preds())
            ++curr; // within actualPreds[]
         else if (curr == &parentbb.p.pred0)
            curr = &parentbb.p.pred1;
         else if (curr == &parentbb.p.pred1)
            curr = &end;
         else
            assert(false);
      }
   
      bool operator==(const ConstPredIter &other) const {
         return (*curr == *other.curr);
      } 
      bool operator!=(const ConstPredIter &other) const {
         return (*curr != *other.curr);
      }
   };

   SuccIter beginSuccIter()            { return SuccIter(*this); }
   ConstSuccIter beginSuccIter() const { return ConstSuccIter(*this); }

   SuccIter endSuccIter()            { return SuccIter(*this, NULL); }
   ConstSuccIter endSuccIter() const { return ConstSuccIter(*this, NULL); }
   
   PredIter beginPredIter()            { return PredIter(*this); }
   ConstPredIter beginPredIter() const { return ConstPredIter(*this); }

   PredIter endPredIter()            { return PredIter(*this, NULL); }
   ConstPredIter endPredIter() const { return ConstPredIter(*this, NULL); }

   virtual kptr_t getExitPoint(const fnCode_t &fnCode) const = 0;

#ifdef _KERNINSTD_
   pdvector<bool> backwardsSlice(kptr_t succ_bb_startAddr, // -1U if none in slice yet
			         regset_t &regsToSlice,
			         bool includeControlDependencies,
			         const fnCode_t &fnContents,
			         uint16_t numInsnBytesToSkipAtEnd=0) const {
       return arch_bb_t::backwardsSlice(succ_bb_startAddr,
					regsToSlice,
					includeControlDependencies,
					fnContents,
					numInsnBytesToSkipAtEnd,
					numInsnBytesInBB,
					startAddr,
					getEndAddrPlus1());
   }

   template <class calcEffectOfCall>
   monotone_bitwise_dataflow_fn *
   doLiveRegAnalysis(kptr_t addrWhereToStart,
                      // This is just *beyond* (STL iterator style) the highest
                      // insn where we should begin analyzing.  Of course, since
                      // dataflow analysis is a backwards problem, we walk *up* a
                      // basic block, so addrWhereToStart is >= addrWhereToStop.
                     kptr_t addrWhereToStop, // must be <= addrWhereToStart
                     const fnCode_t &theFnCode,
                     const calcEffectOfCall &c) const {

      assert(addrWhereToStart >= addrWhereToStop);
      return arch_bb_t::doLiveRegAnalysis(addrWhereToStart,
                                          addrWhereToStop,
                                          theFnCode,
                                          c,
                                          numInsnBytesInBB,
					  startAddr,
					  getEndAddrPlus1(),
					  numSuccessors(),
					  getsucc(0));
   }

   template <class calcEffectOfCall>
   monotone_bitwise_dataflow_fn *
   doLiveRegAnalysisWholeBlock(const fnCode_t &theFnCode,
			       const calcEffectOfCall &c) const {
      return doLiveRegAnalysis(startAddr + getNumInsnBytes(), // just beyond end of bb
                               startAddr, // start of bb
                               theFnCode, c);
   }

   template <class calcEffectOfCall>
   monotone_bitwise_dataflow_fn *
   doLiveRegAnalysisFromBottom(kptr_t addrWhereToStop,
                               const fnCode_t &theFnCode,
                               const calcEffectOfCall &c) const {
      const kptr_t addrJustPastEndOfBlock = startAddr + getNumInsnBytes();
      return doLiveRegAnalysis(addrJustPastEndOfBlock,
                               addrWhereToStop,
                                  // usually higher than bb->startAddr; otherwise,
                                  // the caller would have invoked
                                  // doLiveRegAnalysisWholeBlock(), presumably.
                                  // We don't assert this, however (Draconian).
                               theFnCode,
                               c);
   }

   void factorInControlDependencyInRegsToSlice(regset_t &regsToSlice,
                                               const fnCode_t::codeChunk &theCodeChunk) const {
      return arch_bb_t::factorInControlDependencyInRegsToSlice(regsToSlice,
                                                               theCodeChunk,
							       startAddr,
							       getEndAddrPlus1());
   }
#endif /* _KERNINSTD_ */

};

#endif /* _BASICBLOCK_H_ */
