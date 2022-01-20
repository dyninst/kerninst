// cfgCreationOracle.h

#ifndef _CFG_CREATION_ORACLE_H_
#define _CFG_CREATION_ORACLE_H_

#include "util/h/hashFns.h"
#include "instr.h"
#include "funkshun.h"
#include "hiddenFns.h"
#include "kmem.h"
#include "simplePathSlice.h"
#include "bbParsingStats.h"
#include "out_streams.h"
#include "parseOracle.h"

typedef function_t::bbid_t bbid_t;

class jumpTableMetaData {
 private:
   virtual void shrinkJumpTableSizeIfAppropriate(unsigned maxNumBytes) = 0;
      
 protected:
   function_t &fn;
   kptr_t jumpTableStartAddr; // address where data starts

 public:
   jumpTableMetaData(function_t &ifn, kptr_t ijumpTableStartAddr) :
     fn(ifn), jumpTableStartAddr(ijumpTableStartAddr) {
   }
   virtual jumpTableMetaData *dup() const = 0;
   virtual ~jumpTableMetaData() {}
   virtual void maybeAdjustJumpTableSize(kptr_t otherJumpTableStartAddr) {
      if (otherJumpTableStartAddr == jumpTableStartAddr)
	 assert(false); // unexpected
      else if (otherJumpTableStartAddr > jumpTableStartAddr)
	 shrinkJumpTableSizeIfAppropriate(otherJumpTableStartAddr - jumpTableStartAddr);
   }     
};

class offsetJumpTable32MetaData : public jumpTableMetaData {
 private:
   bbid_t bb_id; // containing the jmp instruction
   pdvector<bbid_t> jumpTableBlockIds;
      
   void shrinkJumpTableSizeIfAppropriate(unsigned maxNumBytes) {
      const unsigned numBytesPerJumpTableEntry = 4;
      const unsigned currSize = jumpTableBlockIds.size() * numBytesPerJumpTableEntry;
         
      if (maxNumBytes < currSize) {
	 dout << "cfg creation: shrinking offsetJumpTable32 from "
	      << currSize << " to " << maxNumBytes << " bytes!" << endl;
	 const unsigned newNumEntries = maxNumBytes / numBytesPerJumpTableEntry;

	 jumpTableBlockIds.shrink(newNumEntries);

	 fn.recreateJumpTableEdgesForFewerEntries(bb_id, jumpTableBlockIds);
      }
   }

   jumpTableMetaData *dup() const { return new offsetJumpTable32MetaData(*this); }

   offsetJumpTable32MetaData(const offsetJumpTable32MetaData &src) :
      jumpTableMetaData(src),
      bb_id(src.bb_id),
      jumpTableBlockIds(src.jumpTableBlockIds) {
   }
   offsetJumpTable32MetaData &operator=(const offsetJumpTable32MetaData &);
      
 public:
   offsetJumpTable32MetaData(function_t &ifn, kptr_t ijumpTableStartAddr,
			     bbid_t ibb_id,
			     const pdvector<bbid_t> &ijumpTableBlockIds) :
         jumpTableMetaData(ifn, ijumpTableStartAddr),
         bb_id(ibb_id), jumpTableBlockIds(ijumpTableBlockIds) {
   }
   ~offsetJumpTable32MetaData() {}
};

class cfgCreationOracle : public parseOracle {
 protected:
   dictionary_hash<kptr_t, bool> &interProcJumpsAndCalls;
   pdvector<hiddenFn> &hiddenFnsToParse;
   const kmem *km;
   bool verbose, verbose_hiddenFns;
   bbParsingStats &theBBParsingStats;

   pdvector<jumpTableMetaData*> allJumpTablesMetaData;
   void adjustForNewJumpTableStartAddr(kptr_t jumpTableStartAddr);

 public:
   cfgCreationOracle(function_t &ifn,
                     dictionary_hash<kptr_t, bool> &iinterProcJumpsAndCalls,
                     pdvector<hiddenFn> &ihiddenFnsToParse,
                     const kmem *ikm,
                     bool iverbose, bool iverbose_hiddenFns,
                     bbParsingStats &iBBParsingStats);
   virtual ~cfgCreationOracle();

   static cfgCreationOracle*
   getCfgCreationOracle(function_t &ifn,
			dictionary_hash<kptr_t, bool> &iinterProcJumpsAndCalls,
			pdvector<hiddenFn> &ihiddenFnsToParse,
			const kmem *ikm,
			bool iverbose, bool iverbose_hiddenFns,
			bbParsingStats &iBBParsingStats);

   bool hasCFGBeenFullyParsedYet() const {
      return false;
   }
   
   void ensureBlockStartInsnIsAlsoBlockOnlyInsn(kptr_t addr,
						instr_t *insn) const;
   void processInterproceduralFallThru(bbid_t bb_id);
   void processStumbleOntoExistingBlock(bbid_t bb_id,
					bbid_t stumble_bb_id,
					const simplePath &/*thePath*/);
   void processSeveralNonControlFlowInsnsAsInsnVecSubset(
            bbid_t bb_id,
	    kptr_t currNonControlFlowCodeChunkStartAddr,
	    unsigned currNonControlFlowCodeChunkNumInsns) const;   
   void processCallToRoutineReturningOnCallersBehalf(bbid_t bb_id,
						     instr_t *i,
						     kptr_t /*instrAddr*/,
						     kptr_t calleeAddr,
						     bool delaySlotInSameBlock,
						     instr_t *delayInsn);
   void processCallToObtainPC(bbid_t bb_id, instr_t *callInsn,
			      kptr_t /*callInsnAddr*/,
			      bool delaySlotInSameBlock,
			      instr_t *delaySlotInsn);

   void noteHiddenIntraProceduralFn(kptr_t calleeAddr);

   void processReturn(bbid_t bb_id, instr_t *insn,
		      kptr_t /*insnAddr*/,
		      bool delaySlotInSameBlock,
		      instr_t *dsInsn);
   void processJmpBehavingAsRet(bbid_t bb_id, instr_t *insn,
				kptr_t insnAddr,
				bool delaySlotInSameBlock,
				instr_t *dsInsn);
   void processDoneOrRetry(bbid_t bb_id, instr_t *insn,
			   kptr_t insnAddr);
   void processTrueCallRestore(bbid_t bb_id, instr_t *callInstr,
			       kptr_t instrAddr,
			       kptr_t destAddr,
			       bool nonTrivialRestore,
			       bool dangerRestore,
			       const reg_t *regWrittenByRestore,
			       bool delaySlotInSameBlock,
			       instr_t *dsInsn);
   void processJmplCallRestore(bbid_t bb_id, instr_t *callInstr,
			       kptr_t instrAddr,
			       bool nonTrivialRestore,
			       bool dangerousRestore,
			       const reg_t *rdOfRestore,
			       bool delaySlotInSameBlock,
			       instr_t *dsInsn);
   void processJmplRestoreOptimizedTailCall(bbid_t bb_id,
					    instr_t *instr,
					    kptr_t /*insnAddr*/,
					    bool delaySlotInSameBlock,
					    instr_t *dsInsn) const;

   void prepareSimpleJumpTable32(bbid_t bb_id,
				 instr_t *jmpInsn,
				 kptr_t /*jmpInsnAddr*/,
				 bool delaySlotInThisBlock,
				 instr_t *delaySlotInsn,
				 kptr_t jumpTableDataStartAddr,
				 const simplePathSlice_t &);
   void process1SimpleJumpTable32Entry(bbid_t bb_id,
				       kptr_t nextJumpTableEntry,
				       const simplePath &pathFromEntryBB);
   void completeSimpleJumpTable32(bbid_t bb_id, 
				  kptr_t jumpTableStartAddr);
   void prepareTaggedJumpTable32(bbid_t bb_id,
				 kptr_t jumpTableStartAddr,
				 instr_t *jmpInsn,
				 bool delaySlotInThisBlock,
				 instr_t *delaySlotInsn) const;
   void process1TaggedJumpTable32Entry(bbid_t bb_id,
				       uint32_t /*theTag*/,
				       kptr_t theDestAddr,
				       const simplePath &pathFromEntryBB);
   void completeTaggedJumpTable32(bbid_t bb_id, 
				  kptr_t jumpTableStartAddr);
};

#endif
