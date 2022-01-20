// parseOracle.h - abstract base class for function parsing oracles

#ifndef _PARSE_ORACLE_H_
#define _PARSE_ORACLE_H_

#include "funkshun.h"
#include "simplePathSlice.h"
#include "instr.h" // unfortunately needed; fwd decl can't work for 
                   // nested controlFlowInfo class

class parseOracle {
 public:
   typedef function_t::bbid_t bbid_t;
   typedef simplePathSlice simplePathSlice_t;
   typedef instr_t::controlFlowInfo controlFlowInfo_t;
 protected:
   function_t &fn;

   // On a function-wide basis, we need to track jump table entries, in order 
   // to properly detect "collisions" necessitating a jump table truncation.
   pdvector<kptr_t> allJumpTableStartAddrs; // kept sorted
   virtual void adjustForNewJumpTableStartAddr(kptr_t) = 0;
   unsigned getMaxJumpTableNumBytesBasedOnOtherJumpTableStartAddrs(kptr_t) const;

 public:
   parseOracle(function_t &ifn) : fn(ifn) {}
   virtual ~parseOracle() {}
   virtual bool hasCFGBeenFullyParsedYet() const = 0;
   const function_t *getFn() const { return &fn; }
   function_t *getMutableFn() const { return &fn; }

   virtual void ensureBlockStartInsnIsAlsoBlockOnlyInsn(kptr_t addr,
							instr_t *insn) const = 0;
   virtual void processInterproceduralFallThru(bbid_t bb_id) = 0;
   virtual void processStumbleOntoExistingBlock(bbid_t bb_id,
						bbid_t stumble_bb_id,
						const simplePath &) = 0;
   virtual void 
   processSeveralNonControlFlowInsnsAsInsnVecSubset(bbid_t bb_id,
						    kptr_t startAddr,
						    unsigned numInsns) const = 0;

   virtual void 
   processCallToRoutineReturningOnCallersBehalf(bbid_t bb_id,
						instr_t *insn,
						kptr_t callerAddr,
						kptr_t calleeAddr,
						bool delaySlotInSameBlock,
						instr_t *dsInsn
						) = 0;

   virtual void processCallToObtainPC(bbid_t bb_id,
				      instr_t *callInstr,
				      kptr_t callInsnAddr,
				      bool delaySlotInSameBlock,
				      instr_t *dsInsn) = 0;

   virtual void noteHiddenIntraProceduralFn(kptr_t calleeAddr) = 0;
     // doesn't process the call; that'll happen with some other "processXXX()"
     // routine.  In particular, this routine doesn't add the call insn to the
     // basic block.

   virtual void processCallRecursion(bbid_t bb_id, kptr_t calleeAddr,
				     instr_t *callInsr,
				     kptr_t instrAddr,
				     bool delaySlotInSameBlock,
				     instr_t *dsInsn,
				     const simplePath &pathFromEntryBB
                                       // doesn't include this block yet
				     ) = 0;
      // a recursive call.
   virtual void processReturn(bbid_t bb_id, instr_t *insn,
			      kptr_t insnAddr,
			      bool delaySlotInSameBlock,
			      instr_t *dsInsn) = 0;

   virtual void processJmpBehavingAsRet(bbid_t bb_id, 
					instr_t *insn,
					kptr_t insnAddr,
					bool delaySlotInSameBlock,
					instr_t *dsInsn) = 0;

   virtual void processDoneOrRetry(bbid_t bb_id, instr_t *insn,
				   kptr_t insnAddr) = 0;

   virtual void
   processIndirectTailCall(bbid_t bb_id, instr_t *jmpInsn,
			   kptr_t insnAddr,
			   bool delaySlotInSameBlock,
			   instr_t *dsInsn) const = 0;

   virtual void 
   processJmplRestoreOptimizedTailCall(bbid_t bb_id,
				      instr_t *instr,
				      kptr_t insnAddr,
				      bool delaySlotInSameBlock,
				      instr_t *dsInsn) const = 0;

   virtual void processCondBranch(bbid_t bb_id,
				  bool branchAlways, bool branchNever,
				  instr_t *i,
				  controlFlowInfo_t *cfi,
				  kptr_t instrAddr,
				  bool hasDelaySlotInsn,
				  bool delaySlotInThisBlock,
				  instr_t *dsInsn,
				  kptr_t fallThroughAddr,
				  kptr_t destAddr,
				  const simplePath &pathFromEntryBB) = 0;

   virtual void processInterproceduralBranch(bbid_t bb_id, 
					     kptr_t destAddr,
					     instr_t *i,
					     const controlFlowInfo_t &cfi,
					     bool branchAlways,
					     kptr_t instrAddr,
					     bool delaySlotInSameBlock,
					     instr_t *dsInsn) = 0;

   virtual void 
   processIntraproceduralJmpToFixedAddr(bbid_t bb_id,
					instr_t *instr,
					kptr_t jmpInsnAddr,
					bool delaySlotInSameBlock,
					instr_t *dsInsn,
					kptr_t destAddr,
					const simplePathSlice_t &jmpRegSlice,
					const simplePath &pathFromEntryBB) = 0;

   virtual void 
   processInterproceduralJmpToFixedAddr(bbid_t bb_id,
					instr_t *instr,
					bool delaySlotInSameBlock,
					instr_t *dsInsn,
					kptr_t destAddr,
					const simplePath &pathFromEntryBB) = 0;

   virtual void 
   processTrueCallRestore(bbid_t bb_id, instr_t *callInstr,
			  kptr_t instrAddr,
			  kptr_t destAddr,
			  bool nonTrivialRestore,
			  bool dangerousRestore,
			  const reg_t *regWrittenByRestore,
			  bool delaySlotInSameBlock,
			  instr_t *dsInsn) = 0;

   virtual void 
   processJmplCallRestore(bbid_t bb_id, instr_t *callInstr,
			  kptr_t instrAddr,
			  bool nonTrivialRestore,
			  bool dangerousRestore,
			  const reg_t *rdOfRestore,
			  bool delaySlotInSameBlock,
			  instr_t *dsInsn) = 0;

   virtual void 
   processNonTailOptimizedTrueCall(bbid_t bb_id, 
				   instr_t *callInstr,
				   kptr_t instrAddr,
				   kptr_t destAddr,
				   bool delaySlotInSameBlock,
				   instr_t *delaySlotInsn
				   ) = 0;
   
   virtual void
   processNonTailOptimizedJmplCall(bbid_t bb_id, 
				   instr_t *callInstr,
				   kptr_t instrAddr,
				   const bool delaySlotInSameBlock,
				   instr_t *delaySlotInsn
				   ) = 0;

   // the following are sparc-specific, but need to be defined in
   // the generic interface anyways, since sparc-specific functions that 
   // take a parseOracle* may call them
   virtual void processV9Return(bbid_t /*bb_id*/, instr_t */*insn*/,
				kptr_t /*insnAddr*/,
				bool /*delaySlotInSameBlock*/,
				instr_t */*delayInsn*/) {}

   virtual void processTrueCallThatWritesToO7InDelay(
                                           bbid_t /*bb_id*/,
					   instr_t */*callInstr*/,
					   kptr_t /*instrAddr*/,
					   kptr_t /*destAddr*/,
					   bool /*delaySlotInSameBlock*/,
					   instr_t */*dsInsn*/) {}

   virtual void processJmplThatWritesToO7InDelay(bbid_t /*bb_id*/, 
						 instr_t */*callInstr*/,
						 kptr_t /*instrAddr*/,
						 bool /*delaySlotInSameBlock*/,
						 instr_t */*dsInsn*/) {}

   // ---------- Simple Jump Table 32: ----------

   virtual void
   prepareSimpleJumpTable32(bbid_t bb_id, instr_t *jmpInsn,
			    kptr_t jmpInsnAddr,
			    bool delaySlotInThisBlock,
			    instr_t *delaySlotInsn,
			    kptr_t jumpTableDataStartAddr,
			    const simplePathSlice_t &settingJumpTableStartAddrSlice) = 0;

   virtual void 
   process1SimpleJumpTable32Entry(bbid_t bb_id,
				  kptr_t theJumpTableEntry,
				  const simplePath &pathFromEntryBB) = 0;

   virtual void
   completeSimpleJumpTable32(bbid_t bb_id, 
			     kptr_t jumpTableStartAddr) = 0;

   // ---------- Tagged Jump Table 32: ----------

   virtual void prepareTaggedJumpTable32(bbid_t bb_id,
					 kptr_t jumpTableStartAddr,
					 instr_t *jmpInsn,
					 bool delaySlotInThisBlock,
					 instr_t *delaySlotInsn) const = 0;

   virtual void 
   process1TaggedJumpTable32Entry(bbid_t bb_id,
				  uint32_t theTag,
				  kptr_t theDestAddr,
				  const simplePath &pathFromEntryBB) = 0;

   virtual void 
   completeTaggedJumpTable32(bbid_t bb_id, 
			     kptr_t jumpTableStartAddr) = 0;
   
   // ---------- Non-Loading Backwards Jump Table: ----------

   virtual void 
   processNonLoadingBackwardsJumpTable(bbid_t bb_id,
				       kptr_t jumpTableStartAddr,
				       kptr_t jumpTableEndAddr,
				       instr_t *jmpInsn,
				       bool delaySlotInThisBlock,
				       instr_t *delaySlotInsn,
				       const simplePathSlice_t &justPastEndOfJmpTableSlice,
				       const simplePath &) = 0;
                                            
   // ---------- Offset Jump Table 32: ----------

   virtual void
   processOffsetJumpTable32WithTentativeBounds(
      bbid_t bb_id,
      instr_t *instr,
      kptr_t jmpInsnAddr,
      bool delaySlotInThisBlock,
      instr_t *delaySlotInsn,
      kptr_t jumpTableStartAddr,
      const pdvector<kptr_t> &,
      // jump table block addrs
      // tentative!! May be too many!!!
      const simplePathSlice_t &theSlice,
      const simplePath &pathFromEntryBB) = 0;
};

#endif
