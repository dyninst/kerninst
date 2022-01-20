// power_cfgCreationOracle.h

#ifndef _POWER_CFG_CREATION_ORACLE_H_
#define _POWER_CFG_CREATION_ORACLE_H_

#include "cfgCreationOracle.h"
#include "powerTraits.h"
typedef function_t::bbid_t bbid_t;

class powerJumpTableMetaData : public jumpTableMetaData {
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
   
   jumpTableMetaData *dup() const { return new powerJumpTableMetaData(*this); }

   powerJumpTableMetaData(const powerJumpTableMetaData &src) :
      jumpTableMetaData(src),
      bb_id(src.bb_id),
      jumpTableBlockIds(src.jumpTableBlockIds) {
   }
   powerJumpTableMetaData &operator=(const powerJumpTableMetaData &);

   void maybeAdjustJumpTableSize(kptr_t otherJumpTableStartAddr) {
      if (otherJumpTableStartAddr == jumpTableStartAddr)
         assert(false); // unexpected
      else if (otherJumpTableStartAddr > jumpTableStartAddr)
         shrinkJumpTableSizeIfAppropriate(otherJumpTableStartAddr - jumpTableStartAddr);
      else // (jumpTableStartAddr > otherJumpTableStartAddr)
          shrinkJumpTableSizeIfAppropriate(jumpTableStartAddr - otherJumpTableStartAddr);
   }     
   
 public:
   powerJumpTableMetaData(function_t &ifn, kptr_t ijumpTableStartAddr,
			     bbid_t ibb_id,
			     const pdvector<bbid_t> &ijumpTableBlockIds) :
         jumpTableMetaData(ifn, ijumpTableStartAddr),
         bb_id(ibb_id), jumpTableBlockIds(ijumpTableBlockIds) {
   }
   ~powerJumpTableMetaData() {}
};

class power_cfgCreationOracle : public cfgCreationOracle {
 private: 

 public:
   power_cfgCreationOracle(function_t &ifn,
			   dictionary_hash<kptr_t, bool> &iinterProcJumpsAndCalls,
			   pdvector<hiddenFn> &ihiddenFnsToParse,
			   const kmem *ikm,
			   bool iverbose, bool iverbose_hiddenFns,
			   bbParsingStats &iBBParsingStats) :
     cfgCreationOracle(ifn, iinterProcJumpsAndCalls, ihiddenFnsToParse,
		       ikm, iverbose, iverbose_hiddenFns, 
		       iBBParsingStats) {};
   
   power_cfgCreationOracle(function_t &ifn,
			   dictionary_hash<kptr_t, bool> &iinterProcJumpsAndCalls,
			   pdvector<hiddenFn> &ihiddenFnsToParse,
			   const kmem *ikm,
			   bool iverbose, bool iverbose_hiddenFns,
			   bbParsingStats &iBBParsingStats, kptr_t tocptr) :
     cfgCreationOracle(ifn, iinterProcJumpsAndCalls, ihiddenFnsToParse,
		       ikm, iverbose, iverbose_hiddenFns, 
		       iBBParsingStats)  {};
   


   ~power_cfgCreationOracle() {}

    void processCallRecursion(bbid_t bb_id, kptr_t calleeAddr,
			      instr_t *callInsr,
			      kptr_t instrAddr,
               bool delaySlotInSameBlock,
			      instr_t *dsInsn,
			      const simplePath & pathFromEntryBB
			      // doesn't include this block yet
			      );
      // a recursive call.

    void preparePowerJumpTable(bbid_t bb_id,
                               const instr_t *jmpInsn,
                               kptr_t /*jmpInsnAddr*/,
                               kptr_t jumpTableDataStartAddr
                               );

    void processTruePowerCall(bbid_t bb_id, const instr_t *callInstr,
                         kptr_t /*instrAddr*/,
                         kptr_t destAddr
                         );
    

    void processCondPowerCall(bbid_t bb_id, const instr_t *callInstr,
                              kptr_t instrAddr ,
                              kptr_t destAddr,
                              const simplePath &pathFromEntryBB
                              ); 
    
    void processCondReturn(bbid_t bb_id, const instr_t *callInstr,
                           kptr_t instrAddr,
                           const simplePath &pathFromEntryBB
                           );
    void processIndirectTailCall(bbid_t bb_id,  instr_t *jmpInsn,
                                 kptr_t /*insnAddr*/,
                                 bool /* delaySlotInSameBlock */,
                                 instr_t  * /* dsInsn */) const;

    void processInterproceduralBranch(bbid_t bb_id, kptr_t destAddr,
				      instr_t *i,
				      const controlFlowInfo_t &cfi,
                  bool /*branchAlways*/,                               
				      kptr_t instrAddr,
                  bool /* delaySlotInSameBlock */,
                  instr_t * /* dsInsn */);
   

    void processCondBranch(bbid_t bb_id,
			   bool branchAlways, bool branchNever,
			   instr_t *i,
			    controlFlowInfo_t *cfi,
			   kptr_t instrAddr,
            bool /* hasDelaySlotInsn */,
            bool /* delaySlotInThisBlock */,
            instr_t  * /*dsInsn */,
			   kptr_t fallThroughAddr,
			   kptr_t destAddr,
			   const simplePath &pathFromEntryBB);
   
   void 
   processIntraproceduralJmpToFixedAddr(bbid_t bb_id,
					instr_t *instr,
					kptr_t jmpInsnAddr,
               bool /* delaySlotInSameBlock */,                  
               instr_t * /*dsInsn*/,
               kptr_t  destAddr,
               const simplePathSlice_t  &/*jmpRegSlice*/ ,
					const simplePath &pathFromEntryBB);
   
   void 
   processInterproceduralJmpToFixedAddr(bbid_t bb_id,
					instr_t *instr,
	            bool /* delaySlotInSameBlock */,
               instr_t * /* dsInsn */,
					kptr_t destAddr,
					const simplePath &pathFromEntryBB);
   


   /**** Methods from here to end are not applicable to POWER ************/

   void 
      processNonTailOptimizedTrueCall(bbid_t /* bb_id */, 
                                   instr_t */* callInstr */,
                                      kptr_t /* instrAddr */,
                                      kptr_t /* destAddr */,
                                      bool /* delaySlotInSameBlock */,
                                      instr_t * /* delaySlotInsn */
                                   ) {
      assert(false);
   }
   
   void
      processNonTailOptimizedJmpCall(bbid_t /* bb_id */, 
                                  instr_t */* callInstr */,
                                  kptr_t /* instrAddr */,
                                  const bool /* delaySlotInSameBlock */,
                                  instr_t  * /* delaySlotInsn */
                                  ) {
      assert(false);
   }
  

   // ---------- Non-Loading Backwards Jump Table: ----------
   void processNonLoadingBackwardsJumpTable(bbid_t bb_id,
					    kptr_t jumpTableStartAddr,
					    kptr_t jumpTableEndAddr,
					    instr_t *jmpInsn,
                   bool /* delaySlotInThisBlock */,
				       instr_t * /* delaySlotInsn */,
					    const simplePathSlice_t &justPastEndOfJmpTableSlice,
					    const simplePath &);

   // ---------- Offset Jump Table 32: ----------
   void processOffsetJumpTable32WithTentativeBounds
                                 (bbid_t bb_id,
				  instr_t *instr,
				  kptr_t jmpInsnAddr,
              bool /* delaySlotInThisBlock */,
              instr_t * /* delaySlotInsn */,
				  kptr_t jumpTableStartAddr,
				  const pdvector<kptr_t> &,
				  // jump table block addrs
				  // tentative!! May be too many!!!
				  const simplePathSlice_t &theSlice,
				  const simplePath &pathFromEntryBB);

   void processTrueCallRestore(bbid_t /* bb_id */, instr_t */* callInstr */,
                               kptr_t /* instrAddr */,
                               kptr_t /* destAddr */,
                               bool /* nonTrivialRestore */,
                               bool /* dangerousRestore */,
                               const reg_t * /* regWrittenByRestore */,
                               bool /* delaySlotInSameBlock */,
                               instr_t */* dsInsn */) {
      assert(false);
   }
   void processJmpCallRestore(bbid_t /* bb_id */, instr_t * /* callInstr */,
                              kptr_t /* instrAddr */,
                              bool /* nonTrivialRestore */,
                              bool /* dangerousRestore */,
                              const reg_t * /* rdOfRestore */,
                              bool /* delaySlotInSameBlock */,
                              instr_t */* dsInsn */) {
      assert(false);
   }
   void processNonTailOptimizedJmplCall(bbid_t /* bb_id */,
                                        instr_t * /* callInstr */,
                                        kptr_t /*  instrAddr */,
                                        bool /* delaySlotInSameBlock */,
                                        instr_t * /* delaySlotInsn */
                                        ) {
      assert(false);
   }
};

#endif


