#ifndef _SPARC_CODE_OBJECT_CREATION_ORACLE_H_
#define _SPARC_CODE_OBJECT_CREATION_ORACLE_H_

#include "codeObjectCreationOracle.h"
#include "common/h/triple.h"

class sparc_codeObjectCreationOracle : public codeObjectCreationOracle {
 protected:
   pair< triple<bbid_t,unsigned,unsigned>, triple<bbid_t,unsigned,unsigned> >
   getSetHiAndBSetOrAddCodeObjRelativeInfo(const simplePathSlice_t &theSlice) const;
   
 public:
   sparc_codeObjectCreationOracle(const moduleMgr &imoduleMgr,
				  function_t &ifn,
				  fnCodeObjects &ifnCodeObjects,
				  const pdvector<fnAddrBeingRelocated> &fnAddrsBeingRelocated,
				  bool verbose_fnParse) :
     codeObjectCreationOracle(imoduleMgr, ifn, ifnCodeObjects, fnAddrsBeingRelocated, verbose_fnParse) {}

   ~sparc_codeObjectCreationOracle() {}

   void processCallToRoutineReturningOnCallersBehalf(bbid_t bb_id,
						     instr_t */*callInstr*/,
						     kptr_t callInsnAddr,
						     kptr_t destAddr,
						     bool delaySlotInSameBlock,
						     instr_t *delaySlotInsn);

   void processCallRecursion(bbid_t bb_id,
			     kptr_t /*calleeAddr*/,
			     instr_t */*callInstr*/,
			     kptr_t instrAddr,
			     bool delaySlotInSameBlock,
			     instr_t *dsInsn,
			     const simplePath &pathFromEntryBB);

   void processTrueCallThatWritesToO7InDelay(bbid_t bb_id,
                                             instr_t *callInstr,
                                             kptr_t instrAddr,
                                             kptr_t destAddr,
                                             bool delaySlotInSameBlock,
                                             instr_t *dsInsn) const;

   void processJmplThatWritesToO7InDelay(bbid_t bb_id, 
					 instr_t *callInstr,
                                         kptr_t instrAddr,
                                         bool delaySlotInSameBlock,
                                         instr_t *dsInsn) const;

   void processBranchDelayInDifferentBlock(kptr_t branchInstrAddr,
					   bbid_t bb_id,
					   bbid_t iftaken_bb_id,
					   const simplePath &pathFromEntryBB);

   void processIntraproceduralJmpToFixedAddr(bbid_t bb_id,
					     instr_t *jmp_insn,
					     kptr_t jmpInsnAddr,
					     bool delaySlotInSameBlock,
					     instr_t *dsInsn,
					     kptr_t destAddr,
					     const simplePathSlice_t &jmpRegSlice,
					     const simplePath &pathFromEntryBB);

   void processV9Return(bbid_t bb_id, instr_t *insn,
			kptr_t insnAddr,
			bool delaySlotInSameBlock,
			instr_t *delayInsn);


   void prepareSimpleJumpTable32(bbid_t bb_id,
				 instr_t *jmpInsn,
				 kptr_t jmpInsnAddr,
				 bool delaySlotInThisBlock,
				 instr_t *dsInsn,
				 kptr_t jumpTableDataStartAddr,
				 const simplePathSlice_t &settingJumpTableStartAddrSlice);

   void processNonLoadingBackwardsJumpTable(bbid_t, kptr_t, kptr_t,
					    instr_t *, bool, instr_t *,
					    const simplePathSlice_t &,
					    const simplePath &);

   void processOffsetJumpTable32WithTentativeBounds(
	       bbid_t bb_id,
	       instr_t *instr,
	       kptr_t jmpInsnAddr,
	       bool delaySlotInThisBlock,
	       instr_t *delaySlotInsn,
	       kptr_t jumpTableStartAddr,
	       const pdvector<kptr_t> &ijumpTableBlockAddrs,
	       const simplePathSlice_t &settingJumpTableStartAddrSlice,
	       const simplePath &pathFromEntryBB);


};

#endif

