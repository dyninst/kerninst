#ifndef _X86_CONTROLFLOW_H_
#define _X86_CONTROLFLOW_H_

#include <inttypes.h>

class function_t; // avoid recursive #include
class kmem;

//#include "x86Traits.h"
#include "x86_instr.h"

#include "common/h/Dictionary.h"
#include "common/h/Vector.h"

typedef uint16_t bbid_t;

class x86_controlFlow
{
 private:
  
   static bool handleCallInstr(bbid_t bb_id,
			       instr_t *callInstr,
			       kptr_t &instrAddr,
                                  // If returning true, modify this
                               x86_instr::x86_cfi *cfi,
			       const simplePath & /*pathFromEntryBB*/,
			       kptr_t stumbleOntoAddr,
                                  // next known bb startaddr, or (kptr_t)-1
			       const kmem *km,
			       parseOracle *theParseOracle);

   static bool handleConditionalBranchInstr(unsigned bb_id,
					    x86_instr *instr,
					    kptr_t &instrAddr,
					    x86_instr::x86_cfi *cfi,
					    const simplePath &pathFromEntryBB,
					    kptr_t stumbleOntoAddr,
					    const kmem *km,
					    parseOracle *theParseOracle);

   static bool handleJmpInstr(unsigned bb_id,
			      x86_instr *instr,
			      kptr_t &instrAddr,
			      x86_instr::x86_cfi *cfi,
			      const simplePath &pathFromEntryBB,
			      kptr_t stumbleOntoAddr,
			      const kmem *km,
			      parseOracle *theParseOracle);

   static bool handleRetInstr(unsigned bb_id,
			      x86_instr *instr,
			      kptr_t &instrAddr,
			      parseOracle *theParseOracle);

   static bool handleIntInstr(unsigned bb_id,
			      x86_instr *instr,
			      kptr_t &instrAddr,
			      parseOracle *theParseOracle);
 public:   
   static bool handleControlFlowInstr(bbid_t bb_id,
				      x86_instr *instr,
				      kptr_t &instrAddr,
				        // iff returning true, we modify this
				      x86_instr::x86_cfi *cfi,
				      const simplePath &pathFromEntryBB,
				        // doesn't yet include curr bb
				      kptr_t stumbleOntoAddr,
				      const kmem *km,
				      parseOracle *theParseOracle);
};

#endif /* _X86_CONTROLFLOW_H_ */
