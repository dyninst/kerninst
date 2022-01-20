// codeObjectCreationOracle.h

#ifndef _CODE_OBJECT_CREATION_ORACLE_H_
#define _CODE_OBJECT_CREATION_ORACLE_H_

#include "parseOracle.h"

#include "fnCodeObjects.h"
#include "moduleMgr.h"
#include "jumpTableCodeObject.h"

class codeObjectCreationOracle : public parseOracle {
 protected:
   const moduleMgr &theModuleMgr;

   fnCodeObjects &theFnCodeObjects;
   dictionary_hash<kptr_t, pdstring> fnsBeingRelocated;
     // key: (original) addr of a fn being relocated
     // name: the soon-to-be name of the relocated fn; format - 
     //       "modName/fnName", e.g. "outlined/genunix:kmem_cache_alloc"
   bool verbose_fnParse;

   // --------------------

   // On a function-wide basis, we need to track jump table entries, in order
   // to properly detect "collisions" necessitating a jump table truncation.
   pdvector<jumpTableCodeObject*> allJumpTableCodeObjects;
   void adjustForNewJumpTableStartAddr(kptr_t jumpTableStartAddr);
   // Adds to allJumpTableStartAddrs[], then goes through 
   // allJumpTableCodeObjects[], perhaps trimming some of the jump tables.

   // --------------------

   pair<unsigned, unsigned>
   getCodeObjNdxWithinBBAndInsnByteOffsetWithinCodeObj(bbid_t bbid,
                                                       unsigned insnByteOffsetWithinBB) const;

   // --------------------

   void appendCodeObjectToBasicBlock(bbid_t bb_id,
                                     unsigned byteOffsetWithinBB,
                                     codeObject *co) const;
   void endBasicBlock(bbid_t bb_id) const;
   void parseBlockIfNeeded(bbid_t bb_id, const simplePath &);

   virtual void processBranchDelayInDifferentBlock(kptr_t branchInstrAddr,
						   bbid_t bb_id,
						   bbid_t iftaken_bb_id,
						   const simplePath &pathFromEntryBB) = 0;

   void processIntraProcCondBranch(bbid_t bb_id, bool branchAlways, bool branchNever,
                                   instr_t *i,
                                   instr_t::controlFlowInfo *cfi,
                                   kptr_t instrAddr,
                                   bool hasDelaySlotInsn,
                                   bool delaySlotInThisBlock,
                                   /*bool delaySlotInItsOwnBlock,*/
                                   instr_t *dsInsn,
                                   kptr_t fallThroughAddr,
                                   kptr_t destAddr,
                                   const simplePath &pathFromEntryBB);
   void processInterProcCondBranch(bbid_t bb_id, bool branchAlways, 
				   bool branchNever,
                                   instr_t *i,
                                   instr_t::controlFlowInfo *cfi,
                                   kptr_t instrAddr,
                                   bool hasDelaySlotInsn,
                                   bool delaySlotInThisBlock,
                                   /*bool delaySlotInOwnBlock,*/
                                   instr_t *dsInsn,
                                   kptr_t fallThroughAddr,
                                   kptr_t destAddr,
                                   const simplePath &pathFromEntryBB);

   codeObjectCreationOracle(const codeObjectCreationOracle &);
   codeObjectCreationOracle &operator=(const codeObjectCreationOracle &);
   
 public:
   codeObjectCreationOracle(const moduleMgr &imoduleMgr,
                            function_t &ifn,
                            fnCodeObjects &ifnCodeObjects,
                            const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated,
                            bool verbose_fnParse);
   virtual ~codeObjectCreationOracle();

   static codeObjectCreationOracle * 
   getCodeObjectCreationOracle(const moduleMgr &imoduleMgr,
			       function_t &ifn,
			       fnCodeObjects &ifnCodeObjects,
			       const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated,
			       bool verbose_fnParse);

   bool hasCFGBeenFullyParsedYet() const {
      return true;
   }

   void ensureBlockStartInsnIsAlsoBlockOnlyInsn(kptr_t addr,
						instr_t *i) const;
   void processSeveralNonControlFlowInsnsAsInsnVecSubset(bbid_t bb_id,
							 kptr_t startAddr,
							 unsigned numInsns) const;
   void processInterproceduralFallThru(bbid_t bb_id);
   void processStumbleOntoExistingBlock(bbid_t bb_id,
					bbid_t stumble_bb_id,
					const simplePath &thePath);
   void processCallToObtainPC(bbid_t bb_id,
			      instr_t */*callInsn*/,
			      kptr_t callInsnAddr,
			      bool delaySlotInSameBlock,
			      instr_t *dsInsn);
   void noteHiddenIntraProceduralFn(kptr_t addr);
   void processInterproceduralBranch(bbid_t, kptr_t,
				     instr_t *,
				     const controlFlowInfo_t &,
				     bool,
				     kptr_t,
				     bool,
				     instr_t *) {}
   void processInterproceduralJmpToFixedAddr(bbid_t,
					     instr_t *,
					     bool,
					     instr_t *,
					     kptr_t,
					     const simplePath &) {}
   void processTrueCallRestore(bbid_t bb_id,
			       instr_t */*callInstr*/,
			       kptr_t instrAddr,
			       kptr_t destAddr,
			       bool /*nonTrivialRestore*/,
			       bool /*dangerousRestore*/,
			       const reg_t */*regWrittenByRestore*/,
			       bool delaySlotInSameBlock,
			       instr_t *dsInsn);
   void processJmplCallRestore(bbid_t bb_id, instr_t *callInstr,
			       kptr_t instrAddr,
			       bool /*nonTrivialRestore*/,
			       bool /*dangerousRestore*/,
			       const reg_t */*rdOfRestore*/,
			       bool delaySlotInSameBlock,
			       instr_t *dsInsn);
   void processNonTailOptimizedTrueCall(bbid_t bb_id,
					instr_t *callInstr,
					kptr_t instrAddr,
					kptr_t destAddr,
					bool delaySlotInSameBlock,
					instr_t *dsInsn);
   void processNonTailOptimizedJmplCall(bbid_t bb_id,
					instr_t *callInstr,
					kptr_t instrAddr,
					bool delaySlotInSameBlock,
					instr_t *dsInsn);
   void processReturn(bbid_t bb_id, instr_t *insn, kptr_t insnAddr,
		      bool delaySlotInSameBlock, instr_t *dsInsn);
   void processJmpBehavingAsRet(bbid_t bb_id, instr_t *insn,
				kptr_t insnAddr,
				bool delaySlotInSameBlock,
				instr_t *dsInsn);
   void processDoneOrRetry(bbid_t bb_id, instr_t *insn, 
			   kptr_t insnAddr);
   void processCondBranch(bbid_t bb_id,
			  bool branchAlways, bool branchNever,
			  instr_t *i,
			  instr_t::controlFlowInfo *cfi,
			  kptr_t instrAddr,
			  bool hasDelaySlotInsn,
			  bool delaySlotInThisBlock,
			  instr_t *dsInsn,
			  kptr_t fallThroughAddr,
			  kptr_t destAddr,
			  const simplePath &pathFromEntryBB);

   void processJmplRestoreOptimizedTailCall(bbid_t bb_id,
					   instr_t *instr,
					   kptr_t insnAddr,
					   bool delaySlotInSameBlock,
					   instr_t *dsInsn) const;
   void processIndirectTailCall(bbid_t, instr_t *, kptr_t, bool, 
				instr_t *) const;

   void process1SimpleJumpTable32Entry(bbid_t our_block_id,
				       kptr_t nextJumpTableEntry,
				       const simplePath &pathFromEntryBB);
   void completeSimpleJumpTable32(bbid_t, kptr_t);
   void prepareTaggedJumpTable32(bbid_t, kptr_t, instr_t *, bool, 
				 instr_t *) const;
   void process1TaggedJumpTable32Entry(bbid_t, uint32_t, kptr_t,
				       const simplePath &);
   void completeTaggedJumpTable32(bbid_t, kptr_t);
};

#endif
