// bbParsingStats.h

#ifndef _BB_PARSING_STATS_H_
#define _BB_PARSING_STATS_H_

struct bbParsingStats {
   unsigned numStumblesOntoExistingBlock;
   unsigned numInterProceduralStumbles;
      // except for numCondBranchFallThrusThatDoInterProceduralStumble cases

   unsigned numRetOrRetl;
   unsigned numV9Returns;
   unsigned numDoneOrRetry;

   unsigned numIntraProcBranchAlways;
   unsigned numIntraProcBranchNevers;
   unsigned numIntraProcCondBranches;
   unsigned numIntraProcCondBranchesWithAnnulBit;
   unsigned numIntraProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay;

   unsigned numInterProcBranchAlways;
   unsigned numInterProcCondBranches;
   unsigned numInterProcCondBranchesWithAnnulBit;
   unsigned numInterProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay;

   unsigned numCondBranchFallThrusThatDoInterProceduralStumble;

   unsigned numJumpToRegFixedAddr_IntraProc;
   unsigned numJumpToRegFixedAddr_InterProc;

   unsigned numJmpBehavingAsRetl;

   unsigned numTrueCalls_notTail;
   unsigned numJmplCalls_notTail;

   unsigned numTailCalls_TrueCallRestore;
   unsigned numTailCalls_JmplRestore;
   unsigned numTailCalls_trueCallThatWritesToO7InDelay;
   unsigned numTailCalls_jmplThatWritesToO7InDelay;

   unsigned numCallsJustToObtainPC;
   unsigned numCallRecursion;
   unsigned numHiddenIntraProcFnCalls;

   unsigned numSimpleJumpTables;
   unsigned numSimpleJumpTableTotalNonDupEntries;
   unsigned numTaggedJumpTables;
   unsigned numTaggedJumpTableTotalNonDupEntries;
   unsigned numNonLoadingBackwardsJumpTables;
   unsigned numNonLoadingBackwardsJumpTableTotalNonDupEntries;
   unsigned numOffsetJumpTables;
   unsigned numOffsetJumpTableTotalNonDupEntries;
   
   bbParsingStats() {
      numStumblesOntoExistingBlock = 0;
      numInterProceduralStumbles = 0;

      numRetOrRetl = 0;
      numV9Returns = 0;
      numDoneOrRetry = 0;
      
      numIntraProcBranchAlways = 0;
      numIntraProcBranchNevers = 0;
      numIntraProcCondBranches = 0;
      numIntraProcCondBranchesWithAnnulBit = 0;
      numIntraProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay = 0;

      numInterProcBranchAlways = 0;
      numInterProcCondBranches = 0;
      numInterProcCondBranchesWithAnnulBit = 0;
      numInterProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay = 0;

      numCondBranchFallThrusThatDoInterProceduralStumble = 0;

      numJumpToRegFixedAddr_IntraProc = 0;
      numJumpToRegFixedAddr_InterProc = 0;

      numJmpBehavingAsRetl = 0;

      numTrueCalls_notTail = 0;
      numJmplCalls_notTail = 0;
      
      numTailCalls_TrueCallRestore = 0;
      numTailCalls_JmplRestore = 0;
      numTailCalls_trueCallThatWritesToO7InDelay = 0;
      numTailCalls_jmplThatWritesToO7InDelay = 0;

      numCallsJustToObtainPC = 0;
      numCallRecursion = 0;
      numHiddenIntraProcFnCalls = 0;

      numSimpleJumpTables = 0;
      numSimpleJumpTableTotalNonDupEntries = 0;
      numTaggedJumpTables = 0;
      numTaggedJumpTableTotalNonDupEntries = 0;
      numNonLoadingBackwardsJumpTables = 0;
      numNonLoadingBackwardsJumpTableTotalNonDupEntries = 0;
      numOffsetJumpTables = 0;
      numOffsetJumpTableTotalNonDupEntries = 0;
   }

   void operator+=(const bbParsingStats &other) {
      numStumblesOntoExistingBlock += other.numStumblesOntoExistingBlock;
      numInterProceduralStumbles += other.numInterProceduralStumbles;

      numRetOrRetl += other.numRetOrRetl;
      numV9Returns += other.numV9Returns;
      numDoneOrRetry += other.numDoneOrRetry;

      numIntraProcBranchAlways += other.numIntraProcBranchAlways;
      numIntraProcBranchNevers += other.numIntraProcBranchNevers;
      numIntraProcCondBranches += other.numIntraProcCondBranches;
      numIntraProcCondBranchesWithAnnulBit +=
         other.numIntraProcCondBranchesWithAnnulBit;
      numIntraProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay +=
            other.numIntraProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay;
      
      numInterProcBranchAlways += other.numInterProcBranchAlways;
      numInterProcCondBranches += other.numInterProcCondBranches;
      numInterProcCondBranchesWithAnnulBit +=
         other.numInterProcCondBranchesWithAnnulBit;
      numInterProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay +=
         other.numInterProcCondBranchesWithAnnulBitAndSaveOrRestoreInDelay;

      numCondBranchFallThrusThatDoInterProceduralStumble +=
         other.numCondBranchFallThrusThatDoInterProceduralStumble;

      numJumpToRegFixedAddr_IntraProc += other.numJumpToRegFixedAddr_IntraProc;
      numJumpToRegFixedAddr_InterProc += other.numJumpToRegFixedAddr_InterProc;

      numJmpBehavingAsRetl += other.numJmpBehavingAsRetl;

      numTrueCalls_notTail += other.numTrueCalls_notTail;
      numJmplCalls_notTail += other.numJmplCalls_notTail;
      
      numTailCalls_TrueCallRestore += other.numTailCalls_TrueCallRestore;
      numTailCalls_JmplRestore += other.numTailCalls_JmplRestore;
      numTailCalls_trueCallThatWritesToO7InDelay +=
         other.numTailCalls_trueCallThatWritesToO7InDelay;
      numTailCalls_jmplThatWritesToO7InDelay +=
         other.numTailCalls_jmplThatWritesToO7InDelay;

      numCallsJustToObtainPC += other.numCallsJustToObtainPC;
      numCallRecursion += other.numCallRecursion;
      numHiddenIntraProcFnCalls += other.numHiddenIntraProcFnCalls;

      numSimpleJumpTables += other.numSimpleJumpTables;
      numSimpleJumpTableTotalNonDupEntries +=
         other.numSimpleJumpTableTotalNonDupEntries;
      numTaggedJumpTables += other.numTaggedJumpTables;
      numTaggedJumpTableTotalNonDupEntries +=
         other.numTaggedJumpTableTotalNonDupEntries;
      numNonLoadingBackwardsJumpTables += other.numNonLoadingBackwardsJumpTables;
      numNonLoadingBackwardsJumpTableTotalNonDupEntries +=
         other.numNonLoadingBackwardsJumpTableTotalNonDupEntries;
      numOffsetJumpTables += other.numOffsetJumpTables;
      numOffsetJumpTableTotalNonDupEntries +=
         other.numOffsetJumpTableTotalNonDupEntries;
   }
};

#endif
