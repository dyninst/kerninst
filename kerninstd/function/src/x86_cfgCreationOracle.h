// x86_cfgCreationOracle.h

#ifndef _X86_CFG_CREATION_ORACLE_H_
#define _X86_CFG_CREATION_ORACLE_H_

#include "cfgCreationOracle.h"

class x86_cfgCreationOracle : public cfgCreationOracle {
 public:
   x86_cfgCreationOracle(function_t &ifn,
			 dictionary_hash<kptr_t, bool> &iinterProcJumpsAndCalls,
			 pdvector<hiddenFn> &ihiddenFnsToParse,
			 const kmem *ikm,
			 bool iverbose, bool iverbose_hiddenFns,
			 bbParsingStats &iBBParsingStats) :
      cfgCreationOracle(ifn, iinterProcJumpsAndCalls, ihiddenFnsToParse,
			ikm, iverbose, iverbose_hiddenFns, 
			iBBParsingStats) {}

   ~x86_cfgCreationOracle() {}

   void processCallRecursion(bbid_t bb_id, kptr_t calleeAddr,
                             instr_t *callInsr,
                             kptr_t instrAddr,
                             bool delaySlotInSameBlock,
                             instr_t *dsInsn,
                             const simplePath & pathFromEntryBB
			        // doesn't include this block yet
                             );

   void processInterproceduralBranch(bbid_t bb_id, kptr_t destAddr,
                                     instr_t *i,
                                     const controlFlowInfo_t &cfi,
                                     bool branchAlways,
                                     kptr_t instrAddr,
                                     bool delaySlotInSameBlock,
                                     instr_t *dsInsn);
   
   void processCondBranch(bbid_t bb_id,
			  bool branchAlways, bool branchNever,
			  instr_t *i,
			  controlFlowInfo_t *cfi,
			  kptr_t instrAddr,
			  bool hasDelaySlotInsn,
			  bool delaySlotInThisBlock,
			  instr_t *delaySlotInsn,
			  kptr_t fallThroughAddr,
			  kptr_t destAddr,
			  const simplePath &pathFromEntryBB);

   void processNonTailOptimizedTrueCall(bbid_t bb_id, 
					instr_t *callInstr,
					kptr_t instrAddr,
					kptr_t destAddr,
					bool delaySlotInSameBlock,
					instr_t *delaySlotInsn
					);
   
   void processNonTailOptimizedJmplCall(bbid_t bb_id, 
					instr_t *callInstr,
					kptr_t instrAddr,
					const bool delaySlotInSameBlock,
					instr_t *delaySlotInsn
					);

   void processIndirectTailCall(bbid_t bb_id, 
				instr_t *jmpInsn,
				kptr_t insnAddr,
				bool delaySlotInSameBlock,
				instr_t *dsInsn) const;

   void 
   processIntraproceduralJmpToFixedAddr(bbid_t bb_id,
					instr_t *instr,
					kptr_t jmpInsnAddr,
					bool delaySlotInSameBlock,
					instr_t *dsInsn,
					kptr_t destAddr,
					const simplePathSlice_t &jmpRegSlice,
					const simplePath &pathFromEntryBB);

   void 
   processInterproceduralJmpToFixedAddr(bbid_t bb_id,
					instr_t *instr,
					bool delaySlotInSameBlock,
					instr_t *dsInsn,
					kptr_t destAddr,
					const simplePath &pathFromEntryBB);

   // ---------- Non-Loading Backwards Jump Table: ----------
   void processNonLoadingBackwardsJumpTable(bbid_t bb_id,
					    kptr_t jumpTableStartAddr,
					    kptr_t jumpTableEndAddr,
					    instr_t *jmpInsn,
					    bool delaySlotInThisBlock,
					    instr_t *delaySlotInsn,
					    const simplePathSlice_t &justPastEndOfJmpTableSlice,
					    const simplePath &);

   // ---------- Offset Jump Table 32: ----------
   void processOffsetJumpTable32WithTentativeBounds(
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
				  const simplePath &pathFromEntryBB);

};

#endif // _X86_CFG_CREATION_ORACLE_H_
