// x86_funkshun.h 

#ifndef _X86_FUNKSHUN_
#define _X86_FUNKSHUN_

#include "funkshun.h"

class x86_function : public function_t {
 public:
  x86_function(XDR *xdr) : function_t(xdr) {}
  x86_function(kptr_t startAddr, const StaticString &fnName) :
    function_t(startAddr, fnName) {}
  x86_function(kptr_t startAddr, const pdvector<StaticString> &fnNames) :
    function_t(startAddr, fnNames) {}

  ~x86_function() {} // let ~function do the work

  kptr_t getBBInterProcBranchDestAddrIfAny(bbid_t bbid) const;
  pdvector<kptr_t> getCurrExitPoints() const;

  void getMemUsage(unsigned &forBasicBlocks, unsigned &forFns_OrigCode,
		   unsigned &forLiveRegAnalysisInfo, unsigned &num1Levels,
		   unsigned &num2Levels, unsigned &numOtherLevels) const;

#ifdef _KERNINSTD_
  void parse(bbid_t bb_id,
	     const simplePath &pathFromEntryBB,
	     parseOracle *theParseOracle) const;

  void parseCFG(const fnCode_t &iCode,
		// Above param will be used to initialize, en masse, 
		// the fn's code. After parsing has completed, unused 
		// parts of the code will be trimmed, so it's only 
		// important to pass *at least* enough bytes to cover 
		// the entire function.
		// NOTE: no need to say which of "iCode"'s chunks are 
		// the entry chunk; we can figure that out from 
		// this->entryAddr, which has already been set.
		const pdvector<unsigned> &chunkNdxsNotToTrim,
		// e.g. for jump table data
		dictionary_hash<kptr_t,bool> &interProcJumps,
		pdvector<hiddenFn> &,
		bbParsingStats &);
#endif
};

#endif
